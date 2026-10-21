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
En este apartado usando los métodos anteriormente verificados y validados, se realizará la identificación individual de cada motor, usando el método general de los dos puntos específicamente el Smith para su identificación paramétrica y la tabla de sintonización de controladores de Ziegler & Nichols para el controlador PI que se pretende aplicar.

Primeramente, se programó un código en base a los anteriormente programados para la medición de los rpm, es importante resaltar un apartado clave en esta identificación, debido a que se hace uso de una fuente fija de 12.3v y posteriormente se cambiará por una batería de 7.4v, la identificación que se realizará sera en función del voltaje de entrada y los rpm medidos por el encoder, de esta forma podemos hacer uso de un artificio matemático util para hacer la conversion del voltaje a PWM independientemente de la fuente usada, la formula utilizada es la siguiente:

V = (PWM/255)*V_Fuente [1]

Reemplazando con la fuente fija y dejando en función del PWM:

PWM = (V/12.3)*255 [2]

Dando uso de la ecuación 2, sabemos que PWM le tiene que aplicar el puente H al motor, por lo mencionado con anterioridad, la identificación al hacerse en función del voltaje y los rpm, es necesario hacer esta conversión.

Seguidamente de tener deducida la ecuación 2, se establecen dos escalones fijos los cuales se aplicarán a los motores, para este caso en particular se establecieron de 3.6v el primer escalón y de 6.6v el segundo escalón, correspondiendo a una aproximación de un número entero de PWM (aproximación importante para que el actuador funcione de manera correcta y presente el código de identificación) de 75 y 136 respectivamente.

Este procedimiento está planteado para tomar 200 muestras aplicando el primer escalón y registrando el valor obtenido por el sensor con un tiempo de muestreo de 10ms, pasadas las 200 muestras se aplica el segundo escalón durante otras 200 muestras con el mismo tiempo de muestreo
para un total de 400 muestras, esto se repite durante 30 ciclos siendo que cuando se termina un ciclo el actuador se apaga y se espera durante 1s para realizar el siguiente ciclo.

A su ves que los ciclos terminan son enviados por medio del serial a un código en python el cual toma los datos del serial y los imprime en un archivo .xlsx o de hoja de calculo. Importante mencionar que el código de python está programado para ejecutarse con el microcontrolador conectado, de lo contrario imprimirá un error en consola, por lo mismo el código del microcontrolador al ser energizado tiene un tiempo de espera de 4 para comenzar a medir e imprimir los datos por el serial.

Una ves obtenidos los archivos de hoja de calculo de cada motor se procede con el 1er paso para la sintonización de los controladores.

PDD: Por temas de practicidad al implementar el controlador con la batería se seleccionó que el máximo de voltaje que pueda entregar el controlador sea de 6.6v siendo el voltaje mínimo cuando la batería se encuentra descargada, permitiendo que el controlador esté siempre en funcionamiento aunque la batería le quede poca carga.

Archivo Adjunto: Identificacion_motores.ino