/*
  Zona_muerta_motores
  ESP32-S3 N16R8 + TB6612FNG + 2 motores con encoder

  Funcionamiento por Serial (115200 baudios, terminador "Nueva línea"):
    1) Se pregunta qué motor mover:  "1" o "A" -> Motor 1 (canal A)
                                     "2" o "B" -> Motor 2 (canal B)
    2) Se envían valores enteros de PWM entre 0 y 255. Cada valor nuevo
       reemplaza al anterior inmediatamente.
    3) Enviar "stop" (o "s" / "p" / "parar") detiene el motor y vuelve al paso 1.

  Mientras el motor está activo se imprime el conteo del encoder y los
  pulsos por segundo, útil para identificar la zona muerta (PWM mínimo
  en el que el motor empieza a girar).

  Los encoders se leen con el periférico PCNT (contador de pulsos por
  hardware), así el conteo no depende de interrupciones por flanco ni de
  la carga del loop(). Requiere el core ESP32 de Arduino 3.x (ESP-IDF 5).
*/

#if ESP_ARDUINO_VERSION_MAJOR < 3
#error "Este sketch requiere el core ESP32 de Arduino 3.x (driver PCNT de ESP-IDF 5)."
#endif

#include "driver/pulse_cnt.h"
#include "driver/gpio.h"

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
int motorActivo = 0;   // 1 o 2
int pwmActual   = 0;

// ------------------ Encoders (PCNT) ------------------
// Modo de conteo:
//   1 -> solo flancos de subida de la fase A (igual que el attachInterrupt anterior)
//   2 -> subida y bajada de la fase A
//   4 -> todos los flancos de A y B (cuadratura completa, máxima resolución)
const int MODO_CONTEO = 4;

// El contador por hardware es de 16 bits; al llegar a estos límites el
// driver acumula el valor y lo vuelve a 0, así el conteo total no se desborda.
const int PCNT_LIMITE_ALTO = 30000;
const int PCNT_LIMITE_BAJO = -30000;
const uint32_t PCNT_FILTRO_NS = 1000;  // ignora pulsos más cortos que 1 us (ruido)

pcnt_unit_handle_t unidadEnc1 = NULL;
pcnt_unit_handle_t unidadEnc2 = NULL;

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

  pcnt_channel_edge_action_t accionBajadaA =
    (MODO_CONTEO == 1) ? PCNT_CHANNEL_EDGE_ACTION_HOLD : PCNT_CHANNEL_EDGE_ACTION_DECREASE;
  ESP_ERROR_CHECK(pcnt_channel_set_edge_action(canalA,
                  PCNT_CHANNEL_EDGE_ACTION_INCREASE, accionBajadaA));
  ESP_ERROR_CHECK(pcnt_channel_set_level_action(canalA,
                  PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));

  // Canal B (solo en x4): cuenta flancos de la fase B, la fase A define la dirección
  if (MODO_CONTEO == 4) {
    pcnt_chan_config_t configCanalB = {};
    configCanalB.edge_gpio_num  = pinB;
    configCanalB.level_gpio_num = pinA;
    pcnt_channel_handle_t canalB = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(unidad, &configCanalB, &canalB));
    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(canalB,
                    PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(canalB,
                    PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));
  }

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

// ------------------ Lectura serial ------------------
String bufferSerial = "";

// ------------------ Reporte ------------------
const unsigned long INTERVALO_REPORTE = 500;  // ms
unsigned long ultimoReporte = 0;
long conteoAnterior = 0;

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

long leerConteo(int motor) {
  int c = 0;
  pcnt_unit_get_count((motor == 1) ? unidadEnc1 : unidadEnc2, &c);
  return c;
}

void reiniciarConteo(int motor) {
  pcnt_unit_clear_count((motor == 1) ? unidadEnc1 : unidadEnc2);
}

void preguntarMotor() {
  Serial.println();
  Serial.println("========================================");
  Serial.println("¿Qué motor desea mover?");
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
    if (cmd == "1" || cmd == "a") {
      motorActivo = 1;
    } else if (cmd == "2" || cmd == "b") {
      motorActivo = 2;
    } else {
      Serial.println("Opción inválida. Envíe 1/A o 2/B.");
      return;
    }

    pwmActual = 0;
    configurarAdelante(motorActivo);
    escribirPWM(motorActivo, 0);
    conteoAnterior = leerConteo(motorActivo);
    ultimoReporte = millis();
    estado = RECIBIENDO_PWM;

    Serial.print("Motor ");
    Serial.print(motorActivo);
    Serial.println(" seleccionado.");
    Serial.println("Envíe valores de PWM (0 - 255). Envíe 'stop' para detener.");
    return;
  }

  // estado == RECIBIENDO_PWM
  if (esComandoStop(cmd)) {
    detenerMotor(motorActivo);
    reiniciarConteo(motorActivo);
    conteoAnterior = 0;
    Serial.print("Motor ");
    Serial.print(motorActivo);
    Serial.println(" detenido. Conteo del encoder reiniciado a 0.");
    motorActivo = 0;
    pwmActual = 0;
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

void reportarEncoder() {
  if (estado != RECIBIENDO_PWM) return;

  unsigned long ahora = millis();
  if (ahora - ultimoReporte < INTERVALO_REPORTE) return;

  long conteo = leerConteo(motorActivo);
  float dt = (ahora - ultimoReporte) / 1000.0;
  float pulsosPorSeg = (conteo - conteoAnterior) / dt;

  Serial.print("[M");
  Serial.print(motorActivo);
  Serial.print("] PWM: ");
  Serial.print(pwmActual);
  Serial.print(" | Conteo: ");
  Serial.print(conteo);
  Serial.print(" | Pulsos/s: ");
  Serial.println(pulsosPorSeg, 1);

  conteoAnterior = conteo;
  ultimoReporte = ahora;
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

  Serial.println("Zona_muerta_motores - ESP32-S3 + TB6612");
  preguntarMotor();
}

void loop() {
  leerSerial();
  reportarEncoder();
}
