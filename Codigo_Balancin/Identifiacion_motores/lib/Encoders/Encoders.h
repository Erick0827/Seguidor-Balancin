#pragma once
#include <Arduino.h>
#include "MediaMovil.h"

// Cuenta flancos de subida de la fase A; la fase B da el sentido.
class Encoder {
public:
  bool begin(int pinA, int pinB, uint16_t ppr, uint16_t reduccion,
             uint32_t ventanaMs = 200, bool invertir = false);
  bool actualizar();           // Mide solo si pasó la ventana; true si hubo medición
  void medir();                // Mide ya, con el tiempo real transcurrido
  void setMuestras(uint8_t n) { _mm.setMuestras(n); }  // Media móvil de rpm (1 = sin filtro)

  long  conteo();              // Pulsos acumulados (con signo)
  void  reset();
  float rpm() const    { return _rpm; }      // Filtrada
  float rpmCruda() const { return _rpmCruda; }

private:
  static void IRAM_ATTR isr(void *arg);

  int  _pinA = -1, _pinB = -1;
  uint32_t _pprTotal = 0;
  uint32_t _ventanaUs = 200000;
  uint32_t _tAnterior = 0;     // micros
  int8_t _signo = 1;
  MediaMovil<16> _mm;
  volatile long _conteo = 0;   // Total acumulado
  volatile long _pasos = 0;    // Pulsos de la ventana actual
  float _rpm = 0, _rpmCruda = 0;
  portMUX_TYPE _mux = portMUX_INITIALIZER_UNLOCKED;
};
