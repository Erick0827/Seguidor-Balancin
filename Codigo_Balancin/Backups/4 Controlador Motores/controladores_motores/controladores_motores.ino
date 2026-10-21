/*
  controladores_motores
  ESP32-S3 N16R8 + TB6612FNG + 2 motores con encoder

  Dos controladores PI discretos de velocidad (uno por motor) que corren
  simultáneamente con la misma referencia de RPM.

  Entrada por Serial (115200 baudios, terminador "Nueva línea"):
    Un número con la referencia en RPM, positiva o negativa (ej. "60", "-45.5").
    "0" detiene ambos motores y reinicia los controladores.

  Salida por Serial (una línea por cada muestra, cada TS_MS):
    PWM_motor_1,RPM_motor_1,PWM_motor_2,RPM_motor_2
    El PWM lleva signo según el sentido de giro (-255 .. 255).

  Controlador (planta modelada en rpm/V):
    e[k] = ref - rpm[k]
    u[k] = u[k-1] + q0*e[k] + q1*e[k-1]          (u en voltios)
    q0 = Kp + Ki*Ts,  q1 = -Kp                    (PI, Euler hacia atrás)
    u se satura en ±V_MAX_CONTROL (anti-windup: se guarda u saturado).
    Al voltaje aplicado se le suma ±V_ZONA_MUERTA según el signo de u.

  Conversión voltaje -> PWM con fuente variable:
    PWM = |u| / V_BATERIA * 255
    Actualiza V_BATERIA según el nivel de carga de la batería.

  Motores espejados: el Motor 2 tiene invertidos el giro y la medición,
  así una referencia positiva hace avanzar a los dos en el mismo sentido.

  Requiere el core ESP32 de Arduino 3.x (ESP-IDF 5).
*/

#if ESP_ARDUINO_VERSION_MAJOR < 3
#error "Este sketch requiere el core ESP32 de Arduino 3.x (driver PCNT de ESP-IDF 5)."
#endif

#include "driver/pulse_cnt.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/queue.h"

//==================================================
// PARAMETROS DEL CONTROLADOR
//==================================================

const uint32_t TS_MS = 10;            // Tiempo de muestreo
const float    TS    = TS_MS / 1000.0f;

// Ganancias PI de cada motor (Kp en V/rpm, Ki en V/(rpm*s))
const float KP_1 = 0.073037;
const float KI_1 = 2.1599;
const float KP_2 = 0.070481;
const float KI_2 = 2.0043;

//==================================================
// VOLTAJE
//==================================================

float V_BATERIA = 12.3;               // Voltaje actual de la batería (actualízalo)
const float V_MAX_CONTROL = 6.6;     // Límite de la salida del controlador

// Compensación de zona muerta: voltaje mínimo en el que cada motor empieza
// a girar. Se suma (con el signo de u) a la salida del PI. 0 = sin compensar.
const float V_ZONA_MUERTA_1 = (13/255)*V_BATERIA;
const float V_ZONA_MUERTA_2 = (11/255)*V_BATERIA;

//==================================================
// ENCODERS
//==================================================

const float CPR = 1960;  // Cuentas por revolución en x4

// Promedio móvil de la medición de RPM (igual que en la identificación).
// No cambia el tiempo de muestreo, pero agrega un retardo de ~(N-1)/2 muestras.
const bool USAR_PROMEDIO_MOVIL = true;
const int  N_PROMEDIO = 5;

// 1 = normal, -1 = invertido (motores espejados)
const int SENTIDO_MOTOR_1 = 1;
const int SENTIDO_MOTOR_2 = -1;

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
// ESTRUCTURAS
//==================================================

struct Motor {
  int pinPWM, pinIN1, pinIN2;
  int sentido;                  // 1 o -1
  pcnt_unit_handle_t encoder;
  long conteoAnterior;
  float vZonaMuerta;            // compensación de zona muerta (V)
  float q0, q1;                 // coeficientes del PI
  float u, eAnterior;           // estado del PI
  float rpm;                    // RPM usada por el controlador (filtrada o no)
  int pwm;                      // con signo
  float bufferRPM[N_PROMEDIO];  // promedio móvil
  float sumaRPM;
  int indiceRPM;
};

Motor motor1 = { PIN_PWMA, PIN_AIN1, PIN_AIN2, SENTIDO_MOTOR_1, NULL, 0, V_ZONA_MUERTA_1 };
Motor motor2 = { PIN_PWMB, PIN_BIN1, PIN_BIN2, SENTIDO_MOTOR_2, NULL, 0, V_ZONA_MUERTA_2 };

struct Muestra {
  int pwm1;
  float rpm1;
  int pwm2;
  float rpm2;
};

volatile float referenciaRPM = 0;
volatile bool reiniciarControl = false;

esp_timer_handle_t temporizadorControl = NULL;
QueueHandle_t colaMuestras = NULL;

String bufferSerial = "";

//==================================================
// ENCODER PCNT x4
//==================================================

// Sentido positivo: la fase A sube mientras la fase B está en alto
pcnt_unit_handle_t crearEncoderPCNT(int pinA, int pinB)
{
  pcnt_unit_config_t configUnidad = {};
  configUnidad.high_limit = PCNT_LIMITE_ALTO;
  configUnidad.low_limit  = PCNT_LIMITE_BAJO;
  configUnidad.flags.accum_count = 1;

  pcnt_unit_handle_t unidad = NULL;
  ESP_ERROR_CHECK(pcnt_new_unit(&configUnidad, &unidad));

  pcnt_glitch_filter_config_t configFiltro = {};
  configFiltro.max_glitch_ns = PCNT_FILTRO_NS;
  ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(unidad, &configFiltro));

  // Canal A: cuenta flancos de la fase A, la fase B define la dirección
  pcnt_chan_config_t configCanalA = {};
  configCanalA.edge_gpio_num  = pinA;
  configCanalA.level_gpio_num = pinB;
  pcnt_channel_handle_t canalA = NULL;
  ESP_ERROR_CHECK(pcnt_new_channel(unidad, &configCanalA, &canalA));
  ESP_ERROR_CHECK(pcnt_channel_set_edge_action(canalA,
                  PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_DECREASE));
  ESP_ERROR_CHECK(pcnt_channel_set_level_action(canalA,
                  PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));

  // Canal B: cuenta flancos de la fase B, la fase A define la dirección
  pcnt_chan_config_t configCanalB = {};
  configCanalB.edge_gpio_num  = pinB;
  configCanalB.level_gpio_num = pinA;
  pcnt_channel_handle_t canalB = NULL;
  ESP_ERROR_CHECK(pcnt_new_channel(unidad, &configCanalB, &canalB));
  ESP_ERROR_CHECK(pcnt_channel_set_edge_action(canalB,
                  PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE));
  ESP_ERROR_CHECK(pcnt_channel_set_level_action(canalB,
                  PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));

  // Puntos de observación en los límites, necesarios para acumular el conteo
  ESP_ERROR_CHECK(pcnt_unit_add_watch_point(unidad, PCNT_LIMITE_ALTO));
  ESP_ERROR_CHECK(pcnt_unit_add_watch_point(unidad, PCNT_LIMITE_BAJO));

  // Pull-ups internos en las fases (el driver configura los pines como entrada)
  gpio_pullup_en((gpio_num_t)pinA);
  gpio_pullup_en((gpio_num_t)pinB);

  ESP_ERROR_CHECK(pcnt_unit_enable(unidad));
  ESP_ERROR_CHECK(pcnt_unit_clear_count(unidad));
  ESP_ERROR_CHECK(pcnt_unit_start(unidad));
  return unidad;
}

long leerConteo(pcnt_unit_handle_t unidad)
{
  int c = 0;
  pcnt_unit_get_count(unidad, &c);
  return c;
}

//==================================================
// MOTORES
//==================================================

void configurarMotor(Motor &m, int pinA, int pinB, float kp, float ki)
{
  pinMode(m.pinIN1, OUTPUT);
  pinMode(m.pinIN2, OUTPUT);
  ledcAttach(m.pinPWM, PWM_FREQ, PWM_RES);

  m.encoder = crearEncoderPCNT(pinA, pinB);
  m.conteoAnterior = 0;
  m.q0 = kp + ki * TS;
  m.q1 = -kp;
  m.u = 0;
  m.eAnterior = 0;
  m.rpm = 0;
  m.pwm = 0;
  for (int i = 0; i < N_PROMEDIO; i++) m.bufferRPM[i] = 0;
  m.sumaRPM = 0;
  m.indiceRPM = 0;
}

// Aplica un voltaje con signo al motor (ya corregido por el espejado)
void aplicarVoltaje(Motor &m, float u)
{
  float vFisico = u * m.sentido;

  int pwm = (int)(fabsf(vFisico) / V_BATERIA * 255.0f);
  pwm = constrain(pwm, 0, 255);

  if (pwm == 0) {
    digitalWrite(m.pinIN1, LOW);
    digitalWrite(m.pinIN2, LOW);
  } else if (vFisico > 0) {
    digitalWrite(m.pinIN1, HIGH);
    digitalWrite(m.pinIN2, LOW);
  } else {
    digitalWrite(m.pinIN1, LOW);
    digitalWrite(m.pinIN2, HIGH);
  }
  ledcWrite(m.pinPWM, pwm);

  // PWM reportado en el mismo sentido que la referencia
  m.pwm = (u >= 0) ? pwm : -pwm;
}

void medirRPM(Motor &m)
{
  long conteo = leerConteo(m.encoder);
  long delta = conteo - m.conteoAnterior;
  m.conteoAnterior = conteo;

  float rpm = m.sentido * (delta * 60.0f) / (CPR * TS);

  if (USAR_PROMEDIO_MOVIL) {
    m.sumaRPM -= m.bufferRPM[m.indiceRPM];
    m.bufferRPM[m.indiceRPM] = rpm;
    m.sumaRPM += rpm;
    m.indiceRPM = (m.indiceRPM + 1) % N_PROMEDIO;
    rpm = m.sumaRPM / N_PROMEDIO;
  }

  m.rpm = rpm;
}

void controlarPI(Motor &m, float ref, bool reiniciar)
{
  if (reiniciar || ref == 0) {
    m.u = 0;
    m.eAnterior = 0;
    aplicarVoltaje(m, 0);
    return;
  }

  float e = ref - m.rpm;
  float u = m.u + m.q0 * e + m.q1 * m.eAnterior;

  float uMax = min(V_MAX_CONTROL, V_BATERIA);
  u = constrain(u, -uMax, uMax);

  m.u = u;
  m.eAnterior = e;

  // La zona muerta se compensa fuera del estado del PI, así el integrador
  // trabaja sobre la planta "linealizada"
  float uAplicado = u;
  if (u > 0)      uAplicado = u + m.vZonaMuerta;
  else if (u < 0) uAplicado = u - m.vZonaMuerta;
  uAplicado = constrain(uAplicado, -uMax, uMax);

  aplicarVoltaje(m, uAplicado);
}

//==================================================
// LAZO DE CONTROL (cada TS_MS, tarea de esp_timer)
//==================================================

void lazoControl(void *arg)
{
  float ref = referenciaRPM;
  bool reiniciar = reiniciarControl;
  reiniciarControl = false;

  medirRPM(motor1);
  medirRPM(motor2);

  controlarPI(motor1, ref, reiniciar);
  controlarPI(motor2, ref, reiniciar);

  Muestra s = { motor1.pwm, motor1.rpm, motor2.pwm, motor2.rpm };
  xQueueSend(colaMuestras, &s, 0);  // si la cola está llena se descarta
}

//==================================================
// SERIAL
//==================================================

// Devuelve true si el texto es un número (con signo y decimales opcionales)
bool leerNumero(String s, float &v)
{
  s.replace(',', '.');
  if (s.length() == 0) return false;

  unsigned int i = 0;
  if (s[0] == '-' || s[0] == '+') i = 1;
  if (i == s.length()) return false;

  bool punto = false;
  for (; i < s.length(); i++) {
    if (s[i] == '.' && !punto) { punto = true; continue; }
    if (!isDigit(s[i])) return false;
  }

  v = s.toFloat();
  return true;
}

void procesarComando(String cmd)
{
  cmd.trim();

  float v;
  if (!leerNumero(cmd, v)) return;  // se ignora para no ensuciar la salida

  if (v == 0) reiniciarControl = true;
  referenciaRPM = v;
}

void leerSerial()
{
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (bufferSerial.length() > 0) {
        procesarComando(bufferSerial);
        bufferSerial = "";
      }
    } else {
      bufferSerial += c;
    }
  }
}

void imprimirMuestras()
{
  Muestra s;
  while (xQueueReceive(colaMuestras, &s, 0) == pdTRUE) {
    Serial.printf("%d,%.2f,%d,%.2f\n", s.pwm1, s.rpm1, s.pwm2, s.rpm2);
  }
}

//==================================================
// SETUP / LOOP
//==================================================

void setup()
{
  Serial.begin(115200);

  pinMode(PIN_STBY, OUTPUT);

  configurarMotor(motor1, ENC1_A, ENC1_B, KP_1, KI_1);
  configurarMotor(motor2, ENC2_A, ENC2_B, KP_2, KI_2);
  aplicarVoltaje(motor1, 0);
  aplicarVoltaje(motor2, 0);

  digitalWrite(PIN_STBY, HIGH);  // Habilita el TB6612

  colaMuestras = xQueueCreate(32, sizeof(Muestra));

  esp_timer_create_args_t argsTemporizador = {};
  argsTemporizador.callback = &lazoControl;
  argsTemporizador.name = "lazo_control";
  ESP_ERROR_CHECK(esp_timer_create(&argsTemporizador, &temporizadorControl));
  ESP_ERROR_CHECK(esp_timer_start_periodic(temporizadorControl,
                  (uint64_t)TS_MS * 1000ULL));
}

void loop()
{
  leerSerial();
  imprimirMuestras();
}
