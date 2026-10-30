Procedimiento:
Posterior a haber obtenido las dos hojas de calculo correspondiente a cada motor, se tiene que tomar el 70% de las ciclos (o 21 ciclos de los 30) y promediarlos para minimizar el error al medirse, de este promedio se crea un promedio adicional para la respuesta con el primer escalón y con el segundo escalón siendo los datos obtenidos en este caso los siguiente:

Motor 1:
Respuesta promedio al primer escalón: 35.24 RPM
Respuesta promedio al segundo escalón: 66.13 RPM
Diferencia en la respuesta: 30.89
Motor 2:
Respuesta promedio al primer escalón: 35.4 RPM
Respuesta promedio al segundo escalón: 67.25 RPM
Diferencia en la respuesta: 31.85

Diferencia de voltaje de entrada (igual para ambos motores): 3v

Obtenido los datos de diferencia de entrada y de salida de cada motor, se creo una hoja de calculo adicional para cada motor para realizar la identificación paramétrica y la sintonización de los controladores; Creada la hoja se insertaron los datos anteriormente obtenidos de los cuales se puede obtener el valor de la ganancia dividiendo el valor de la diferencia de salida entre el de la diferencia de la entrada obteniendo en cada caso:

Ganancia motor 1: 10.29

Ganancia motor 2: 10.61

Seguidamente, se deben obtener los valores de la constante de tiempo Tao y el tiempo muerto, en este apartado es cuando se dará uso al método de los dos puntos general Smith, el cual como su nombre indica se ubican dos puntos en el porcentaje de la respuesta del sistema, en el caso de Smith los puntos P1 y P2 son 28.3% y 63.2% respectivamente, de estos puntos se tiene que obtener un T1 y un T2, siendo el tiempo que tarda el sistema en llegar a ese porcentaje en el cambio de escalón, contado desde el momento en el que aplica el escalón hasta que se llega a dicho porcentaje, en este caso los datos obtenidos fueron:

Motor 1:

P1: 43.98
P2: 54.76

T1: 0.031
T2: 0.052

Motor 2:

P1: 44.41
P2: 55.53

T1: 0.032
T2: 0.054

Teniendo los valores de T1 y T2 para cada motor con las variables del método de Smith, A,B,C y D; -1.5,1.5,1.5 y 0.5 respectivamente se usa la siguiente ecuación para determinar la constante de tiempo Tao y el tiempo muerto:

Tao= (A*T1)+(B*T2)
Tiempo Muerto= (C*T1)+(D*T2)

Valores obtenidos en cada motor:

Motor 1:

Tao: 0.032

Tiempo Muerto: 0.0208

Motor 2:
 
Tao: 0.033

Tiempo Muerto: 0.021

Finalmente con estos valores se haya el valor de los controladores usando la tabla de sintonización de controladores de Ziegler & Nichols, para esto la constante y los tiempos se obtienen de la siguiente manera (para el caso de los controladores PI como en este caso):

Kp: (0.9/K)*(Tao/Tiempo Muerto)

Ti: 3.33*Tiempo Muerto

Ki: Kp/Ti

Datos obtenidos mediante la tabla para cada motor:

Motor 1:

Kp: 0.134

Ki: 1.928

Motor 2:

Kp: 0.13

Ki: 1.822

Obtenidas las constantes del controlador de cada motor se procederá con el 2do paso, la simulación y ajuste manual de los valores de los controladores en el Software de MATLAB-Simulink

Archivos Adjuntos:
- Identificacion_Motor_1.xlsx
- Identificacion_Motor_2.xlsx
- Funcion_Transferencia_y_Controlador_Motor_1.xlsx
- Funcion_Transferencia_y_Controlador_Motor_2.xlsx
