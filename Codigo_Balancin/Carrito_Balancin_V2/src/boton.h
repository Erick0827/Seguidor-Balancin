/*
  boton.h
  Pulsador de calibración (PIN_BOTON_CALIBRAR a GND, pull-up interno).
  Se lee por sondeo desde la tarea de control, con antirrebote.
*/
#pragma once

void iniciarBoton();

// Llamar cada TS_BASE_MS. true una sola vez por cada pulsación.
bool botonPulsado();
