/*
  Identificación_motores
  ESP32-S3 N16R8 + TB6612FNG + 2 motores con encoder

  Ensayo de identificación con dos escalones de voltaje. Basado en
  Planta_ESP32_Datas, adaptado a PCNT en cuadratura x4.

  El motor y los dos escalones de voltaje se fijan en MOTOR_ENSAYO, V1 y V2.
  El ensayo inicia solo, ESPERA_INICIO_MS después de encender.

  Conversión voltaje -> PWM:  PWM = V / V_FUENTE * 255

  Salida de datos (igual que Planta_ESP32_Datas, para el script de Python,
  115200 baudios):
    INICIO,<rep>
    <rpm>        (NUM_MUESTRAS líneas, 4 decimales)
    FIN,<rep>
    ...
    COMPLETADO

  RPM = (conteo_actual - conteo_anterior) * 60 / (CPR * Ts[s])

  Requiere el core ESP32 de Arduino 3.x (ESP-IDF 5).
*/

#if ESP_ARDUINO_VERSION_MAJOR < 3
#error "Este sketch requiere el core ESP32 de Arduino 3.x (driver PCNT de ESP-IDF 5)."
#endif

#include "driver/pulse_cnt.h"
#include "driver/gpio.h"

//==================================================
// CONFIGURACION ENSAYO
//==================================================

const int NUM_MUESTRAS     = 400;
const int NUM_REPETICIONES = 30;
const int MUESTRA_ESCALON  = 200;  // muestra en la que se pasa al 2do escalón

const int Ts = 10; // ms

const float CPR = 1960;  // Cuentas por revolución en x4

//==================================================
// VOLTAJE
//==================================================

constexpr float V_FUENTE = 12.3;  // Voltaje de referencia para la conversión
constexpr float V_MAX    = 6.6;   // Límite de voltaje de los escalones

//==================================================
// ENSAYO FIJO
//==================================================

const int MOTOR_ENSAYO = 2;       // Motor a identificar: 1 o 2

constexpr float V1 = 3.6;         // Voltaje del 1er escalón
constexpr float V2 = 6.6;         // Voltaje del 2do escalón

const unsigned long ESPERA_INICIO_MS = 4000;  // Espera antes de iniciar el ensayo

static_assert(MOTOR_ENSAYO == 1 || MOTOR_ENSAYO == 2, "MOTOR_ENSAYO debe ser 1 o 2");
static_assert(V1 >= 0 && V1 <= V_MAX, "V1 debe estar entre 0 y V_MAX");
static_assert(V2 >= 0 && V2 <= V_MAX, "V2 debe estar entre 0 y V_MAX");

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

pcnt_unit_handle_t unidadEnc1 = NULL;
pcnt_unit_handle_t unidadEnc2 = NULL;

//==================================================
// BUFFER DE DATOS
//==================================================

float datos[NUM_MUESTRAS];

//==================================================
// FILTRO PROMEDIO MOVIL
//==================================================

float bufferRPM[5] = {0};

float sumaRPM = 0;

int indiceRPM = 0;

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

long leerConteo(int motor)
{
    int c = 0;
    pcnt_unit_get_count((motor == 1) ? unidadEnc1 : unidadEnc2, &c);
    return c;
}

void reiniciarConteo(int motor)
{
    pcnt_unit_clear_count((motor == 1) ? unidadEnc1 : unidadEnc2);
}

//==================================================
// MOTORES
//==================================================

int voltajeAPWM(float v)
{
    int pwm = (int)((v / V_FUENTE) * 255.0);  // trunca para no pasar de V_MAX
    return constrain(pwm, 0, 255);
}

void escribirPWM(int motor, int valor)
{
    if (motor == 1) ledcWrite(PIN_PWMA, valor);
    else            ledcWrite(PIN_PWMB, valor);
}

// Sentido de giro "adelante" fijo (IN1 = HIGH, IN2 = LOW)
void configurarAdelante(int motor)
{
    if (motor == 1) {
        digitalWrite(PIN_AIN1, HIGH);
        digitalWrite(PIN_AIN2, LOW);
    } else {
        digitalWrite(PIN_BIN1, HIGH);
        digitalWrite(PIN_BIN2, LOW);
    }
}

void detenerMotor(int motor)
{
    escribirPWM(motor, 0);
    if (motor == 1) {
        digitalWrite(PIN_AIN1, LOW);
        digitalWrite(PIN_AIN2, LOW);
    } else {
        digitalWrite(PIN_BIN1, LOW);
        digitalWrite(PIN_BIN2, LOW);
    }
}

//==================================================
// SETUP
//==================================================

void setup()
{
    Serial.begin(115200);

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

    delay(ESPERA_INICIO_MS);

    ejecutarEnsayos();
}

//==================================================
// LOOP VACIO
//==================================================

void loop()
{
}

//==================================================
// ENSAYOS
//==================================================

void ejecutarEnsayos()
{
    const int PWM1 = voltajeAPWM(V1);
    const int PWM2 = voltajeAPWM(V2);
    const float Ts_s = Ts / 1000.0;

    configurarAdelante(MOTOR_ENSAYO);

    for(int rep = 1; rep <= NUM_REPETICIONES; rep++)
    {
        // Reiniciar filtro
        for(int i=0;i<5;i++)
            bufferRPM[i] = 0;

        sumaRPM = 0;
        indiceRPM = 0;

        // Motor detenido

        escribirPWM(MOTOR_ENSAYO, 0);

        delay(1000);

        reiniciarConteo(MOTOR_ENSAYO);
        long conteoAnterior = 0;

        unsigned long tAnterior = millis();

        for(int i = 0; i < NUM_MUESTRAS; i++)
        {
            while(millis() - tAnterior < Ts);

            tAnterior += Ts;

            //--------------------------------------------------
            // Escalones
            //--------------------------------------------------

            if(i < MUESTRA_ESCALON)
            {
                escribirPWM(MOTOR_ENSAYO, PWM1);
            }
            else
            {
                escribirPWM(MOTOR_ENSAYO, PWM2);
            }

            //--------------------------------------------------
            // Lectura encoder (PCNT x4)
            //--------------------------------------------------

            long conteo = leerConteo(MOTOR_ENSAYO);
            long delta = conteo - conteoAnterior;
            conteoAnterior = conteo;

            float rpm = (delta * 60.0) / (CPR * Ts_s);

            //--------------------------------------------------
            // Promedio movil 5 muestras
            //--------------------------------------------------

            sumaRPM -= bufferRPM[indiceRPM];

            bufferRPM[indiceRPM] = rpm;

            sumaRPM += rpm;

            indiceRPM = (indiceRPM + 1) % 5;

            rpm = sumaRPM / 5.0;

            //--------------------------------------------------
            // Guardar muestra
            //--------------------------------------------------

            datos[i] = rpm;
        }

        //------------------------------------------------------
        // Detener motor
        //------------------------------------------------------

        escribirPWM(MOTOR_ENSAYO, 0);

        //------------------------------------------------------
        // Enviar ensayo completo
        //------------------------------------------------------

        Serial.printf("INICIO,%d\n", rep);

        for(int i = 0; i < NUM_MUESTRAS; i++)
        {
            Serial.println(datos[i], 4);
        }

        Serial.printf("FIN,%d\n", rep);
    }

    detenerMotor(MOTOR_ENSAYO);

    Serial.println("COMPLETADO");
}
