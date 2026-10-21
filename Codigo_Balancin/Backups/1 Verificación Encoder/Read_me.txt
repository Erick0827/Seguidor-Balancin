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
Esta verificación se realizó girando manualmente las ruedas 10 veces y guardando el valor que daba el conteo de los pulsos, para esto se marcó en el chasis y en la rueda una línea que al coincidir se contaba una vuelta completa, de estas mediciones se obtuvieron los siguiente datos:

Motor 1: [vista lateral]

- CPR Horario 1: -4792
- CPR Horario 2: -4792
- CPR Horario 3: -4790
- CPR Horario 4: -4688
- CPR Horario 5: -4818

- CPR Anti-Horario 1: 4977
- CPR Anti-Horario 2: 4916
- CPR Anti-Horario 3: 4874
- CPR Anti-Horario 4: 4841
- CPR Anti-Horario 5: 4896

Motor 2: [vista lateral]

- CPR Horario 1: -4897
- CPR Horario 2: -4952
- CPR Horario 3: -4916
- CPR Horario 4: -4866
- CPR Horario 5: -4922

- CPR Anti-Horario 1: 4864
- CPR Anti-Horario 2: 4883
- CPR Anti-Horario 3: 4937
- CPR Anti-Horario 4: 4828
- CPR Anti-Horario 5: 4915

Posteriormente, se realizó la verificación de la zona muerta de cada motor individualmente, siendo que el motor 1 necesito para que empezara a girar 13 de PWM con una fuente fija de 12.3 v, en cambio el motor 2 se necesitaron 11 de PWM para girar con la misma fuente fija, cabe resaltar esta prueba se hiso a una frecuencia de 1k Hz debido a que a una mayor frecuencia se inducia ruido eléctrico de manera muy notable.

Finalmente, como ultimas validaciones se le aplicó el mismo PWM a cada motor y se revisaba los pulsos por segundo que generaba cada motor, obteniendo una medida de 100 pulsos por segundo con una diferencia de +- 2 pulsos por segundo; seguidamente con el mismo PWM se contaba la cantidad de vueltas completas que este realizaba deteniendo el motor al momento de completar 10 vueltas para hacer una validación con los datos obtenidos al girar la rueda manualmente siendo que los datos coincidían en un CPR de 4900 en promedio para los dos motores. Definido el valor de CPR se procederá con la verificación de los RPM medidos de cada motor.

PDD: Esta prueba se realizó con una cuadratura de x1 (o en otras palabras solo se media el flanco de subida de la Fase A), posteriormente se cambio por una cuadratura de x4 (o en otras palabras se media los flancos de subida y baja tanto de la Fase A como la Fase B), esto con el objetivo de tener una resolución total aproximada de 19600 o 1960 x vuelta. Como nota final, es muy recomendable usar PCNT en lugar de attachinterupt, debido a que el PCNT depende meramente del hardware, en cambio el otro método anteriormente mencionado dependiendo del código, si este se extiende demasiado o presenta una gran cantidad de funciones puede introducir latencia a las mediciones lo cual afecta enormemente a las mediciones de los encoders y por ende a cualquier variable que se quiera medir.

Archivo a Adjuntar: Zona_muerta_motores.ino