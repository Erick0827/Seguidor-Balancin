/*
  config.h
  Parámetros generales del carrito: tiempos de muestreo, ganancias,
  voltajes, MPU, encoders, pines, núcleos y WiFi.
  Cualquier ajuste de configuración se hace aquí.
*/
#pragma once

#include <Arduino.h>

#if ESP_ARDUINO_VERSION_MAJOR < 3
#error "Este proyecto requiere el core ESP32 de Arduino 3.x (driver PCNT de ESP-IDF 5)."
#endif

//==================================================
// TIEMPOS DE MUESTREO
//==================================================
// La tarea de control despierta cada TS_TICK_MS y de ahí ejecuta cada
// tarea/lazo cuando le toca. Todos deben ser múltiplos de TS_TICK_MS.

const uint32_t TS_TICK_MS   = 1;    // Periodo de despertar de la tarea de control
const uint32_t TS_BASE_MS   = 2;    // Lectura del MPU, botón, calibración y seguridad (500 Hz)
const uint32_t TS_KALMAN_MS = 5;    // Filtro de Kalman del ángulo (200 Hz)
const uint32_t TS_MS        = 10;   // Lazos PI de velocidad de los motores
const uint32_t TS_ANGULO_MS = 20;   // Controlador de ángulo (PID)
const uint32_t TS_GIRO_MS   = 100;  // Controlador de giro (P / PI)
const uint32_t TS_POSICION_MS = 50;  // Controlador de desplazamiento (PID)

const float TS_BASE   = TS_BASE_MS / 1000.0f;
const float TS_KALMAN = TS_KALMAN_MS / 1000.0f;
const float TS        = TS_MS / 1000.0f;
const float TS_ANGULO = TS_ANGULO_MS / 1000.0f;
const float TS_GIRO   = TS_GIRO_MS / 1000.0f;
const float TS_POSICION = TS_POSICION_MS / 1000.0f;

static_assert(TS_BASE_MS % TS_TICK_MS == 0,   "TS_BASE_MS debe ser múltiplo de TS_TICK_MS");
static_assert(TS_KALMAN_MS % TS_TICK_MS == 0, "TS_KALMAN_MS debe ser múltiplo de TS_TICK_MS");
static_assert(TS_MS % TS_TICK_MS == 0,        "TS_MS debe ser múltiplo de TS_TICK_MS");
static_assert(TS_ANGULO_MS % TS_TICK_MS == 0, "TS_ANGULO_MS debe ser múltiplo de TS_TICK_MS");
static_assert(TS_GIRO_MS % TS_TICK_MS == 0,   "TS_GIRO_MS debe ser múltiplo de TS_TICK_MS");
static_assert(TS_POSICION_MS % TS_TICK_MS == 0, "TS_POSICION_MS debe ser múltiplo de TS_TICK_MS");

//==================================================
// CONTROLADORES PI DE VELOCIDAD (MOTORES)
//==================================================

// Ganancias PI de cada motor (Kp en V/rpm, Ki en V/(rpm*s))
const float KP_1 = 0.073037;
const float KI_1 = 2.1599;
const float KP_2 = 0.070481;
const float KI_2 = 2.0043;

//==================================================
// CONTROLADOR DE ANGULO (PID)
//==================================================
// angulo = angulo_sensor + AJUSTE_ANGULO       (0° = carrito equilibrado)
// e = angulo - set_point_desplazamiento        (set point: salida del
//                                               controlador de desplazamiento)
// rpm_ref = Kp*e + Ki*∫e + Kd*velocidad_caida
// La acción derivativa usa directamente la velocidad de caída del giroscopio.
// Su salida es la referencia de RPM común para ambos motores.
// KI_ANGULO = 0 lo deja como PD.

const float KP_ANGULO = 13.0f;           // rpm/°        POR SINTONIZAR
const float KI_ANGULO = 0.0f;           // rpm/(°*s)    POR SINTONIZAR
const float KD_ANGULO = 0.2f;           // rpm/(°/s)    POR SINTONIZAR
const float RPM_MAX_ANGULO = 60.0f;     // Límite de la salida (±rpm)

// Ajuste del sensor: se suma a la medida para que 0° sea el punto de
// equilibrio. Se usa solo si no hay calibración guardada; el botón de
// calibración lo fija en -(ángulo medido en equilibrio). Editable en la web.
const float AJUSTE_ANGULO = 0.0f;       // °

// Seguridad (sobre el ángulo ya ajustado): fuera de ±ANGULO_CAIDA se apagan
// los motores y se reinician los controladores. Se vuelve a controlar cuando
// el carrito se pone dentro de ±ANGULO_REARME (también al arrancar).
const float ANGULO_CAIDA  = 30.0f;      // °
const float ANGULO_REARME = 10.0f;       // °

//==================================================
// CONTROLADOR DE GIRO (P o PI sobre la velocidad de giro en Z)
//==================================================
// e = VEL_GIRO_REF - velocidad_giro
// rpm_motor1 = rpm_angulo + u_giro,   rpm_motor2 = rpm_angulo - u_giro
// KI_GIRO = 0 lo deja como proporcional puro.

const float KP_GIRO = 0.01f;             // rpm/(°/s)   POR SINTONIZAR
const float KI_GIRO = 0.0f;             // rpm/°       POR SINTONIZAR
const float RPM_MAX_GIRO = 10.0f;       // Límite de la salida (±rpm)
const float VEL_GIRO_REF = 0.0f;        // °/s; 0 = avanzar recto

//==================================================
// CONTROLADOR DE DESPLAZAMIENTO (PID sobre la posición)
//==================================================
// Es el lazo externo y su set point (POSICION_REF) es la única referencia
// del sistema. Mantiene el carrito quieto en el punto donde empezó a equilibrar.
//   x = ((cuentas1 + cuentas2)/2) * M_X_CUENTA     [m]   (desde ese punto)
//   v = ((rpm1 + rpm2)/2) * PI*DIAMETRO_RUEDA/60   [m/s]
//   set_point_angulo = Kp*(ref - x) + Ki*∫(ref - x) + Kd*(0 - v)   [°]
// Su salida es el set point del controlador de ángulo: si el carrito avanzó
// más allá de la referencia, pide inclinarse hacia atrás para que regrese.

const float KP_POSICION = 15.0f;         // °/m          POR SINTONIZAR
const float KI_POSICION = 5.0f;         // °/(m*s)      POR SINTONIZAR
const float KD_POSICION = 0.2f;         // °/(m/s)      POR SINTONIZAR
const float ANGULO_MAX_POSICION = 20.0f; // Límite del set point de ángulo (±°)
const float POSICION_REF = 0.0f;        // m, respecto al punto de inicio (editable en la web)

//==================================================
// MPU6050
//==================================================

const int PIN_MPU_SDA = 12;
const int PIN_MPU_SCL = 11;
const uint8_t MPU_DIRECCION = 0x68;     // AD0 a GND
const uint32_t I2C_FRECUENCIA = 400000; // 400 kHz (modo rápido)

// Frecuencia de muestreo = 1 kHz / (1 + SMPLRT_DIV) con el DLPF activo
const uint8_t MPU_SMPLRT_DIV = 1;       // -> 500 Hz
// DLPF_CFG (registro 0x1A): 2 -> giro 98 Hz (2.8 ms), acel 94 Hz (3.0 ms)
//                           3 -> giro 42 Hz (4.8 ms), acel 44 Hz (4.9 ms)
const uint8_t MPU_DLPF_CFG = 3;
// GYRO_CONFIG FS_SEL: 0=±250, 1=±500, 2=±1000, 3=±2000 °/s
const uint8_t MPU_RANGO_GIRO = 1;
// ACCEL_CONFIG AFS_SEL: 0=±2, 1=±4, 2=±8, 3=±16 g
const uint8_t MPU_RANGO_ACEL = 0;

// Muestras promediadas al arrancar para quitar el offset del giroscopio
// cuando no hay calibración guardada en la flash (carrito quieto ~1 s).
const int MPU_MUESTRAS_CALIBRACION = 500;

// Filtro de Kalman del ángulo de pitch (estado: ángulo [°], sesgo del giro [°/s]).
// Predicción con el giroscopio, corrección con el ángulo del acelerómetro.
//   Q_ANGULO grande -> sigue más al acelerómetro (más rápido, más ruido)
//   R_MEDIDA grande -> confía menos en el acelerómetro (más suave)
const float KALMAN_Q_ANGULO = 0.001f;
const float KALMAN_Q_SESGO  = 0.003f;
const float KALMAN_R_MEDIDA = 0.1f;
// Rechazo de vibraciones: si |‖a‖ - 1 g| supera este valor no se corrige
// con el acelerómetro en ese paso (solo se predice con el giroscopio).
const float KALMAN_RECHAZO_G = 0.15f;

// Montaje del MPU (0 = X, 1 = Y, 2 = Z)
const int EJE_RUEDAS   = 1;  // eje paralelo al eje de las ruedas (eje de caída)
const int EJE_VERTICAL = 2;  // eje que apunta hacia arriba con el carrito parado (eje de giro)
static_assert(EJE_RUEDAS != EJE_VERTICAL, "EJE_RUEDAS y EJE_VERTICAL deben ser distintos");

// Ajustar a 1 o -1 en la prueba con el carrito:
//   SIGNO_ANGULO: inclinarlo hacia adelante (sentido de RPM positivas) debe dar ángulo positivo.
//   SIGNO_GIRO:   motor 1 más rápido que el motor 2 debe dar velocidad de giro positiva.
const float SIGNO_ANGULO = 1.0f;
const float SIGNO_GIRO   = 1.0f;

// Lecturas fallidas seguidas del MPU antes de apagar los motores por seguridad
const int MPU_FALLAS_MAX = 5;

//==================================================
// BOTON DE CALIBRACION (GPIO14 a GND, pull-up interno)
//==================================================
// Con los motores apagados: espera CAL_ESPERA_MS, toma CAL_MUESTRAS lecturas
// (una cada TS_BASE_MS), verifica que el carrito estuvo quieto y guarda en
// la flash el bias del giroscopio y el ángulo de equilibrio.

const int PIN_BOTON_CALIBRAR = 14;
const uint32_t BOTON_ANTIRREBOTE_MS = 20;

const uint32_t CAL_ESPERA_MS = 1500;          // para soltar el botón y estabilizar
const int CAL_MUESTRAS = 500;                 // 500 x 2 ms = 1 s
const float CAL_DESVIACION_MAX = 0.5f;        // °/s, desviación estándar máxima del giroscopio
const int CAL_FALLAS_MAX = 10;                // lecturas fallidas admitidas durante el muestreo
const uint32_t CAL_MOSTRAR_RESULTADO_MS = 10000;  // tiempo que la web muestra el resultado

//==================================================
// VOLTAJE
//==================================================

// Voltaje actual de la batería. Se define en config.cpp (valor al arrancar)
// y se puede cambiar desde la interfaz web; lo escribe solo la tarea de control.
extern float V_BATERIA;
// Rango aceptado desde la web (el TB6612 admite hasta 13.5 V en VM)
const float V_BATERIA_MIN = 4.5f;
const float V_BATERIA_MAX = 13.5f;
const float V_MAX_CONTROL = 6.6;      // Límite de la salida del controlador

// Compensación de zona muerta: voltaje mínimo en el que cada motor empieza
// a girar. Se suma (con el signo de u) a la salida del PI. 0 = sin compensar.
// Sus valores se definen en config.cpp (dependen de V_BATERIA).
extern const float V_ZONA_MUERTA_1;
extern const float V_ZONA_MUERTA_2;

//==================================================
// ENCODERS
//==================================================

const float CPR = 1960;  // Cuentas por revolución en x4

// Desplazamiento lineal por cuenta: perímetro de la rueda / CPR
const float DIAMETRO_RUEDA = 0.067f;                  // m
const float M_X_CUENTA = (PI * DIAMETRO_RUEDA) / CPR; // m por cuenta (~0.107 mm)

// Promedio móvil de la medición de RPM (igual que en la identificación).
// No cambia el tiempo de muestreo, pero agrega un retardo de ~(N-1)/2 muestras.
const bool USAR_PROMEDIO_MOVIL = true;
const int  N_PROMEDIO = 5;

// 1 = normal, -1 = invertido (motores espejados)
const int SENTIDO_MOTOR_1 = -1;
const int SENTIDO_MOTOR_2 = 1;

//==================================================
// PINES TB6612
//==================================================

const int PIN_PWMA = 4;
const int PIN_AIN2 = 5;
const int PIN_AIN1 = 6;
const int PIN_STBY = 7;
const int PIN_BIN1 = 15;
const int PIN_BIN2 = 16;
const int PIN_PWMB = 17;

const uint32_t PWM_FREQ = 1000;  // 1 kHz
const uint8_t  PWM_RES  = 8;     // 8 bits -> 0..255

//==================================================
// PINES ENCODERS
//==================================================

const int ENC1_A = 10;
const int ENC1_B = 9;
const int ENC2_A = 18;
const int ENC2_B = 8;

// El contador por hardware es de 16 bits; al llegar a estos límites el
// driver acumula el valor y lo vuelve a 0, así el conteo total no se desborda.
const int PCNT_LIMITE_ALTO = 30000;
const int PCNT_LIMITE_BAJO = -30000;
const uint32_t PCNT_FILTRO_NS = 1000;  // ignora pulsos más cortos que 1 us (ruido)

//==================================================
// SERIAL (solo mensajes de estado, sin envío de muestras)
//==================================================

const uint32_t SERIAL_BAUDIOS = 115200;

//==================================================
// INTERFAZ WEB
//==================================================

const uint16_t WEB_PUERTO = 80;
// Muestras guardadas para las gráficas (una cada TS_ANGULO_MS): 150 = 3 s.
// Si la página no las pide a tiempo se descartan las más viejas.
const int TAMANO_COLA_MUESTRAS = 150;

//==================================================
// NUCLEOS Y TAREAS (FreeRTOS)
//==================================================
// Núcleo 0: WiFi, lwIP, esp_timer y eventos de red (fijados ahí por el
//           framework). Ahí deben ir las tareas de comunicación futuras.
// Núcleo 1: exclusivo para el control en tiempo real (incluye el I2C del MPU).

const BaseType_t NUCLEO_CONTROL      = 1;
const BaseType_t NUCLEO_COMUNICACION = 0;

// Prioridades (0 = mínima, configMAX_PRIORITIES-1 = 24 = máxima).
// En el núcleo 1 la tarea de control es la única con prioridad alta.
// En el núcleo 0 las de comunicación deben quedar por debajo de WiFi (23) y lwIP (18).
const UBaseType_t PRIORIDAD_CONTROL      = 20;
const UBaseType_t PRIORIDAD_COMUNICACION = 5;

const uint32_t PILA_CONTROL      = 4096;  // bytes
const uint32_t PILA_COMUNICACION = 4096;

//==================================================
// WIFI (punto de acceso propio, sin internet)
//==================================================
// El ESP32 crea su propia red; los dispositivos se conectan a ella y
// acceden al carrito en WIFI_AP_IP.

const char *const WIFI_AP_SSID  = "Balancin";
const char *const WIFI_AP_CLAVE = "balancin123";  // mínimo 8 caracteres
const uint8_t WIFI_AP_CANAL = 6;                  // 1..13 (2.4 GHz)
const uint8_t WIFI_AP_MAX_CLIENTES = 4;
const IPAddress WIFI_AP_IP(192, 168, 4, 1);
const IPAddress WIFI_AP_MASCARA(255, 255, 255, 0);
