#include "mpu6050.h"
#include <Wire.h>
#include "config.h"

// Registros
const uint8_t REG_SMPLRT_DIV   = 0x19;
const uint8_t REG_CONFIG       = 0x1A;
const uint8_t REG_GYRO_CONFIG  = 0x1B;
const uint8_t REG_ACCEL_CONFIG = 0x1C;
const uint8_t REG_ACCEL_XOUT_H = 0x3B;  // 14 bytes: acel(6), temp(2), giro(6)
const uint8_t REG_PWR_MGMT_1   = 0x6B;
const uint8_t REG_WHO_AM_I     = 0x75;

// Sensibilidad según el rango elegido
const float LSB_POR_G[]   = { 16384.0f, 8192.0f, 4096.0f, 2048.0f };
const float LSB_POR_DPS[] = { 131.0f, 65.5f, 32.8f, 16.4f };

static float offsetGiro[3] = { 0, 0, 0 };

static bool escribirRegistro(uint8_t reg, uint8_t valor)
{
  Wire.beginTransmission(MPU_DIRECCION);
  Wire.write(reg);
  Wire.write(valor);
  return Wire.endTransmission() == 0;
}

static bool leerRegistros(uint8_t reg, uint8_t *datos, uint8_t n)
{
  Wire.beginTransmission(MPU_DIRECCION);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(MPU_DIRECCION, n) != n) return false;
  for (uint8_t i = 0; i < n; i++) datos[i] = Wire.read();
  return true;
}

// Lectura sin quitar offsets
static bool leerCrudo(LecturaMPU &l)
{
  uint8_t b[14];
  if (!leerRegistros(REG_ACCEL_XOUT_H, b, 14)) return false;

  for (int i = 0; i < 3; i++) {
    int16_t a = (int16_t)((b[2 * i] << 8) | b[2 * i + 1]);
    int16_t g = (int16_t)((b[8 + 2 * i] << 8) | b[8 + 2 * i + 1]);
    l.acel[i] = a / LSB_POR_G[MPU_RANGO_ACEL];
    l.giro[i] = g / LSB_POR_DPS[MPU_RANGO_GIRO];
  }
  return true;
}

bool iniciarMPU()
{
  Wire.begin(PIN_MPU_SDA, PIN_MPU_SCL, I2C_FRECUENCIA);
  Wire.setTimeOut(5);  // ms; evita que un fallo del bus bloquee el control

  uint8_t quien = 0;
  if (!leerRegistros(REG_WHO_AM_I, &quien, 1)) return false;
  if (quien != 0x68) {
    // Algunos clones responden 0x70, 0x72 o 0x98 y funcionan igual
    Serial.printf("# MPU: WHO_AM_I = 0x%02X (se esperaba 0x68), se continúa\n", quien);
  }

  bool ok = true;
  ok &= escribirRegistro(REG_PWR_MGMT_1, 0x01);  // despierta, reloj PLL del giroscopio X
  delay(100);
  ok &= escribirRegistro(REG_SMPLRT_DIV, MPU_SMPLRT_DIV);
  ok &= escribirRegistro(REG_CONFIG, MPU_DLPF_CFG & 0x07);
  ok &= escribirRegistro(REG_GYRO_CONFIG, (MPU_RANGO_GIRO & 0x03) << 3);
  ok &= escribirRegistro(REG_ACCEL_CONFIG, (MPU_RANGO_ACEL & 0x03) << 3);
  delay(50);  // deja asentar el DLPF
  return ok;
}

bool calibrarGiroscopio(int n)
{
  float suma[3] = { 0, 0, 0 };
  int validas = 0;
  LecturaMPU l;

  for (int k = 0; k < n; k++) {
    if (leerCrudo(l)) {
      for (int i = 0; i < 3; i++) suma[i] += l.giro[i];
      validas++;
    }
    delay(TS_BASE_MS);
  }
  if (validas < n / 2) return false;

  for (int i = 0; i < 3; i++) offsetGiro[i] = suma[i] / validas;
  return true;
}

void obtenerOffsetGiro(float offset[3])
{
  for (int i = 0; i < 3; i++) offset[i] = offsetGiro[i];
}

void fijarOffsetGiro(const float offset[3])
{
  for (int i = 0; i < 3; i++) offsetGiro[i] = offset[i];
}

bool leerMPU(LecturaMPU &l)
{
  if (!leerCrudo(l)) return false;
  for (int i = 0; i < 3; i++) l.giro[i] -= offsetGiro[i];
  return true;
}
