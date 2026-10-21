/*
  almacenamiento.h
  Guarda y carga la calibración del MPU en la flash (NVS, Preferences).
  OJO: escribir en la flash detiene momentáneamente ambos núcleos; solo se
  hace con los motores apagados (al terminar la calibración).
*/
#pragma once

struct CalibracionGuardada {
  float biasGiro[3];        // °/s
  float anguloEquilibrio;   // °
};

// false si nunca se ha guardado una calibración
bool cargarCalibracion(CalibracionGuardada &c);

bool guardarCalibracion(const CalibracionGuardada &c);
