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
Ajustadas las constantes de los controladores en el entorno virtual se programó un código el cual integre un controlador discreto para la velocidad de los motores usando la medición entregada por los encoders. Algunos aspectos que se tuvieron en cuenta al programar los código fueron que el controlador debe de tener un limite de salida de 6.6v, además de un ajuste individual de zona muerta para cada motor; y la capacidad para poder dar un respuesta tanto negativa como positiva.

Visualizando la respuesta mediante el serial (el cual permite la digitación y envió de un valor entre -60 a 60) se puedo validar que tanto el controlador como la identificación se realizaron de manera correcta presentado un error de +- 1 rpm entre ambos controladores.

PDD: Se realizó un ajuste al motor 2 puesto que este se encuentra espejado con respecto al motor 1, se tuvo que invertir tanto su señal de control como mediciones para que ambas mediciones coincidan y giren en el mismo sentido ambos motores.

Archivo adjunto: controladores_motores.ino