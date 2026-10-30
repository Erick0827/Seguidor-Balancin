#include "wifi_red.h"
#include <WiFi.h>
#include "config.h"

// Corre en la tarea de eventos de Arduino (núcleo 0, ver platformio.ini)
static void alEventoWiFi(arduino_event_id_t evento, arduino_event_info_t info)
{
  switch (evento) {
    case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
      Serial.printf("# WiFi: dispositivo conectado (%d en total)\n", WiFi.softAPgetStationNum());
      break;

    case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
      Serial.printf("# WiFi: dispositivo desconectado (%d en total)\n", WiFi.softAPgetStationNum());
      break;

    default:
      break;
  }
}

bool iniciarWiFi()
{
  WiFi.onEvent(alEventoWiFi);
  WiFi.mode(WIFI_AP);

  bool ok = WiFi.softAPConfig(WIFI_AP_IP, WIFI_AP_IP, WIFI_AP_MASCARA) &&
            WiFi.softAP(WIFI_AP_SSID, WIFI_AP_CLAVE, WIFI_AP_CANAL, false, WIFI_AP_MAX_CLIENTES);

  if (ok) {
    Serial.printf("# WiFi: red \"%s\" creada, IP del carrito: %s\n",
                  WIFI_AP_SSID, WiFi.softAPIP().toString().c_str());
  } else {
    Serial.println("# WiFi: no se pudo crear el punto de acceso");
  }
  return ok;
}

int clientesConectados()
{
  return WiFi.softAPgetStationNum();
}

IPAddress wifiIP()
{
  return WiFi.softAPIP();
}
