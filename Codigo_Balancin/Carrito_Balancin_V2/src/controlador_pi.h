/*
  controlador_pi.h
  Dos formas de PI discreto:

  ControladorPI (forma de velocidad, usada en los motores):
    u[k] = u[k-1] + q0*e[k] + q1*e[k-1]
    q0 = Kp + Ki*Ts,  q1 = -Kp            (Euler hacia atrás)
    La salida se satura en ±uMax y se guarda saturada (anti-windup).

  ControladorPIPosicional (usado en el giro):
    I[k] = I[k-1] + Ki*Ts*e[k]   (limitada a ±uMax, anti-windup)
    u[k] = Kp*e[k] + I[k]         (saturada a ±uMax)
    Con Ki = 0 es un proporcional puro, también al saturar.
*/
#pragma once

struct ControladorPI {
  float q0, q1;        // coeficientes
  float u, eAnterior;  // estado
};

void configurarPI(ControladorPI &pi, float kp, float ki, float ts);
void reiniciarPI(ControladorPI &pi);

// Calcula la nueva salida a partir del error actual
float calcularPI(ControladorPI &pi, float e, float uMax);


struct ControladorPIPosicional {
  float kp, ki, ts;
  float uMax;
  float integral;      // estado
};

void configurarPIPosicional(ControladorPIPosicional &pi, float kp, float ki, float ts, float uMax);
void reiniciarPIPosicional(ControladorPIPosicional &pi);
float calcularPIPosicional(ControladorPIPosicional &pi, float e);
