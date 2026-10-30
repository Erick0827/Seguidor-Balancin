#include "controlador_pi.h"
#include <Arduino.h>

void configurarPI(ControladorPI &pi, float kp, float ki, float ts)
{
  pi.q0 = kp + ki * ts;
  pi.q1 = -kp;
  reiniciarPI(pi);
}

void reiniciarPI(ControladorPI &pi)
{
  pi.u = 0;
  pi.eAnterior = 0;
}

float calcularPI(ControladorPI &pi, float e, float uMax)
{
  float u = pi.u + pi.q0 * e + pi.q1 * pi.eAnterior;
  u = constrain(u, -uMax, uMax);

  pi.u = u;
  pi.eAnterior = e;
  return u;
}


void configurarPIPosicional(ControladorPIPosicional &pi, float kp, float ki, float ts, float uMax)
{
  pi.kp = kp;
  pi.ki = ki;
  pi.ts = ts;
  pi.uMax = uMax;
  reiniciarPIPosicional(pi);
}

void reiniciarPIPosicional(ControladorPIPosicional &pi)
{
  pi.integral = 0;
}

float calcularPIPosicional(ControladorPIPosicional &pi, float e)
{
  pi.integral += pi.ki * pi.ts * e;
  pi.integral = constrain(pi.integral, -pi.uMax, pi.uMax);

  float u = pi.kp * e + pi.integral;
  return constrain(u, -pi.uMax, pi.uMax);
}
