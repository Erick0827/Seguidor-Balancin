/*
  mpu6050.h
  Driver mínimo del MPU6050 por I2C (Wire).
  Configura SMPLRT_DIV, DLPF y rangos según config.h y entrega las
  lecturas en unidades físicas (g y °/s) con el offset del giroscopio quitado.

  Debe iniciarse desde el núcleo de control para que la interrupción
  del I2C quede en ese núcleo.
*/
#pragma once

struct LecturaMPU {
  float acel[3];  // g      (X, Y, Z)
  float giro[3];  // °/s    (X, Y, Z)
};

// Inicia el I2C y configura el sensor. false si no responde.
bool iniciarMPU();

// Promedia n lecturas en reposo para obtener el offset del giroscopio
bool calibrarGiroscopio(int n);

// Offset (bias) del giroscopio que se resta en cada lectura, en °/s
void obtenerOffsetGiro(float offset[3]);
void fijarOffsetGiro(const float offset[3]);

// Lee aceleración y velocidad angular. false si falló el I2C.
bool leerMPU(LecturaMPU &l);
