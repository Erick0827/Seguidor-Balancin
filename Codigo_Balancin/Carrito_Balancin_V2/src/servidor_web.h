/*
  servidor_web.h
  Servidor HTTP de la interfaz de control, en una tarea fijada al núcleo
  NUCLEO_COMUNICACION (junto al WiFi). Entrar desde un dispositivo conectado
  a la red del carrito a http://192.168.4.1

  Rutas:
    GET  /            página (pagina_web.h)
    GET  /datos       estado (incluida la calibración) + muestras nuevas
                      para las gráficas (JSON)
    POST /motores     on=1 enciende, on=0 apaga
    GET  /parametros  constantes actuales de ángulo y giro (JSON)
    POST /parametros  kpa, kda, ref, kpg, kig -> aplica y devuelve las actuales
    GET  /bateria     voltaje de batería actual y rango aceptado (JSON)
    POST /bateria     v -> fija el voltaje de batería (ok = 0 si está fuera de rango)

  Las muestras se entregan una sola vez: con varias páginas abiertas a la
  vez cada una vería solo una parte de los datos.
*/
#pragma once

// Arranca la tarea del servidor. Llamar después de iniciarWiFi().
void iniciarServidorWeb();
