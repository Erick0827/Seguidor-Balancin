#include "controlador_pid.h"
#include <Arduino.h>

void configurarPID(ControladorPID &pid, float kp, float ki, float kd, float ts, float uMax)
{
  pid.kp = kp;
  pid.ki = ki;
  pid.kd = kd;
  pid.ts = ts;
  pid.uMax = uMax;
  reiniciarPID(pid);
}

void reiniciarPID(ControladorPID &pid)
{
  pid.integral = 0;
}

float calcularPID(ControladorPID &pid, float e, float derivada)
{
  pid.integral += pid.ki * pid.ts * e;
  pid.integral = constrain(pid.integral, -pid.uMax, pid.uMax);

  float u = pid.kp * e + pid.integral + pid.kd * derivada;
  return constrain(u, -pid.uMax, pid.uMax);
}
