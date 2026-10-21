/*
  wifi_red.h
  El ESP32 crea su propia red WiFi (punto de acceso, sin internet) para
  que otros dispositivos se conecten directamente al carrito.
  Nombre, clave, canal e IP en config.h (WIFI_AP_*).
  El stack de WiFi corre en el núcleo 0, sin tocar el control del núcleo 1.
*/
#pragma once

#include <IPAddress.h>

// Crea el punto de acceso. false si no se pudo iniciar.
bool iniciarWiFi();

int clientesConectados();
IPAddress wifiIP();
