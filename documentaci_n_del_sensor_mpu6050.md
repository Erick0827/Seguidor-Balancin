# Funcionamiento e Implementación del Sensor MPU6050 en el Sistema Balancín

Este documento describe el principio de funcionamiento del sensor inercial **MPU6050** (Acelerómetro + Giroscopio) y su programación en **Arduino** para obtener el ángulo de inclinación en tiempo real, fundamental para el control de equilibrio del proyecto **Seguidor-Balancín**.

---

## 1. ¿Qué es el MPU6050 y cómo funciona?

El **MPU6050** es una Unidad de Medición Inercial (IMU) de 6 grados de libertad (6-DOF) que combina dos sensores microelectromecánicos (MEMS) en un solo chip:

1. **Acelerómetro de 3 ejes ($X, Y, Z$):** Mide la aceleración lineal y la fuerza de gravedad. Permite calcular el ángulo de inclinación absoluto mediante trigonometría, pero es muy sensible al ruido y a las vibraciones mecánicas del motor.
2. **Giroscopio de 3 ejes ($X, Y, Z$):** Mide la velocidad angular (qué tan rápido gira el balancín en grados por segundo, $^\circ/\text{s}$). Es muy preciso en movimientos rápidos, pero sufre de *deriva* (drift) con el tiempo si se integra por sí solo.

### Fusión de Sensores (Filtro Complementario)
Para obtener un ángulo limpio y confiable para el control del balancín, combinamos ambos sensores mediante un **Filtro Complementario**:
* Calculamos el ángulo con el acelerómetro:
  $$\theta_{acc} = \arctan\left(\frac{A_y}{\sqrt{A_x^2 + A_z^2}}\right) \cdot \frac{180}{\pi}$$
* Integramos la velocidad angular del giroscopio en cada ciclo de tiempo ($\Delta t$):
  $$\theta_{gyro} = \theta_{anterior} + \omega_x \cdot \Delta t$$
* Combinamos ambas lecturas dando un $98\%$ de peso al giroscopio (rápido y sin ruido) y un $2\%$ al acelerómetro (corrige la deriva a largo plazo):
  $$\theta_{final} = 0.98 \cdot (\theta_{anterior} + \omega_x \cdot \Delta t) + 0.02 \cdot (\theta_{acc})$$

---

## 2. Conexión con Arduino (Protocolo $\text{I}^2\text{C}$)

El sensor se comunica con el Arduino utilizando el bus $\text{I}^2\text{C}$, lo que requiere únicamente 4 cables:

| Pin MPU6050 | Pin Arduino (Uno / Nano) | Pin Arduino (Mega) | Descripción |
| :---: | :---: | :---: | :--- |
| **VCC** | 5V (o 3.3V) | 5V | Alimentación del módulo |
| **GND** | GND | GND | Tierra común |
| **SCL** | A5 | Pin 21 | Reloj de comunicación ($\text{I}^2\text{C}$) |
| **SDA** | A4 | Pin 20 | Datos de comunicación ($\text{I}^2\text{C}$) |

---

## 3. Código de Arduino para Lectura de Ángulo

Este código utiliza la librería estándar `Wire.h` (incluida por defecto en el IDE de Arduino) para comunicarse directamente con los registros del MPU6050 sin depender de librerías externas complejas, garantizando una lectura rápida ideal para un lazo de control PID.

```cpp
#include <Wire.h>

// Dirección I2C del MPU6050
const int MPU_ADDR = 0x68;

// Variables para almacenar los datos crudos (raw) del sensor
int16_t AcX, AcY, AcZ, Tmp, GyX, GyY, GyZ;

// Variables para el cálculo de ángulos
float angulo_acc_x, angulo_acc_y;
float gx_deg, gy_deg;
float angulo_x = 0.0, angulo_y = 0.0;

// Variables de tiempo para la integración del giroscopio
unsigned long tiempo_previo;
float dt;

void setup() {
  Serial.begin(115200);
  Wire.begin();
  
  // Despertar el MPU6050 (sale del modo suspensión)
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B); // Registro PWR_MGMT_1
  Wire.write(0);    // Poner en 0 para activar el sensor
  Wire.endTransmission(true);

  tiempo_previo = millis();
  Serial.println("MPU6050 inicializado correctamente.");
}

void loop() {
  // 1. Solicitar los 14 bytes de registros de datos al MPU6050
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B); // Empezar en el registro 0x3B (ACCEL_XOUT_H)
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 14, true);

  // 2. Leer los datos del acelerómetro, temperatura y giroscopio
  AcX = Wire.read() << 8 | Wire.read(); // Eje X del acelerómetro
  AcY = Wire.read() << 8 | Wire.read(); // Eje Y del acelerómetro
  AcZ = Wire.read() << 8 | Wire.read(); // Eje Z del acelerómetro
  Tmp = Wire.read() << 8 | Wire.read(); // Temperatura interna
  GyX = Wire.read() << 8 | Wire.read(); // Eje X del giroscopio
  GyY = Wire.read() << 8 | Wire.read(); // Eje Y del giroscopio
  GyZ = Wire.read() << 8 | Wire.read(); // Eje Z del giroscopio

  // 3. Calcular el diferencial de tiempo (dt) en segundos
  unsigned long tiempo_actual = millis();
  dt = (tiempo_actual - tiempo_previo) / 1000.0;
  tiempo_previo = tiempo_actual;

  // 4. Calcular ángulos en grados con el Acelerómetro (Roll y Pitch)
  angulo_acc_x = atan(AcY / sqrt(pow(AcX, 2) + pow(AcZ, 2))) * (180.0 / PI);
  angulo_acc_y = atan(-1 * AcX / sqrt(pow(AcY, 2) + pow(AcZ, 2))) * (180.0 / PI);

  // 5. Convertir lecturas del Giroscopio a grados/segundo (escala por defecto: +-250 deg/s -> factor 131.0)
  gx_deg = GyX / 131.0;
  gy_deg = GyY / 131.0;

  // 6. Aplicar Filtro Complementario para obtener el ángulo real del balancín
  angulo_x = 0.98 * (angulo_x + gx_deg * dt) + 0.02 * angulo_acc_x;
  angulo_y = 0.98 * (angulo_y + gy_deg * dt) + 0.02 * angulo_acc_y;

  // 7. Mostrar resultados en el Monitor Serie / Serial Plotter
  Serial.print("Angulo_X:");
  Serial.print(angulo_x);
  Serial.print("\tAngulo_Y:");
  Serial.println(angulo_y);

  delay(10); // Frecuencia de muestreo aproximada de 100 Hz
}
```

---

## 4. Calibración y Recomendaciones de Montaje

* **Orientación del sensor:** Monta el módulo MPU6050 lo más cerca posible del eje de giro del balancín para reducir las fuerzas centrífugas que puedan afectar las lecturas del acelerómetro.
* **Velocidad de comunicación:** El código utiliza `Serial.begin(115200)`. Asegúrate de configurar la misma velocidad (115200 baudios) en el Monitor Serie o Serial Plotter de tu Arduino IDE.