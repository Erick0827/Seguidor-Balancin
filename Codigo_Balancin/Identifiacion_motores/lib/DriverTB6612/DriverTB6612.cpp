#include "DriverTB6612.h"

void DriverTB6612::begin(const Motor &a, const Motor &b, int pinStby,
                         uint32_t frecuencia, uint8_t resolucion) {
  _a = a;
  _b = b;
  _pinStby = pinStby;
  _maxDuty = (1 << resolucion) - 1;

  pinMode(_pinStby, OUTPUT);
  digitalWrite(_pinStby, LOW);

  for (const Motor *m : {&_a, &_b}) {
    pinMode(m->pinIn1, OUTPUT);
    pinMode(m->pinIn2, OUTPUT);
    digitalWrite(m->pinIn1, LOW);
    digitalWrite(m->pinIn2, LOW);
    ledcSetup(m->canal, frecuencia, resolucion);
    ledcAttachPin(m->pinPwm, m->canal);
    ledcWrite(m->canal, 0);
  }
  _habilitado = false;
}

void DriverTB6612::habilitar(bool on) {
  if (!on) parar();
  digitalWrite(_pinStby, on ? HIGH : LOW);
  _habilitado = on;
}

void DriverTB6612::mover(float uA, float uB) {
  _dutyA = aplicar(_a, uA);
  _dutyB = aplicar(_b, uB);
}

void DriverTB6612::parar() {
  _dutyA = aplicar(_a, 0);
  _dutyB = aplicar(_b, 0);
}

// IN1/IN2: 10 = adelante, 01 = atrás, 00 = libre
int DriverTB6612::aplicar(const Motor &m, float u) {
  if (m.invertir) u = -u;
  float mag = fabsf(u);
  if (mag < 0.5f) {
    digitalWrite(m.pinIn1, LOW);
    digitalWrite(m.pinIn2, LOW);
    ledcWrite(m.canal, 0);
    return 0;
  }

  // Compensación de zona muerta: [0, 255] -> [zonaMuerta, 255]
  int duty = _zonaMuerta + (int)(mag * (_maxDuty - _zonaMuerta) / 255.0f + 0.5f);
  duty = constrain(duty, 0, _maxDuty);

  digitalWrite(m.pinIn1, u > 0 ? HIGH : LOW);
  digitalWrite(m.pinIn2, u > 0 ? LOW : HIGH);
  ledcWrite(m.canal, duty);
  return u > 0 ? duty : -duty;
}
