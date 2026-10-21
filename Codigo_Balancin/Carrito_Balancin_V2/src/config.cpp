#include "config.h"

// Voltaje actual de la batería (actualízalo según el nivel de carga)
float V_BATERIA = 8.1;

// Zona muerta: PWM mínimo de arranque de cada motor (13 y 11 de 255)
// convertido a voltios con la batería actual.
const float V_ZONA_MUERTA_1 = (13.0f/255.0f)*12.3f;
const float V_ZONA_MUERTA_2 = (11.0f/255.0f)*12.3f;

