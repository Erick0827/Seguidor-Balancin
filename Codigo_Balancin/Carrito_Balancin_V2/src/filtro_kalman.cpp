#include "filtro_kalman.h"

void configurarKalman(FiltroKalman &k, float qAngulo, float qSesgo, float rMedida)
{
  k.qAngulo = qAngulo;
  k.qSesgo = qSesgo;
  k.rMedida = rMedida;
  reiniciarKalman(k, 0, 0);
}

void reiniciarKalman(FiltroKalman &k, float angulo, float sesgo)
{
  k.angulo = angulo;
  k.sesgo = sesgo;
  k.P[0][0] = k.P[0][1] = k.P[1][0] = k.P[1][1] = 0;
}

void predecirKalman(FiltroKalman &k, float velocidad, float dt)
{
  k.angulo += dt * (velocidad - k.sesgo);

  k.P[0][0] += dt * (dt * k.P[1][1] - k.P[0][1] - k.P[1][0] + k.qAngulo);
  k.P[0][1] -= dt * k.P[1][1];
  k.P[1][0] -= dt * k.P[1][1];
  k.P[1][1] += k.qSesgo * dt;
}

void corregirKalman(FiltroKalman &k, float anguloMedido)
{
  float S = k.P[0][0] + k.rMedida;
  float K0 = k.P[0][0] / S;
  float K1 = k.P[1][0] / S;

  float y = anguloMedido - k.angulo;
  k.angulo += K0 * y;
  k.sesgo  += K1 * y;

  float P00 = k.P[0][0];
  float P01 = k.P[0][1];
  k.P[0][0] -= K0 * P00;
  k.P[0][1] -= K0 * P01;
  k.P[1][0] -= K1 * P00;
  k.P[1][1] -= K1 * P01;
}
