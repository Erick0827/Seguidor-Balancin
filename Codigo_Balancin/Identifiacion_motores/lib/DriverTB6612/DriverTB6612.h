#pragma once
#include <Arduino.h>

// Driver TB6612FNG para dos motores. Mando con signo en [-255, +255].
class DriverTB6612 {
public:
  struct Motor {
    int pinPwm, pinIn1, pinIn2;
    uint8_t canal;   // Canal LEDC
    bool invertir;
  };

  void begin(const Motor &a, const Motor &b, int pinStby,
             uint32_t frecuencia = 20000, uint8_t resolucion = 8);

  void habilitar(bool on);                 // STBY
  void mover(float uA, float uB);          // Aplica el mando a cada motor
  void parar();                            // PWM a 0 y motores en libre
  void setZonaMuerta(uint8_t zm) { _zonaMuerta = zm; }
  uint8_t zonaMuerta() const     { return _zonaMuerta; }
  void setInvertir(uint8_t motor, bool inv) { (motor == 1 ? _a : _b).invertir = inv; }  // motor 1 = A, 2 = B
  bool invertido(uint8_t motor) const       { return (motor == 1 ? _a : _b).invertir; }

  bool  habilitado() const { return _habilitado; }
  int   dutyA() const      { return _dutyA; }   // PWM real con signo
  int   dutyB() const      { return _dutyB; }

private:
  int  aplicar(const Motor &m, float u);

  Motor _a{}, _b{};
  int   _pinStby = -1;
  int   _maxDuty = 255;
  volatile uint8_t _zonaMuerta = 0;
  bool  _habilitado = false;
  int   _dutyA = 0, _dutyB = 0;
};
