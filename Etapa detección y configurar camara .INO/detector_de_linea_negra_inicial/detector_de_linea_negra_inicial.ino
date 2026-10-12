#include "esp_camera.h"

// Pines de la GOOUUU ESP32-S3-CAM
#define PWDN_GPIO_NUM  -1
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM  15
#define SIOD_GPIO_NUM  4
#define SIOC_GPIO_NUM  5
#define Y9_GPIO_NUM    16
#define Y8_GPIO_NUM    17
#define Y7_GPIO_NUM    18
#define Y6_GPIO_NUM    12
#define Y5_GPIO_NUM    10
#define Y4_GPIO_NUM    8
#define Y3_GPIO_NUM    9
#define Y2_GPIO_NUM    11
#define VSYNC_GPIO_NUM 6
#define HREF_GPIO_NUM  7
#define PCLK_GPIO_NUM  13

#define BAUDIOS 921600   

#define VOLTEAR_VERTICAL   1   
#define VOLTEAR_HORIZONTAL 0   

#define ANCHO_IMG 160
#define ALTO_IMG  120
#define UMBRAL_NEGRO 100 

#define ROI_Y_INICIO 95
#define ROI_Y_FIN    115

void setup() {
  Serial.begin(BAUDIOS);
  Serial.setTxBufferSize(32768); // Aumentar buffer TX Serial para no bloquear la transmisión de frames

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  
  // AJUSTE CRÍTICO DE RELOJ Y FORMATO PARA ESP32-S3:
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_GRAYSCALE; 
  config.frame_size   = FRAMESIZE_QQVGA;     
  config.jpeg_quality = 12;
  config.fb_count     = 2;                   
  config.fb_location  = CAMERA_FB_IN_PSRAM;
  config.grab_mode    = CAMERA_GRAB_LATEST;

  // Inicializar cámara
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    // Si falla en escala de grises, reintentar la configuración estándar
    while (true) {
      delay(1000);
    }
  }

  sensor_t *s = esp_camera_sensor_get();
  if (s != NULL) {
    s->set_vflip(s, VOLTEAR_VERTICAL);
    s->set_hmirror(s, VOLTEAR_HORIZONTAL);
    s->set_brightness(s, 0);
    s->set_saturation(s, 0);
  }
}

void loop() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    delay(10);
    return;
  }

  uint32_t suma_ponderada_x = 0;
  uint32_t suma_pesos = 0;
  uint8_t *imagen = fb->buf;

  // 1. Recorrer la ROI para calcular el centroide
  for (int y = ROI_Y_INICIO; y < ROI_Y_FIN; y++) {
    int offset_fila = y * ANCHO_IMG;
    for (int x = 0; x < ANCHO_IMG; x++) {
      uint8_t pixel = imagen[offset_fila + x];

      if (pixel < UMBRAL_NEGRO) {
        uint8_t peso = 255 - pixel; 
        suma_ponderada_x += (x * peso);
        suma_pesos += peso;
      }
    }
  }

  int16_t error_linea = 0;
  uint8_t linea_detectada = 0;

  if (suma_pesos > 1000) { 
    int centroide_x = suma_ponderada_x / suma_pesos; 
    error_linea = centroide_x - (ANCHO_IMG / 2);     
    linea_detectada = 1;
  } else {
    error_linea = 0; 
    linea_detectada = 0;
  }

  // 2. Transmitir Cabecera y Datos de Control
  uint8_t paquete[6];
  paquete[0] = 0xAA; 
  paquete[1] = 0x55; 
  paquete[2] = (error_linea >> 8) & 0xFF; 
  paquete[3] = error_linea & 0xFF;        
  paquete[4] = linea_detectada;            
  paquete[5] = paquete[2] ^ paquete[3] ^ paquete[4]; 

  Serial.write(paquete, 6);

  // 3. Transmitir la Matriz RAW en Escala de Grises
  Serial.write(fb->buf, fb->len);

  // Liberar el buffer para habilitar la captura del siguiente frame por DMA
  esp_camera_fb_return(fb);
}