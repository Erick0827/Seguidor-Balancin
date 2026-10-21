#include "controlador_pd.h"
#include <Arduino.h>

void configurarPD(ControladorPD &pd, float kp, float kd, float uMax)
{
  pd.kp = kp;
  pd.kd = kd;
  pd.uMax = uMax;
}

float calcularPD(const ControladorPD &pd, float e, float derivada)
{
  float u = pd.kp * e + pd.kd * derivada;
  return constrain(u, -pd.uMax, pd.uMax);
}
