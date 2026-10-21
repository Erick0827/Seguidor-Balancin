/*
  lazo_control.h
  Tarea de control en tiempo real, fijada al núcleo NUCLEO_CONTROL y
  aislada del WiFi que corre en el otro núcleo. Corre cada TS_BASE_MS y
  ejecuta cada lazo con su propio periodo:

    TS_BASE_MS   (2 ms)   lectura del MPU y filtro complementario
    TS_GIRO_MS   (100 ms) controlador de giro    -> u_giro (rpm)
    TS_ANGULO_MS (20 ms)  controlador de ángulo  -> rpm_angulo (±60 rpm)
    TS_MS        (10 ms)  PI de velocidad de cada motor:
                            ref motor 1 = rpm_angulo + u_giro
                            ref motor 2 = rpm_angulo - u_giro

  Los motores solo actúan si están habilitados (botón de la interfaz web),
  el MPU tiene señal y el carrito está dentro de los límites de ángulo.
  Tras una caída se reanuda al volver a ponerlo dentro de ±ANGULO_REARME.

  El botón físico de calibración (calibracion.h) se atiende en esta misma
  tarea; mientras calibra los motores quedan apagados y al terminar siguen
  apagados. Al arrancar se carga la calibración guardada en la flash.

  Las funciones de consulta/ajuste se pueden llamar desde el otro núcleo.
*/
#pragma once

#include <stdint.h>

enum MotivoParo {
  PARO_NINGUNO = 0,     // controlando
  PARO_APAGADO,         // motores apagados por el usuario
  PARO_ESPERA_VERTICAL, // habilitado, esperando que se ponga vertical
  PARO_CAIDA,           // el ángulo superó ±ANGULO_CAIDA
  PARO_FALLA_MPU,       // el MPU no responde
  PARO_CALIBRANDO       // rutina del botón de calibración en curso
};

struct EstadoControl {
  bool habilitado;       // botón de encendido de motores
  bool mpuOk;            // el MPU entrega lecturas
  bool anguloEnLimites;  // |ángulo - referencia| <= ANGULO_CAIDA
  MotivoParo motivo;
};

// Constantes ajustables en caliente (sintonización manual)
struct ParametrosControl {
  float kpAngulo, kdAngulo, anguloRef;
  float kpGiro, kiGiro;
};

// Una muestra cada TS_ANGULO_MS para las gráficas
struct MuestraControl {
  float anguloRef, angulo;
  float ref1, rpm1;
  float ref2, rpm2;
};

// Carga la calibración de la flash, inicia el MPU (sin calibración guardada
// calibra el giroscopio: carrito quieto ~1 s) y arranca la tarea de control.
// Devuelve false si el MPU no responde; la tarea arranca igual (con los
// motores apagados) para poder informar el estado.
bool iniciarLazoControl();

// Se ignora mientras se está calibrando
void habilitarMotores(bool on);
EstadoControl obtenerEstado();

ParametrosControl obtenerParametros();
void fijarParametros(const ParametrosControl &p);

// Voltaje de batería usado para convertir voltaje -> PWM. El nuevo valor
// se aplica en el siguiente ciclo de control. false si está fuera de
// [V_BATERIA_MIN, V_BATERIA_MAX].
float obtenerVoltajeBateria();
bool fijarVoltajeBateria(float v);

// Saca la muestra más antigua pendiente. false si no hay.
bool obtenerMuestra(MuestraControl &m);

// Veces que un ciclo base no alcanzó a terminar dentro de TS_BASE_MS
uint32_t obtenerSobrepasos();
