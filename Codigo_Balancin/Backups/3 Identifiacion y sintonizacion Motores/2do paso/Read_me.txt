Procedimiento:
Primeramente con los valores obtenidos del paso anterior, ganancia, constante de tiempo tao y tiempo muerto, de cada uno de los motores, se creo un entorno en MATLAB-Simulink para simular la respuesta y ajustar los valores de los controladores obtenidos mediante la tabla de sintonización de controladores de Ziegler & Nichols.

Los bloques usados fueron para cada caso:
- Bloque de Step (para establecer el set point)
- Comparador o bloque se suma (usado en +- para hacer la comparación del set point con el valor obtenido de la respuesta del sistema)
- Bloque controlador PID (para establecer las constantes del controlador y hacer el ajuste)
- Transfer function (en su forma de primer orden)
- Transfer Dalay (para agregar el tiempo muerto)
- Scope (para visualizar la respuesta del sistema)

Ubicados los bloques y conectados entre si, con el bloque de Transfer delay conectado al scope y al comparador para ajustar los valores del controlador, se establece un set point en el bloque de step con un valor inicial de 0 y un final de 60, además de un step time de 0.

Importante resaltar que el scope está configurado para tener un sample time o tiempo de muestreo de 0.01 o los 10ms usados anteriormente para la identificación del sistema, adicionalmente la simulación tiene un sample time de 1.

Terminados los ajustes se simula la respuesta del sistema y se visualiza esta misma en caso de tener sobre impulso mayor al 5% del valor del set point se deben ajustar los valores de las constantes del controlador hasta obtener un sobre impulso menor al 5% o prácticamente nulo además de un tiempo de estabilización menor a 4 veces la constante de tiempo tao.

Acortando el procedimiento se realizó un ajuste manual de las contantes de los controladores mediante multiplicadores para mantener el valor original y únicamente agregar un multiplicador que lo reduzca o aumente según sea el caso, para este caso las constantes de los controladores de los motores quedaron de la siguiente manera:

Motor 1:

Kp: 0.134*0.545 = 0.073

Ki: 1.928*1.12 = 2.15

Motor 2:

Kp: 0.13*0.54 = 0.07

Ki: 1.822*1.1 = 2

Con estos ajustes la respuesta mejoró en gran medida con respecto a la anterior puesto que la obtenida originalmente presentaba un tiempo de establecimiento mayor a 4 veces la constante de tiempo tao y un sobre impulso mayor al 20%, por otro lado la respuesta obtenida después de ajustar las constantes cumplen con los criterios anteriormente mencionados.

Ajustadas las constantes de manera simulada, se procederá a hacer pruebas en el montaje físico para validar el efectividad tanto de la identificación como la del controlador.

Archivo adjunto: Sintonisacion_controladores_motores.slx