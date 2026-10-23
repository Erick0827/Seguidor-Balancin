/*
  controlador_pid.h
  PID posicional con la derivada tomada directamente de una medición:
    I[k] = I[k-1] + Ki*Ts*e[k]          (limitada a ±uMax, anti-windup)
    u[k] = Kp*e[k] + I[k] + Kd*derivada  (saturada a ±uMax)
  Con Ki = 0 es un PD (idéntico al controlador_pd anterior).
  Se usa en el controlador de ángulo y en el de desplazamiento.
*/
#pragma once

struct ControladorPID {
  float kp, ki, kd, ts;
  float uMax;
  float integral;      // estado
};

void configurarPID(ControladorPID &pid, float kp, float ki, float kd, float ts, float uMax);
void reiniciarPID(ControladorPID &pid);
float calcularPID(ControladorPID &pid, float e, float derivada);
