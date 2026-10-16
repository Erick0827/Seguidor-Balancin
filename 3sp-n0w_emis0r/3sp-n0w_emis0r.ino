#include <esp_now.h>
#include <WiFi.h>
#include <WebServer.h>

const int PIN_RGB_LOCAL = 48; 
uint8_t macReceptor[] = {0xE0, 0x72, 0xA1, 0xD4, 0x0F, 0x5C}; // MAC N16R8

// Red Wi-Fi que creará la GOOUUU
const char* ssid = "Telemetria_ESP32_S3";
const char* password = "123456788"; // Mínimo 8 caracteres

WebServer server(80); // Servidor web en el puerto estándar 80

struct EstructuraMensaje {
  char comando; 
};
EstructuraMensaje misDatos;

// Variables de telemetría
unsigned long tiempoSegundos = 0;
int codigoEstado = 0;    
int velocidadMotor = 0;  

// Página HTML con el cronómetro de alta precisión y actualización por AJAX
const char HTML_PAGINA[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="es">
<head>
    <meta charset="UTF-8">
    <title>Telemetría Inalámbrica Wi-Fi</title>
    <style>
        body { font-family: Arial, sans-serif; background: #1e1e24; color: #fff; text-align: center; padding: 20px; }
        .container { max-width: 600px; margin: auto; background: #2a2a35; padding: 20px; border-radius: 10px; box-shadow: 0 4px 10px rgba(0,0,0,0.5); }
        .data-card { font-size: 24px; margin: 15px 0; padding: 15px; border-left: 5px solid #00adb5; background: #323242; text-align: left; display: flex; justify-content: space-between; align-items: center; }
        .status-ok { color: #111; background: #00f5d4; font-weight: bold; padding: 5px 12px; border-radius: 5px; }
        .status-wait { color: #fff; background: #ff577f; font-weight: bold; padding: 5px 12px; border-radius: 5px; }
        .time-value { font-family: 'Courier New', Courier, monospace; font-weight: bold; color: #00adb5; font-size: 28px; }
    </style>
</head>
<body>
<div class="container">
    <h2>📡 Telemetría Wi-Fi en Tiempo Real</h2>
    <p>Conectado directamente a la GOOUUU vía Wi-Fi</p>
    <div class="data-card">⏱️ Tiempo Actividad: <span id="txtTiempo" class="time-value">0.00</span> seg</div>
    <div class="data-card">🤖 Estado Sensor: <span id="txtEstado" class="status-wait">BUSCANDO...</span></div>
    <div class="data-card">⚙️ Velocidad Motor: <span id="txtVelocidad" style="font-weight:bold;">0</span> / 255</div>
</div>

<script>
let tiempoBaseESP = 0;
let tUltimoPaquete = performance.now();

function clock() {
    let tiempoTranscurridoLaptop = (performance.now() - tUltimoPaquete) / 1000;
    document.getElementById('txtTiempo').innerText = (tiempoBaseESP + tiempoTranscurridoLaptop).toFixed(2);
    requestAnimationFrame(clock);
}
requestAnimationFrame(clock);

// Peticiones automáticas de fondo para traer los datos del ESP32 cada segundo
setInterval(function() {
    fetch('/datos')
        .then(response => response.json())
        .then(data => {
            tiempoBaseESP = data.tiempo;
            tUltimoPaquete = performance.now();
            document.getElementById('txtVelocidad').innerText = data.velocidad;
            if(data.estado === 1) {
                document.getElementById('txtEstado').innerText = "LÍNEA DETECTADA";
                document.getElementById('txtEstado').className = "status-ok";
            } else {
                document.getElementById('txtEstado').innerText = "BUSCANDO LÍNEA...";
                document.getElementById('txtEstado').className = "status-wait";
            }
        });
}, 1000);
</script>
</body>
</html>
)=====";

// Ruta principal: Envía la página web
void manejarRaiz() {
  server.send_P(200, "text/html", HTML_PAGINA);
}

// Ruta /datos: Envía las variables en formato JSON estructurado
void manejarDatos() {
  String json = "{\"tiempo\":" + String(tiempoSegundos) + 
                ",\"estado\":" + String(codigoEstado) + 
                ",\"velocidad\":" + String(velocidadMotor) + "}";
  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);

  // 1. CONFIGURACIÓN MIXTA: Wi-Fi Access Point + ESP-NOW [Luis Llamas]
  WiFi.mode(WIFI_AP_STA); 
  WiFi.softAP(ssid, password);
  
  // Imprime la IP en el puerto serie de la laptop solo para que sepas cuál es
  Serial.print("Servidor listo. Dirección IP: ");
  Serial.println(WiFi.softAPIP()); 

  if (esp_now_init() != ESP_OK) return;
  
  esp_now_peer_info_t peerInfo;
  memset(&peerInfo, 0, sizeof(peerInfo));
  memcpy(peerInfo.peer_addr, macReceptor, 6);
  peerInfo.channel = WiFi.channel(); // Forzamos a ESP-NOW a usar el mismo canal del Wi-Fi [Luis Llamas]
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);

  // 2. CONFIGURACIÓN DEL SERVIDOR WEB
  server.on("/", manejarRaiz);
  server.on("/datos", manejarDatos);
  server.begin();
}

unsigned long ultimoEnvio = 0;
void loop() {
  server.handleClient(); // Atiende las peticiones web en vivo

  // El algoritmo de simulación corre cada 2 segundos sin detener el servidor web
  if (millis() - ultimoEnvio >= 2000) {
    ultimoEnvio = millis();
    tiempoSegundos = ultimoEnvio / 1000;

    if (codigoEstado == 1) {
      codigoEstado = 0;      
      velocidadMotor = 80;   
      misDatos.comando = '0'; 
    } else {
      codigoEstado = 1;      
      velocidadMotor = 200;  
      misDatos.comando = '1'; 
    }
    esp_now_send(macReceptor, (uint8_t *) &misDatos, sizeof(misDatos));
  }
}
