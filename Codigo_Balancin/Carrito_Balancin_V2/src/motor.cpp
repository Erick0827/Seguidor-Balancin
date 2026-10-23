#include "motor.h"
#include "config.h"
#include "encoder.h"

Motor motor1;
Motor motor2;

static void configurarMotor(Motor &m, int pinPWM, int pinIN1, int pinIN2, int sentido,
                            int pinEncA, int pinEncB, float kp, float ki, float vZonaMuerta)
{
  m.pinPWM = pinPWM;
  m.pinIN1 = pinIN1;
  m.pinIN2 = pinIN2;
  m.sentido = sentido;
  m.vZonaMuerta = vZonaMuerta;

  pinMode(m.pinIN1, OUTPUT);
  pinMode(m.pinIN2, OUTPUT);
  ledcAttach(m.pinPWM, PWM_FREQ, PWM_RES);

  m.encoder = crearEncoderPCNT(pinEncA, pinEncB);
  m.conteoAnterior = 0;
  configurarPI(m.pi, kp, ki, TS);
  reiniciarFiltro(m.filtro);
  m.rpm = 0;
  m.pwm = 0;
}

void inicializarMotores()
{
  pinMode(PIN_STBY, OUTPUT);

  configurarMotor(motor1, PIN_PWMA, PIN_AIN1, PIN_AIN2, SENTIDO_MOTOR_1,
                  ENC1_A, ENC1_B, KP_1, KI_1, V_ZONA_MUERTA_1);
  configurarMotor(motor2, PIN_PWMB, PIN_BIN1, PIN_BIN2, SENTIDO_MOTOR_2,
                  ENC2_A, ENC2_B, KP_2, KI_2, V_ZONA_MUERTA_2);
  aplicarVoltaje(motor1, 0);
  aplicarVoltaje(motor2, 0);

  digitalWrite(PIN_STBY, HIGH);  // Habilita el TB6612
}

void aplicarVoltaje(Motor &m, float u)
{
  float vFisico = u * m.sentido;

  int pwm = (int)(fabsf(vFisico) / V_BATERIA * 255.0f);
  pwm = constrain(pwm, 0, 255);

  if (pwm == 0) {
    digitalWrite(m.pinIN1, LOW);
    digitalWrite(m.pinIN2, LOW);
  } else if (vFisico > 0) {
    digitalWrite(m.pinIN1, HIGH);
    digitalWrite(m.pinIN2, LOW);
  } else {
    digitalWrite(m.pinIN1, LOW);
    digitalWrite(m.pinIN2, HIGH);
  }
  ledcWrite(m.pinPWM, pwm);

  // PWM reportado en el mismo sentido que la referencia
  m.pwm = (u >= 0) ? pwm : -pwm;
}

void medirRPM(Motor &m)
{
  long conteo = leerConteo(m.encoder);
  long delta = conteo - m.conteoAnterior;
  m.conteoAnterior = conteo;

  float rpm = m.sentido * (delta * 60.0f) / (CPR * TS);

  if (USAR_PROMEDIO_MOVIL) rpm = filtrar(m.filtro, rpm);

  m.rpm = rpm;
}

long cuentasConSigno(const Motor &m)
{
  return m.sentido * m.conteoAnterior;
}

void controlarVelocidad(Motor &m, float ref, bool reiniciar)
{
  if (reiniciar) {
    reiniciarPI(m.pi);
    aplicarVoltaje(m, 0);
    return;
  }

  float uMax = min(V_MAX_CONTROL, V_BATERIA);
  float u = calcularPI(m.pi, ref - m.rpm, uMax);

  // La zona muerta se compensa fuera del estado del PI, así el integrador
  // trabaja sobre la planta "linealizada"
  float uAplicado = u;
  if (u > 0)      uAplicado = u + m.vZonaMuerta;
  else if (u < 0) uAplicado = u - m.vZonaMuerta;
  uAplicado = constrain(uAplicado, -uMax, uMax);

  aplicarVoltaje(m, uAplicado);
}
