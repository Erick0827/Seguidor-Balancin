/*
  motor.h
  Manejo de cada motor: driver TB6612, encoder, medición de RPM y lazo
  de velocidad (PI + compensación de zona muerta).

  Motores espejados: el Motor 2 tiene invertidos el giro y la medición,
  así una referencia positiva hace avanzar a los dos en el mismo sentido.
*/
#pragma once

#include "driver/pulse_cnt.h"
#include "controlador_pi.h"
#include "filtro_promedio.h"

struct Motor {
  int pinPWM, pinIN1, pinIN2;
  int sentido;                  // 1 o -1
  pcnt_unit_handle_t encoder;
  long conteoAnterior;
  float vZonaMuerta;            // compensación de zona muerta (V)
  ControladorPI pi;
  FiltroPromedio filtro;
  float rpm;                    // RPM usada por el controlador (filtrada o no)
  int pwm;                      // con signo
};

extern Motor motor1;
extern Motor motor2;

// Configura pines, PWM y encoders de ambos motores y habilita el TB6612
void inicializarMotores();

// Aplica un voltaje con signo al motor (ya corregido por el espejado).
// PWM = |u| / V_BATERIA * 255
void aplicarVoltaje(Motor &m, float u);

// Lee el encoder y actualiza m.rpm
void medirRPM(Motor &m);

// Cuentas acumuladas del encoder (de la última medirRPM) con el signo del
// sentido del motor: positivas cuando el carrito avanza, en ambos motores
long cuentasConSigno(const Motor &m);

// Un paso del lazo de velocidad: PI sobre (ref - rpm) + zona muerta.
// reiniciar = true apaga el motor y borra el estado del PI.
void controlarVelocidad(Motor &m, float ref, bool reiniciar);
