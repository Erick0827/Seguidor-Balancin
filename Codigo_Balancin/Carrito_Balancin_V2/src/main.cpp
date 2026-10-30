/*
  Carrito_Balancin_V2
  ESP32-S3 N16R8 + TB6612FNG + MPU6050 + 2 motores con encoder

  Control en cascada:
    Desplazamiento (PID, 100 ms, encoders) -> set point de ángulo (±5°)
    Ángulo (PID, 20 ms) -> referencia común de RPM (±60 rpm)
    Giro   (P/PI, 100 ms, eje Z) -> se suma al motor 1 y se resta al motor 2
    Velocidad (PI por motor, 10 ms) -> voltaje -> PWM

  Reparto de núcleos:
    Núcleo 1  lazo_control (prioridad alta): MPU + todos los controladores.
    Núcleo 0  WiFi (punto de acceso) + lwIP + eventos de red (framework)
              + servidor_web (interfaz en http://192.168.4.1).

  Módulos:
    config              parámetros, ganancias, pines, núcleos, WiFi
    encoder             lectura x4 con PCNT
    filtro_promedio     promedio móvil de la RPM
    controlador_pi      PI de velocidad (motores) y PI posicional (giro)
    controlador_pid     PID con derivada medida (ángulo y desplazamiento)
    motor               TB6612, medición de RPM y lazo de velocidad
    mpu6050             driver I2C del MPU6050
    imu                 ángulo (Kalman), velocidad de caída y velocidad de giro
    filtro_kalman       filtro de Kalman de 2 estados (ángulo, sesgo del giro)
    lazo_control        tarea multitasa de control y seguridad por caída
    boton               pulsador de calibración (GPIO14) con antirrebote
    calibracion         rutina del botón: bias del giroscopio y ángulo de equilibrio
    almacenamiento      guarda/carga la calibración en la flash (NVS)
    wifi_red            punto de acceso WiFi propio
    servidor_web        rutas HTTP de la interfaz de control
    pagina_web          HTML/CSS/JS de la interfaz

  Historial de cambios: CAMBIOS.txt
  Requiere el core ESP32 de Arduino 3.x (ESP-IDF 5).
*/

#include <Arduino.h>
#include "config.h"
#include "motor.h"
#include "lazo_control.h"
#include "wifi_red.h"
#include "servidor_web.h"

void setup()
{
  Serial.begin(SERIAL_BAUDIOS);

  inicializarMotores();

  if (iniciarLazoControl()) {
    Serial.println("# Control listo: encender los motores desde la interfaz web");
  } else {
    Serial.println("# ERROR: el MPU6050 no responde, los motores no se pueden encender");
  }

  if (iniciarWiFi()) iniciarServidorWeb();
}

void loop()
{
  // Todo el trabajo está en tareas propias; se elimina la tarea de loop()
  // (núcleo 1) para que no le quite tiempo al control.
  vTaskDelete(NULL);
}
