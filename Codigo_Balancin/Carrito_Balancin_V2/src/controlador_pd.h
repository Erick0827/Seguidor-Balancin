/*
  controlador_pd.h
  PD con la derivada tomada directamente de la medición:
    u = Kp*e + Kd*derivada
  La salida se satura en ±uMax.
*/
#pragma once

struct ControladorPD {
  float kp, kd;
  float uMax;
};

void configurarPD(ControladorPD &pd, float kp, float kd, float uMax);

float calcularPD(const ControladorPD &pd, float e, float derivada);
