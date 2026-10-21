/*
  encoder.h
  Lectura de encoders en cuadratura x4 con el periférico PCNT del ESP32-S3.
*/
#pragma once

#include "driver/pulse_cnt.h"

// Crea y arranca una unidad PCNT para un encoder con fases A y B.
// Sentido positivo: la fase A sube mientras la fase B está en alto.
pcnt_unit_handle_t crearEncoderPCNT(int pinA, int pinB);

// Conteo acumulado del encoder
long leerConteo(pcnt_unit_handle_t unidad);
