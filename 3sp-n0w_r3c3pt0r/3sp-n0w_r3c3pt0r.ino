#include <esp_now.h>
#include <WiFi.h>

const int PIN_RGB = 48; // LED RGB integrado en tu N16R8 estándar

struct EstructuraMensaje {
  char comando; 
};
EstructuraMensaje misDatos;

void alRecibirDatos(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
  memcpy(&misDatos, data, sizeof(misDatos));
  
  // Comparamos caracteres directos de forma segura y eficiente
  if (misDatos.comando == '1') {
    rgbLedWrite(PIN_RGB, 0, 50, 0);   // Línea detectada -> LED en VERDE
  } 
  else if (misDatos.comando == '0') {
    rgbLedWrite(PIN_RGB, 50, 0, 0);   // Buscando línea -> LED en ROJO
  }
}

void setup() {
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) return;
  
  esp_now_register_recv_cb(alRecibirDatos);
}

void loop() {
  // Manejado por interrupciones físicas de la antena
}
