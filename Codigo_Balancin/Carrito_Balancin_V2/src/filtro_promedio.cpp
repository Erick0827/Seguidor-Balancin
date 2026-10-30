#include "filtro_promedio.h"

void reiniciarFiltro(FiltroPromedio &f)
{
  for (int i = 0; i < N_PROMEDIO; i++) f.buffer[i] = 0;
  f.suma = 0;
  f.indice = 0;
}

float filtrar(FiltroPromedio &f, float x)
{
  f.suma -= f.buffer[f.indice];
  f.buffer[f.indice] = x;
  f.suma += x;
  f.indice = (f.indice + 1) % N_PROMEDIO;
  return f.suma / N_PROMEDIO;
}
