#include "lazo_control.h"
#include "config.h"
#include "motor.h"
#include "imu.h"
#include "controlador_pid.h"
#include "controlador_pi.h"
#include "calibracion.h"
#include "boton.h"
#include "almacenamiento.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

// Cada cuántos ciclos de TS_TICK_MS se ejecuta cada parte
const uint32_t CICLOS_MPU    = TS_BASE_MS / TS_TICK_MS;
const uint32_t CICLOS_KALMAN = TS_KALMAN_MS / TS_TICK_MS;
const uint32_t CICLOS_MOTOR  = TS_MS / TS_TICK_MS;
const uint32_t CICLOS_ANGULO = TS_ANGULO_MS / TS_TICK_MS;
const uint32_t CICLOS_GIRO   = TS_GIRO_MS / TS_TICK_MS;
const uint32_t CICLOS_POSICION = TS_POSICION_MS / TS_TICK_MS;

// rpm -> m/s con el perímetro de la rueda
const float MS_POR_RPM = PI * DIAMETRO_RUEDA / 60.0f;

//--------------------------------------------------
// Datos compartidos con el núcleo 0 (protegidos por el candado)
//--------------------------------------------------
static portMUX_TYPE candado = portMUX_INITIALIZER_UNLOCKED;
static bool habilitado = false;                 // escrito por la web
static ParametrosControl parametros;            // escrito por la web
static bool parametrosNuevos = false;
static float vBateriaPedido = 0;                // escrito por la web
static bool vBateriaNuevo = false;
static EstadoControl estadoPublico;             // escrito por el control

static QueueHandle_t colaMuestras = NULL;
static volatile uint32_t sobrepasos = 0;

//--------------------------------------------------
// Estado propio de la tarea de control (solo núcleo 1)
//--------------------------------------------------
static ControladorPID controlAngulo;
static ControladorPIPosicional controlGiro;
static ControladorPID controlPosicion;
static float ajusteAngulo = AJUSTE_ANGULO;  // se suma a la medida del sensor (°)
static float posicionRef = POSICION_REF;    // set point de desplazamiento (m)
static float anguloRef = 0;          // set point de ángulo = salida del desplazamiento (°)
static long cuentasOrigen = 0;       // cuentas1 + cuentas2 al empezar a equilibrar

static bool imuLista = false;      // false si el MPU no respondió al iniciar
static bool activo = false;        // true = controlando
static MotivoParo motivo = PARO_APAGADO;
static int fallasMPU = 0;
static float rpmAngulo = 0;        // salida del controlador de ángulo
static float rpmGiro = 0;          // salida del controlador de giro

static void detenerControladores()
{
  activo = false;
  rpmAngulo = 0;
  rpmGiro = 0;
  anguloRef = 0;
  reiniciarPIPosicional(controlGiro);
  reiniciarPID(controlAngulo);
  reiniciarPID(controlPosicion);
}

// Ángulo del sensor con el ajuste aplicado (0° = equilibrio)
static float anguloAjustado()
{
  return anguloInclinacion() + ajusteAngulo;
}

// Suma de las cuentas con signo de ambos encoders (positiva al avanzar)
static long sumaCuentas()
{
  return cuentasConSigno(motor1) + cuentasConSigno(motor2);
}

// Al empezar a equilibrar: el punto actual pasa a ser la posición a mantener
static void activarControl()
{
  activo = true;
  cuentasOrigen = sumaCuentas();
  tomarVelocidadGiro();  // descarta lo acumulado mientras estaba detenido
}

// Copia los parámetros pedidos desde la web (sin borrar el integrador del giro)
static void aplicarParametrosNuevos()
{
  ParametrosControl p;
  bool hay = false;

  portENTER_CRITICAL(&candado);
  if (vBateriaNuevo) {
    V_BATERIA = vBateriaPedido;  // único lugar donde cambia en ejecución
    vBateriaNuevo = false;
  }
  if (parametrosNuevos) {
    p = parametros;
    parametrosNuevos = false;
    hay = true;
  }
  portEXIT_CRITICAL(&candado);
  if (!hay) return;

  controlAngulo.kp = p.kpAngulo;
  controlAngulo.ki = p.kiAngulo;
  controlAngulo.kd = p.kdAngulo;
  ajusteAngulo = p.ajusteAngulo;
  posicionRef = p.posicionRef;
  controlGiro.kp = p.kpGiro;
  controlGiro.ki = p.kiGiro;
  controlPosicion.kp = p.kpPosicion;
  controlPosicion.ki = p.kiPosicion;
  controlPosicion.kd = p.kdPosicion;
}

static bool leerHabilitado()
{
  portENTER_CRITICAL(&candado);
  bool hab = habilitado;
  portEXIT_CRITICAL(&candado);
  return hab;
}

// Rutina del botón de calibración. Al terminar con éxito el ajuste del
// sensor pasa a ser -(ángulo de equilibrio), así ese punto se lee como 0°
// (también se actualiza en la web).
// true si se escribió en la flash (el ciclo tardó más de lo normal).
static bool atenderCalibracion(bool lecturaOk)
{
  float anguloEquilibrio;
  if (!pasoCalibracion(botonPulsado(), leerHabilitado(), lecturaOk, anguloEquilibrio)) return false;

  ajusteAngulo = -anguloEquilibrio;
  portENTER_CRITICAL(&candado);
  parametros.ajusteAngulo = ajusteAngulo;
  portEXIT_CRITICAL(&candado);
  return true;
}

// Decide si se puede controlar y por qué no
static void supervisarSeguridad(bool lecturaOk)
{
  bool cal = calibrando();
  if (cal) {
    // Los motores quedan apagados durante y después de la calibración
    portENTER_CRITICAL(&candado);
    habilitado = false;
    portEXIT_CRITICAL(&candado);
  }
  bool hab = leerHabilitado();

  fallasMPU = lecturaOk ? 0 : fallasMPU + 1;
  bool mpuOk = imuLista && fallasMPU < MPU_FALLAS_MAX;
  float desvio = fabsf(anguloAjustado());
  bool enLimites = mpuOk && desvio <= ANGULO_CAIDA;

  if (cal || !hab || !mpuOk) {
    if (activo) detenerControladores();
    motivo = cal ? PARO_CALIBRANDO : (!hab ? PARO_APAGADO : PARO_FALLA_MPU);
  } else if (activo) {
    if (!enLimites) {
      detenerControladores();
      motivo = PARO_CAIDA;
    }
  } else if (desvio < ANGULO_REARME) {
    activarControl();
    motivo = PARO_NINGUNO;
  } else if (motivo != PARO_CAIDA) {
    motivo = PARO_ESPERA_VERTICAL;  // el aviso de caída se mantiene hasta el rearme
  }

  portENTER_CRITICAL(&candado);
  estadoPublico.habilitado = hab;
  estadoPublico.mpuOk = mpuOk;
  estadoPublico.anguloEnLimites = enLimites;
  estadoPublico.motivo = motivo;
  portEXIT_CRITICAL(&candado);
}

static void lazoGiro()
{
  float e = VEL_GIRO_REF - tomarVelocidadGiro();
  rpmGiro = calcularPIPosicional(controlGiro, e);
}

// Desplazamiento x (m) desde el origen y velocidad lineal v (m/s).
// La salida es el set point del controlador de ángulo (±ANGULO_MAX_POSICION):
// si el carrito pasó la referencia hacia adelante, pide inclinarse hacia
// atrás y el carrito regresa.
static void lazoPosicion()
{
  float x = (float)(sumaCuentas() - cuentasOrigen) * 0.5f * M_X_CUENTA;
  float v = (motor1.rpm + motor2.rpm) * 0.5f * MS_POR_RPM;
  anguloRef = calcularPID(controlPosicion, posicionRef - x, 0.0f - v);
}

static void lazoAngulo()
{
  float e = anguloAjustado() - anguloRef;
  rpmAngulo = calcularPID(controlAngulo, e, velocidadCaida());
}

static void lazoMotores()
{
  medirRPM(motor1);
  medirRPM(motor2);

  bool reiniciar = !activo;
  controlarVelocidad(motor1, rpmAngulo + rpmGiro, reiniciar);
  controlarVelocidad(motor2, rpmAngulo - rpmGiro, reiniciar);
}

// Si la cola está llena se descarta la muestra más vieja (se conservan
// siempre los últimos segundos aunque nadie esté mirando la página)
static void guardarMuestra()
{
  MuestraControl m = { anguloRef, anguloAjustado(),
                       rpmAngulo + rpmGiro, motor1.rpm,
                       rpmAngulo - rpmGiro, motor2.rpm };
  if (xQueueSend(colaMuestras, &m, 0) != pdTRUE) {
    MuestraControl vieja;
    xQueueReceive(colaMuestras, &vieja, 0);
    xQueueSend(colaMuestras, &m, 0);
  }
}

// El periodo lo marca el tick de FreeRTOS (1 kHz) del propio núcleo 1, por
// lo que no depende del esp_timer ni de nada que corra en el núcleo 0.
static void tareaLazoControl(void *arg)
{
  const TickType_t periodo = pdMS_TO_TICKS(TS_TICK_MS);
  TickType_t ultimoDespertar = xTaskGetTickCount();
  uint32_t ciclo = 0;

  for (;;) {
    // pdFALSE: el ciclo anterior tardó más de TS_TICK_MS y no hubo espera
    if (xTaskDelayUntil(&ultimoDespertar, periodo) == pdFALSE) sobrepasos = sobrepasos + 1;

    bool tocaMPU = ciclo % CICLOS_MPU == 0;
    bool lecturaOk = false;

    // Cada TS_BASE_MS: lectura del MPU, botón y calibración
    if (tocaMPU) {
      aplicarParametrosNuevos();
      lecturaOk = imuLista && actualizarIMU();
      if (atenderCalibracion(lecturaOk)) {
        // La escritura en flash pausó la tarea: se retoma el periodo desde
        // ahora en vez de recuperar los ciclos perdidos de golpe
        ultimoDespertar = xTaskGetTickCount();
      }
    }

    // Cada TS_KALMAN_MS: estimación del ángulo con la última lectura
    if (imuLista && ciclo % CICLOS_KALMAN == 0) actualizarAnguloKalman();

    // La seguridad usa el ángulo ya actualizado
    if (tocaMPU) supervisarSeguridad(lecturaOk);

    // Primero los lazos externos, para que los motores usen la referencia recién calculada
    if (activo && ciclo % CICLOS_GIRO == 0)     lazoGiro();
    if (activo && ciclo % CICLOS_POSICION == 0) lazoPosicion();
    if (activo && ciclo % CICLOS_ANGULO == 0)   lazoAngulo();
    if (ciclo % CICLOS_MOTOR == 0)            lazoMotores();
    if (ciclo % CICLOS_ANGULO == 0)           guardarMuestra();

    // Múltiplo común de todos los periodos, para que el contador no rompa la secuencia
    ciclo = (ciclo + 1) % (CICLOS_MPU * CICLOS_KALMAN * CICLOS_MOTOR * CICLOS_ANGULO *
                           CICLOS_GIRO * CICLOS_POSICION);
  }
}

bool iniciarLazoControl()
{
  configurarPID(controlAngulo, KP_ANGULO, KI_ANGULO, KD_ANGULO, TS_ANGULO, RPM_MAX_ANGULO);
  configurarPIPosicional(controlGiro, KP_GIRO, KI_GIRO, TS_GIRO, RPM_MAX_GIRO);
  configurarPID(controlPosicion, KP_POSICION, KI_POSICION, KD_POSICION, TS_POSICION,
                ANGULO_MAX_POSICION);
  detenerControladores();

  CalibracionGuardada cal;
  bool hayCalibracion = cargarCalibracion(cal);
  if (hayCalibracion) {
    ajusteAngulo = -cal.anguloEquilibrio;
    Serial.printf("# Calibración cargada de la flash: equilibrio %.2f°\n", cal.anguloEquilibrio);
  } else {
    Serial.println("# Sin calibración guardada: calibrando giroscopio, mantener el carrito quieto...");
  }

  parametros = { KP_ANGULO, KI_ANGULO, KD_ANGULO, ajusteAngulo, KP_GIRO, KI_GIRO,
                 KP_POSICION, KI_POSICION, KD_POSICION, posicionRef };
  estadoPublico = { false, false, false, PARO_APAGADO };
  colaMuestras = xQueueCreate(TAMANO_COLA_MUESTRAS, sizeof(MuestraControl));
  iniciarBoton();

  // Se llama desde setup(), que corre en el núcleo 1: la interrupción del
  // I2C queda en el mismo núcleo que la tarea de control.
  imuLista = iniciarIMU(hayCalibracion ? cal.biasGiro : NULL);

  xTaskCreatePinnedToCore(tareaLazoControl, "lazo_control", PILA_CONTROL, NULL,
                          PRIORIDAD_CONTROL, NULL, NUCLEO_CONTROL);
  return imuLista;
}

void habilitarMotores(bool on)
{
  if (calibrando()) return;
  portENTER_CRITICAL(&candado);
  habilitado = on;
  portEXIT_CRITICAL(&candado);
}

EstadoControl obtenerEstado()
{
  portENTER_CRITICAL(&candado);
  EstadoControl e = estadoPublico;
  portEXIT_CRITICAL(&candado);
  return e;
}

ParametrosControl obtenerParametros()
{
  portENTER_CRITICAL(&candado);
  ParametrosControl p = parametros;
  portEXIT_CRITICAL(&candado);
  return p;
}

void fijarParametros(const ParametrosControl &p)
{
  portENTER_CRITICAL(&candado);
  parametros = p;
  parametrosNuevos = true;
  portEXIT_CRITICAL(&candado);
}

float obtenerVoltajeBateria()
{
  portENTER_CRITICAL(&candado);
  float v = vBateriaNuevo ? vBateriaPedido : V_BATERIA;
  portEXIT_CRITICAL(&candado);
  return v;
}

bool fijarVoltajeBateria(float v)
{
  if (!(v >= V_BATERIA_MIN && v <= V_BATERIA_MAX)) return false;
  portENTER_CRITICAL(&candado);
  vBateriaPedido = v;
  vBateriaNuevo = true;
  portEXIT_CRITICAL(&candado);
  return true;
}

bool obtenerMuestra(MuestraControl &m)
{
  return colaMuestras != NULL && xQueueReceive(colaMuestras, &m, 0) == pdTRUE;
}

uint32_t obtenerSobrepasos()
{
  return sobrepasos;
}
