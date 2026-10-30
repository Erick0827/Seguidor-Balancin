/*
  medicion_de_rpm
  ESP32-S3 N16R8 + TB6612FNG + 2 motores con encoder

  Funcionamiento por Serial (115200 baudios, terminador "Nueva línea"):
    1) Se pregunta qué motor medir:  "1" o "A" -> Motor 1 (canal A)
                                     "2" o "B" -> Motor 2 (canal B)
    2) Se envían valores enteros de PWM entre 0 y 255. Cada valor nuevo
       reemplaza al anterior inmediatamente.
    3) Enviar "stop" (o "s" / "p" / "parar") detiene el motor, reinicia el
       conteo del encoder y vuelve al paso 1.

  Medición de RPM:
    - Encoders leídos con PCNT en cuadratura x4 (todos los flancos de A y B).
    - Un temporizador periódico (esp_timer) toma la muestra cada
      TIEMPO_MUESTREO_MS, independiente de lo que haga el loop().
    - RPM = (conteo_actual - conteo_anterior) * 60 / (CPR * Ts[s])

  Requiere el core ESP32 de Arduino 3.x (ESP-IDF 5).
*/

#if ESP_ARDUINO_VERSION_MAJOR < 3
#error "Este sketch requiere el core ESP32 de Arduino 3.x (driver PCNT de ESP-IDF 5)."
#endif

#include "driver/pulse_cnt.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/queue.h"

// ------------------ Parámetros de medición ------------------
const uint32_t TIEMPO_MUESTREO_MS = 10;   // Tiempo de muestreo
const float    CPR                = 1960;  // Cuentas por revolución en x4

// ------------------ Pines TB6612 ------------------
const int PIN_PWMA = 4;
const int PIN_AIN2 = 5;
const int PIN_AIN1 = 6;
const int PIN_STBY = 7;
const int PIN_BIN1 = 15;
const int PIN_BIN2 = 16;
const int PIN_PWMB = 17;

// ------------------ Pines encoders ------------------
const int ENC1_A = 10;
const int ENC1_B = 9;
const int ENC2_A = 18;
const int ENC2_B = 8;

// ------------------ PWM ------------------
const uint32_t PWM_FREQ = 1000;  // 1 kHz
const uint8_t  PWM_RES  = 8;     // 8 bits -> 0..255

// ------------------ Estados ------------------
enum Estado { ESPERANDO_MOTOR, RECIBIENDO_PWM };
Estado estado = ESPERANDO_MOTOR;
volatile int motorActivo = 0;   // 1 o 2 (0 = ninguno, no se muestrea)
int pwmActual = 0;

// ------------------ Encoders (PCNT x4) ------------------
// El contador por hardware es de 16 bits; al llegar a estos límites el
// driver acumula el valor y lo vuelve a 0, así el conteo total no se desborda.
const int PCNT_LIMITE_ALTO = 30000;
const int PCNT_LIMITE_BAJO = -30000;
const uint32_t PCNT_FILTRO_NS = 1000;  // ignora pulsos más cortos que 1 us (ruido)

pcnt_unit_handle_t unidadEnc1 = NULL;
pcnt_unit_handle_t unidadEnc2 = NULL;

// ------------------ Muestreo ------------------
struct Muestra {
  int   motor;
  int   pwm;
  long  conteo;
  long  delta;
  float rpm;
};

esp_timer_handle_t temporizadorMuestreo = NULL;
QueueHandle_t colaMuestras = NULL;
volatile bool reiniciarReferencia = true;  // la siguiente muestra solo toma referencia
volatile int pwmMuestreo = 0;
long conteoAnterior = 0;                   // usado solo por el temporizador

// ------------------ Lectura serial ------------------
String bufferSerial = "";

// ==================================================
// Sentido positivo: la fase A sube mientras la fase B está en alto
pcnt_unit_handle_t crearEncoderPCNT(int pinA, int pinB) {
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

long leerConteo(int motor) {
  int c = 0;
  pcnt_unit_get_count((motor == 1) ? unidadEnc1 : unidadEnc2, &c);
  return c;
}

void reiniciarConteo(int motor) {
  pcnt_unit_clear_count((motor == 1) ? unidadEnc1 : unidadEnc2);
}

// Se ejecuta cada TIEMPO_MUESTREO_MS en la tarea de esp_timer
void tomarMuestra(void *arg) {
  int motor = motorActivo;
  if (motor == 0) return;

  long conteo = leerConteo(motor);

  if (reiniciarReferencia) {
    conteoAnterior = conteo;
    reiniciarReferencia = false;
    return;
  }

  Muestra m;
  m.motor  = motor;
  m.pwm    = pwmMuestreo;
  m.conteo = conteo;
  m.delta  = conteo - conteoAnterior;
  m.rpm    = (m.delta * 60.0f) / (CPR * (TIEMPO_MUESTREO_MS / 1000.0f));
  conteoAnterior = conteo;

  xQueueSend(colaMuestras, &m, 0);  // si la cola está llena se descarta
}

// ==================================================
void escribirPWM(int motor, int valor) {
  if (motor == 1) ledcWrite(PIN_PWMA, valor);
  else            ledcWrite(PIN_PWMB, valor);
}

// Sentido de giro "adelante" fijo (IN1 = HIGH, IN2 = LOW)
void configurarAdelante(int motor) {
  if (motor == 1) {
    digitalWrite(PIN_AIN1, HIGH);
    digitalWrite(PIN_AIN2, LOW);
  } else {
    digitalWrite(PIN_BIN1, HIGH);
    digitalWrite(PIN_BIN2, LOW);
  }
}

void detenerMotor(int motor) {
  escribirPWM(motor, 0);
  if (motor == 1) {
    digitalWrite(PIN_AIN1, LOW);
    digitalWrite(PIN_AIN2, LOW);
  } else {
    digitalWrite(PIN_BIN1, LOW);
    digitalWrite(PIN_BIN2, LOW);
  }
}

void preguntarMotor() {
  Serial.println();
  Serial.println("========================================");
  Serial.println("¿Qué motor desea medir?");
  Serial.println("  1 (o A) -> Motor 1");
  Serial.println("  2 (o B) -> Motor 2");
  Serial.println("========================================");
}

bool esComandoStop(const String &s) {
  return s == "stop" || s == "s" || s == "p" || s == "parar";
}

bool esEntero(const String &s) {
  if (s.length() == 0) return false;
  for (unsigned int i = 0; i < s.length(); i++) {
    if (!isDigit(s[i])) return false;
  }
  return true;
}

// ==================================================
void procesarComando(String cmd) {
  cmd.trim();
  cmd.toLowerCase();
  if (cmd.length() == 0) return;

  if (estado == ESPERANDO_MOTOR) {
    int motor;
    if (cmd == "1" || cmd == "a") {
      motor = 1;
    } else if (cmd == "2" || cmd == "b") {
      motor = 2;
    } else {
      Serial.println("Opción inválida. Envíe 1/A o 2/B.");
      return;
    }

    pwmActual = 0;
    pwmMuestreo = 0;
    configurarAdelante(motor);
    escribirPWM(motor, 0);
    reiniciarConteo(motor);
    reiniciarReferencia = true;
    motorActivo = motor;  // a partir de aquí el temporizador empieza a muestrear
    estado = RECIBIENDO_PWM;

    Serial.print("Motor ");
    Serial.print(motor);
    Serial.print(" seleccionado. Muestreo cada ");
    Serial.print(TIEMPO_MUESTREO_MS);
    Serial.println(" ms.");
    Serial.println("Envíe valores de PWM (0 - 255). Envíe 'stop' para detener.");
    return;
  }

  // estado == RECIBIENDO_PWM
  if (esComandoStop(cmd)) {
    int motor = motorActivo;
    motorActivo = 0;  // detiene el muestreo
    detenerMotor(motor);
    reiniciarConteo(motor);
    xQueueReset(colaMuestras);
    Serial.print("Motor ");
    Serial.print(motor);
    Serial.println(" detenido. Conteo del encoder reiniciado a 0.");
    pwmActual = 0;
    pwmMuestreo = 0;
    estado = ESPERANDO_MOTOR;
    preguntarMotor();
    return;
  }

  if (!esEntero(cmd)) {
    Serial.println("Valor inválido. Envíe un entero entre 0 y 255 o 'stop'.");
    return;
  }

  long valor = cmd.toInt();
  if (valor < 0 || valor > 255) {
    Serial.println("Fuera de rango. El PWM debe estar entre 0 y 255.");
    return;
  }

  pwmActual = (int)valor;
  pwmMuestreo = pwmActual;
  escribirPWM(motorActivo, pwmActual);
  Serial.print("Motor ");
  Serial.print(motorActivo);
  Serial.print(" -> PWM = ");
  Serial.println(pwmActual);
}

void leerSerial() {
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

void imprimirMuestras() {
  Muestra m;
  while (xQueueReceive(colaMuestras, &m, 0) == pdTRUE) {
    Serial.print("[M");
    Serial.print(m.motor);
    Serial.print("] PWM: ");
    Serial.print(m.pwm);
    Serial.print(" | Conteo: ");
    Serial.print(m.conteo);
    Serial.print(" | Delta: ");
    Serial.print(m.delta);
    Serial.print(" | RPM: ");
    Serial.println(m.rpm, 2);
  }
}

// ==================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(PIN_AIN1, OUTPUT);
  pinMode(PIN_AIN2, OUTPUT);
  pinMode(PIN_BIN1, OUTPUT);
  pinMode(PIN_BIN2, OUTPUT);
  pinMode(PIN_STBY, OUTPUT);

  ledcAttach(PIN_PWMA, PWM_FREQ, PWM_RES);
  ledcAttach(PIN_PWMB, PWM_FREQ, PWM_RES);

  detenerMotor(1);
  detenerMotor(2);
  digitalWrite(PIN_STBY, HIGH);  // Habilita el TB6612

  unidadEnc1 = crearEncoderPCNT(ENC1_A, ENC1_B);
  unidadEnc2 = crearEncoderPCNT(ENC2_A, ENC2_B);

  colaMuestras = xQueueCreate(32, sizeof(Muestra));

  esp_timer_create_args_t argsTemporizador = {};
  argsTemporizador.callback = &tomarMuestra;
  argsTemporizador.name = "muestreo_rpm";
  ESP_ERROR_CHECK(esp_timer_create(&argsTemporizador, &temporizadorMuestreo));
  ESP_ERROR_CHECK(esp_timer_start_periodic(temporizadorMuestreo,
                  (uint64_t)TIEMPO_MUESTREO_MS * 1000ULL));

  Serial.println("medicion_de_rpm - ESP32-S3 + TB6612 (PCNT x4)");
  preguntarMotor();
}

void loop() {
  leerSerial();
  imprimirMuestras();
}
