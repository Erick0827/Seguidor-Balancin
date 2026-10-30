/*
  calibracion.h
  Rutina del botón de calibración (se ejecuta en la tarea de control):
    1. Solo arranca con los motores apagados (si no, se rechaza).
    2. Espera CAL_ESPERA_MS para soltar el botón y estabilizar el carrito
       en su punto de equilibrio (la web muestra un indicador parpadeando).
    3. Toma CAL_MUESTRAS lecturas (una cada TS_BASE_MS) y acumula
       giroscopio y acelerómetro.
    4. Si la desviación estándar del giroscopio supera CAL_DESVIACION_MAX
       en algún eje, cancela e indica que hay que repetir.
    5. Bias del giroscopio = promedio; ángulo de equilibrio = ángulo del
       acelerómetro promediado.
    6. Aplica ambos y los guarda en la flash.
*/
#pragma once

enum EstadoCalibracion {
  CAL_INACTIVA = 0,
  CAL_ESPERA,            // esperando que se suelte el botón / se estabilice
  CAL_MUESTREO,          // tomando muestras
  CAL_OK,                // terminó y se guardó
  CAL_ERROR_MOVIMIENTO,  // el carrito se movió: repetir
  CAL_ERROR_MPU,         // fallaron demasiadas lecturas
  CAL_ERROR_FLASH,       // se aplicó pero no se pudo guardar
  CAL_RECHAZADA          // se pulsó con los motores encendidos
};

// Llamar cada TS_BASE_MS desde la tarea de control, después de actualizarIMU().
// Devuelve true en el ciclo en que la calibración termina y se aplica;
// en ese caso anguloEquilibrio trae el nuevo ángulo (°).
bool pasoCalibracion(bool pulsado, bool motoresHabilitados, bool lecturaOk,
                     float &anguloEquilibrio);

// true mientras espera o toma muestras (los motores deben seguir apagados)
bool calibrando();

EstadoCalibracion estadoCalibracion();

// Datos de la última calibración terminada (para mostrarlos)
float ultimoAnguloCalibrado();
float ultimaDesviacionGiro();   // °/s, la mayor de los tres ejes
