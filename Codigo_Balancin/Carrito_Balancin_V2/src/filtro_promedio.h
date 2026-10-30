/*
  filtro_promedio.h
  Promedio móvil de N_PROMEDIO muestras (buffer circular).
*/
#pragma once

#include "config.h"

struct FiltroPromedio {
  float buffer[N_PROMEDIO];
  float suma;
  int indice;
};

void reiniciarFiltro(FiltroPromedio &f);

// Agrega una muestra y devuelve el promedio actual
float filtrar(FiltroPromedio &f, float x);
