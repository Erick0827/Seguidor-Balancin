#include "lazo_control.h"
#include "config.h"
#include "motor.h"
#include "imu.h"
#include "controlador_pd.h"
#include "controlador_pi.h"
#include "calibracion.h"
#include "boton.h"
#include "almacenamiento.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

// Cada cuántos ciclos base se ejecuta cada lazo
const uint32_t CICLOS_MOTOR  = TS_MS / TS_BASE_MS;
const uint32_t CICLOS_ANGULO = TS_ANGULO_MS / TS_BASE_MS;
const uint32_t CICLOS_GIRO   = TS_GIRO_MS / TS_BASE_MS;

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
static ControladorPD controlAngulo;
static ControladorPIPosicional controlGiro;
static float anguloRef = ANGULO_EQUILIBRIO;

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
  reiniciarPIPosicional(controlGiro);
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
  controlAngulo.kd = p.kdAngulo;
  anguloRef = p.anguloRef;
  controlGiro.kp = p.kpGiro;
  controlGiro.ki = p.kiGiro;
}

static bool leerHabilitado()
{
  portENTER_CRITICAL(&candado);
  bool hab = habilitado;
  portEXIT_CRITICAL(&candado);
  return hab;
}

// Rutina del botón de calibración. Al terminar con éxito el ángulo de
// equilibrio pasa a ser el set point de ángulo (también en la web).
// true si se escribió en la flash (el ciclo tardó más de lo normal).
static bool atenderCalibracion(bool lecturaOk)
{
  float anguloNuevo;
  if (!pasoCalibracion(botonPulsado(), leerHabilitado(), lecturaOk, anguloNuevo)) return false;

  anguloRef = anguloNuevo;
  portENTER_CRITICAL(&candado);
  parametros.anguloRef = anguloNuevo;
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
  float desvio = fabsf(anguloInclinacion() - anguloRef);
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
    activo = true;
    motivo = PARO_NINGUNO;
    tomarVelocidadGiro();  // descarta lo acumulado mientras estaba detenido
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

static void lazoAngulo()
{
  float e = anguloInclinacion() - anguloRef;
  rpmAngulo = calcularPD(controlAngulo, e, velocidadCaida());
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
  MuestraControl m = { anguloRef, anguloInclinacion(),
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
  const TickType_t periodo = pdMS_TO_TICKS(TS_BASE_MS);
  TickType_t ultimoDespertar = xTaskGetTickCount();
  uint32_t ciclo = 0;

  for (;;) {
    // pdFALSE: el ciclo anterior tardó más de TS_BASE_MS y no hubo espera
    if (xTaskDelayUntil(&ultimoDespertar, periodo) == pdFALSE) sobrepasos = sobrepasos + 1;

    aplicarParametrosNuevos();
    bool lecturaOk = imuLista && actualizarIMU();
    if (atenderCalibracion(lecturaOk)) {
      // La escritura en flash pausó la tarea: se retoma el periodo desde
      // ahora en vez de recuperar los ciclos perdidos de golpe
      ultimoDespertar = xTaskGetTickCount();
    }
    supervisarSeguridad(lecturaOk);

    // Primero los lazos externos, para que los motores usen la referencia recién calculada
    if (activo && ciclo % CICLOS_GIRO == 0)   lazoGiro();
    if (activo && ciclo % CICLOS_ANGULO == 0) lazoAngulo();
    if (ciclo % CICLOS_MOTOR == 0)            lazoMotores();
    if (ciclo % CICLOS_ANGULO == 0)           guardarMuestra();

    ciclo = (ciclo + 1) % (CICLOS_MOTOR * CICLOS_ANGULO * CICLOS_GIRO);
  }
}

bool iniciarLazoControl()
{
  configurarPD(controlAngulo, KP_ANGULO, KD_ANGULO, RPM_MAX_ANGULO);
  configurarPIPosicional(controlGiro, KP_GIRO, KI_GIRO, TS_GIRO, RPM_MAX_GIRO);
  detenerControladores();

  CalibracionGuardada cal;
  bool hayCalibracion = cargarCalibracion(cal);
  if (hayCalibracion) {
    anguloRef = cal.anguloEquilibrio;
    Serial.printf("# Calibración cargada de la flash: equilibrio %.2f°\n", anguloRef);
  } else {
    Serial.println("# Sin calibración guardada: calibrando giroscopio, mantener el carrito quieto...");
  }

  parametros = { KP_ANGULO, KD_ANGULO, anguloRef, KP_GIRO, KI_GIRO };
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
