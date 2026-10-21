#include <Arduino.h>
#include "config.h"

//==================================================
// SELECCION DE MOTOR
//==================================================

// >>> CAMBIAR AQUI: true = motor 1 (A), false = motor 2 (B) <<<
const bool MOTOR_1 = true;

//==================================================
// ENCODER (fase A del motor seleccionado)
//==================================================

const byte encoderPin = MOTOR_1 ? PIN_ENC1_A : PIN_ENC2_A;

volatile long pasos = 0;

const int PPR = ENC_PPR;
const int REDUCCION = ENC_REDUCCION;
const int PPR_TOTAL = PPR * REDUCCION;

//==================================================
// TB6612FNG (solo el motor seleccionado)
//==================================================

const int PIN_PWM = MOTOR_1 ? PIN_PWMA : PIN_PWMB;
const int IN1     = MOTOR_1 ? PIN_AIN1 : PIN_BIN1;
const int IN2     = MOTOR_1 ? PIN_AIN2 : PIN_BIN2;
const int STBY    = PIN_STBY;

const bool INVERTIR = MOTOR_1 ? MOTOR1_INVERTIR : MOTOR2_INVERTIR;

const int CANAL_PWM = MOTOR_1 ? PWM_CANAL_A : PWM_CANAL_B;

//==================================================
// CONFIGURACION ENSAYO
//==================================================

const int NUM_MUESTRAS = 400;
const int NUM_REPETICIONES = 30;

const int PWM1 = 25;
const int PWM2 = 35;

const int Ts = 10; // ms

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
// INTERRUPCION ENCODER
//==================================================

void IRAM_ATTR contarPulso()
{
    pasos++;
}

//==================================================
// MOTOR
//==================================================

// IN1/IN2: 10 = adelante, 01 = atras
void sentidoMotor()
{
    digitalWrite(IN1, INVERTIR ? LOW : HIGH);
    digitalWrite(IN2, INVERTIR ? HIGH : LOW);
}

void escribirPWM(int duty)
{
    ledcWrite(CANAL_PWM, duty);
}

void ejecutarEnsayos();

//==================================================
// SETUP
//==================================================

void setup()
{
    Serial.begin(115200);

    pinMode(encoderPin, INPUT_PULLUP);

    attachInterrupt(
        digitalPinToInterrupt(encoderPin),
        contarPulso,
        RISING
    );

    pinMode(IN1, OUTPUT);
    pinMode(IN2, OUTPUT);
    pinMode(STBY, OUTPUT);

    ledcSetup(CANAL_PWM, PWM_FRECUENCIA, PWM_RESOLUCION);
    ledcAttachPin(PIN_PWM, CANAL_PWM);

    escribirPWM(0);
    sentidoMotor();

    digitalWrite(STBY, HIGH);

    delay(3000);

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
    for(int rep = 1; rep <= NUM_REPETICIONES; rep++)
    {
        // Reiniciar filtro
        for(int i=0;i<5;i++)
            bufferRPM[i] = 0;

        sumaRPM = 0;
        indiceRPM = 0;

        // Motor detenido

        escribirPWM(0);

        delay(1000);

        noInterrupts();
        pasos = 0;
        interrupts();

        unsigned long tAnterior = millis();

        for(int i = 0; i < NUM_MUESTRAS; i++)
        {
            while(millis() - tAnterior < Ts);

            tAnterior += Ts;

            //--------------------------------------------------
            // Escalones
            //--------------------------------------------------

            if(i < 200)
            {
                escribirPWM(PWM1);
            }
            else
            {
                escribirPWM(PWM2);
            }

            //--------------------------------------------------
            // Lectura encoder
            //--------------------------------------------------

            noInterrupts();

            long pulsos = pasos;
            pasos = 0;

            interrupts();

            float rpm =
                pulsos *
                (60000.0 / (PPR_TOTAL * Ts));

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

        escribirPWM(0);

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

    // Driver en standby al terminar
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, LOW);
    digitalWrite(STBY, LOW);

    Serial.println("COMPLETADO");
}
