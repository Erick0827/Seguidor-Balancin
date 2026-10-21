Materiales:
- ESP32 S3 N16R8
- Puente H TB6612TNG
- Motores JGA25-371 De 130 RPM con Encoder de efecto Hall o magnético.
- Fuente de voltaje fija de 12.3 v a 1.5 Amp
- Conversor DC/DC Reductor 0-24v 5amp (Fijado a una salida de 5v Para alimentar a la ESP32 S3)

Conexiones:
TB6612:
- VMOT: 12v de la Fuente fija
- VCC: 3.3v provenientes de la salida de la ESP32 S3
- GND: GND 
- AO1: M1+
- AO2: M1-
- BO2: M2-
- B01: M2+
- GND: GND
- PWMA: GPIO4
- AIN2: GPIO5
- AIN1: GPIO6
- STBY: GPIO7
- BIN1: GPIO15
- BIN2: GPIO16
- PWMB: GPIO17
- GND: GND

PRIMER MOTOR:
- M1+: AO1
- GND: GND
- FASE A: GPIO10
- FASE B: GPIO9
- 3.3v: 3.3v provenientes de la salida de la ESP32 S3
- M1-: AO2

SEGUNDO MOTOR:
- M2+: BO1
- GND: GND
- FASE A: GPIO18
- FASE B: GPIO8
- 3.3v: 3.3v provenientes de la salida de la ESP32 S3
- M2-: BO2

Procedimiento:
En esta verificación se realizó en base a la verificación del encoder anteriormente ejecutada, en este apartado se midió la velocidad de manera mas asertiva usando el CPR anteriormente medido y validado, la formula usada para medir los rpm fue la siguiente:

RPM = (Diferencia de la lectura [lectura actual - lectura anterior]) x 60 / (CPR x (Tiempo de muestreo en segundos))

Aplicada esta formula se pudo visualizar diferencian mas notables en cada motor siendo que a pesar de que el motor 2 gira con menos PWM no alcanza la misma velocidad tope con el máximo PWM (255) que el motor 1 siendo una diferencia de +- 4 rpm.

Finalmente usando el método de medición aplicado en esta sección, se procederá con la identificación paramétrica de cada motor individualmente.

PDD: Se recomienda comparar los RPM obtenidos de este método con los dados por el fabricante siendo que en esta verificación se obtuvo una diferencia de aproximadamente un 9% con respecto a los dados en la hoja de datos de los motores, siendo el caso para ambos motores de igual referencia.

Archivo a Adjuntar: Medicion_de_rpm.ino 