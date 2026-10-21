#include "Encoders.h"

bool Encoder::begin(int pinA, int pinB, uint16_t ppr, uint16_t reduccion,
                    uint32_t ventanaMs, bool invertir) {
  if (pinA < 0 || pinB < 0 || ppr == 0 || reduccion == 0) return false;
  _pinA = pinA;
  _pinB = pinB;
  _pprTotal = (uint32_t)ppr * reduccion;
  _ventanaUs = ventanaMs * 1000UL;
  _signo = invertir ? -1 : 1;
  pinMode(_pinA, INPUT_PULLUP);
  pinMode(_pinB, INPUT_PULLUP);
  _tAnterior = micros();
  attachInterruptArg(digitalPinToInterrupt(_pinA), isr, this, RISING);
  return true;
}

void IRAM_ATTR Encoder::isr(void *arg) {
  Encoder *e = static_cast<Encoder *>(arg);
  int8_t paso = digitalRead(e->_pinB) ? e->_signo : -e->_signo;
  portENTER_CRITICAL_ISR(&e->_mux);
  e->_pasos += paso;
  e->_conteo += paso;
  portEXIT_CRITICAL_ISR(&e->_mux);
}

bool Encoder::actualizar() {
  if (micros() - _tAnterior < _ventanaUs) return false;
  medir();
  return true;
}

void Encoder::medir() {
  uint32_t ahora = micros();
  uint32_t transcurrido = ahora - _tAnterior;
  if (transcurrido == 0) return;

  portENTER_CRITICAL(&_mux);
  long pulsos = _pasos;
  _pasos = 0;
  portEXIT_CRITICAL(&_mux);

  _rpmCruda = pulsos * (60.0e6f / (_pprTotal * (float)transcurrido));
  _rpm = _mm.agregar(_rpmCruda);
  _tAnterior = ahora;
}

long Encoder::conteo() {
  portENTER_CRITICAL(&_mux);
  long c = _conteo;
  portEXIT_CRITICAL(&_mux);
  return c;
}

void Encoder::reset() {
  portENTER_CRITICAL(&_mux);
  _conteo = 0;
  _pasos = 0;
  portEXIT_CRITICAL(&_mux);
  _rpm = 0;
  _rpmCruda = 0;
  _mm.reset();
  _tAnterior = micros();
}
