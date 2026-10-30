/*
  filtro_kalman.h
  Filtro de Kalman de 2 estados para un ángulo de inclinación:
    x = [ángulo (°), sesgo del giroscopio (°/s)]

  Predicción (con la velocidad angular medida w, en °/s):
    ángulo = ángulo + dt*(w - sesgo)
    P = F P F' + Q*dt,   F = [1 -dt; 0 1],   Q = diag(Q_angulo, Q_sesgo)

  Corrección (con el ángulo del acelerómetro z, en °):
    y = z - ángulo,  S = P00 + R,  K = [P00; P10] / S
    x = x + K*y,     P = (I - K H) P,   H = [1 0]
*/
#pragma once

struct FiltroKalman {
  float qAngulo, qSesgo, rMedida;  // parámetros
  float angulo, sesgo;             // estado
  float P[2][2];                   // covarianza del error
};

void configurarKalman(FiltroKalman &k, float qAngulo, float qSesgo, float rMedida);

// Fija el estado y borra la covarianza (P = 0)
void reiniciarKalman(FiltroKalman &k, float angulo, float sesgo);

void predecirKalman(FiltroKalman &k, float velocidad, float dt);
void corregirKalman(FiltroKalman &k, float anguloMedido);
