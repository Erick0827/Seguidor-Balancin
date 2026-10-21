/*
  imu.h
  Estimación de la orientación del carrito a partir del MPU6050:
    - ángulo de inclinación (filtro complementario acelerómetro + giroscopio)
    - velocidad de caída (giroscopio en el eje de las ruedas)
    - velocidad de giro (giroscopio en el eje vertical, promediada por ventana)
  Los ejes y signos se ajustan en config.h (EJE_RUEDAS, EJE_VERTICAL, SIGNO_*).
*/
#pragma once

#include "mpu6050.h"

// Inicia el MPU y fija el ángulo inicial con el acelerómetro.
// biasGiro: bias guardado del giroscopio (°/s); si es NULL se calibra
// promediando muestras (el carrito debe estar quieto ~1 s).
// false si el MPU no responde.
bool iniciarIMU(const float *biasGiro);

// Lee el MPU y actualiza las estimaciones. Llamar cada TS_BASE_MS.
// false si la lectura falló (se conservan los últimos valores).
bool actualizarIMU();

// Última lectura válida (giroscopio ya sin bias)
const LecturaMPU &ultimaLecturaIMU();

// Ángulo de inclinación que corresponde a una aceleración (g), en °
float anguloDesdeAcelerometro(const float acel[3]);

// Reinicia el filtro complementario en un ángulo dado
void fijarAnguloIMU(float angulo);

float anguloInclinacion();   // °
float velocidadCaida();      // °/s

// Promedio de la velocidad de giro desde la llamada anterior (°/s),
// para usarla en el lazo de giro sin aliasing. Reinicia la ventana.
float tomarVelocidadGiro();
