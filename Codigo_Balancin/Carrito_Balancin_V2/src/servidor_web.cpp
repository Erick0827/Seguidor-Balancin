#include "servidor_web.h"
#include <WebServer.h>
#include "config.h"
#include "lazo_control.h"
#include "calibracion.h"
#include "pagina_web.h"

static WebServer servidor(WEB_PUERTO);

static const char *textoMotivo(MotivoParo m)
{
  switch (m) {
    case PARO_NINGUNO:         return "Controlando";
    case PARO_APAGADO:         return "Detenido: motores apagados";
    case PARO_ESPERA_VERTICAL: return "Detenido: esperando posición vertical";
    case PARO_CAIDA:           return "Detenido: caída, el ángulo salió de los límites";
    case PARO_FALLA_MPU:       return "Detenido: el MPU no tiene señal";
    case PARO_CALIBRANDO:      return "Detenido: calibrando";
  }
  return "";
}

// Texto del indicador de calibración ("" = no se muestra)
static String textoCalibracion(EstadoCalibracion c)
{
  char b[96];
  switch (c) {
    case CAL_ESPERA:
      return "Calibración: suelta el botón y deja el carrito en equilibrio";
    case CAL_MUESTREO:
      return "Calibración: tomando muestras, no mover";
    case CAL_OK:
      snprintf(b, sizeof(b), "Calibrado y guardado: equilibrio %.2f°", ultimoAnguloCalibrado());
      return b;
    case CAL_ERROR_MOVIMIENTO:
      snprintf(b, sizeof(b), "Error: el carrito se movió (%.2f °/s), repetir calibración",
               ultimaDesviacionGiro());
      return b;
    case CAL_ERROR_MPU:
      return "Error: el MPU no respondió durante la calibración, repetir";
    case CAL_ERROR_FLASH:
      return "Calibrado, pero no se pudo guardar en la flash";
    case CAL_RECHAZADA:
      return "Apaga los motores para calibrar";
    default:
      return "";
  }
}

static void responderJSON(const String &json)
{
  servidor.sendHeader("Cache-Control", "no-store");
  servidor.send(200, "application/json", json);
}

static String parametrosJSON()
{
  ParametrosControl p = obtenerParametros();
  char b[256];
  snprintf(b, sizeof(b),
           "{\"kpa\":%g,\"kia\":%g,\"kda\":%g,\"aja\":%g,\"kpg\":%g,\"kig\":%g,"
           "\"kpx\":%g,\"kix\":%g,\"kdx\":%g,\"xref\":%g}",
           p.kpAngulo, p.kiAngulo, p.kdAngulo, p.ajusteAngulo, p.kpGiro, p.kiGiro,
           p.kpPosicion, p.kiPosicion, p.kdPosicion, p.posicionRef);
  return String(b);
}

// Lee un argumento numérico; si falta o no es un número deja v sin cambios
static void leerArgumento(const char *nombre, float &v)
{
  if (!servidor.hasArg(nombre)) return;
  String s = servidor.arg(nombre);
  s.trim();
  s.replace(',', '.');
  if (s.length() == 0) return;

  char *fin = NULL;
  float x = strtof(s.c_str(), &fin);
  if (*fin == '\0' && isfinite(x)) v = x;
}

static void rutaRaiz()
{
  servidor.send(200, "text/html; charset=utf-8", PAGINA_WEB);
}

static void rutaDatos()
{
  EstadoControl e = obtenerEstado();
  EstadoCalibracion c = estadoCalibracion();

  String json;
  json.reserve(300 + TAMANO_COLA_MUESTRAS * 48);
  char b[300];
  snprintf(b, sizeof(b),
           "{\"hab\":%d,\"mpu\":%d,\"lim\":%d,\"mot\":%d,\"txt\":\"%s\",\"cal\":%d,\"ctxt\":\"%s\",\"m\":[",
           e.habilitado, e.mpuOk, e.anguloEnLimites, (int)e.motivo, textoMotivo(e.motivo),
           (int)c, textoCalibracion(c).c_str());
  json += b;

  MuestraControl m;
  bool primera = true;
  while (obtenerMuestra(m)) {
    snprintf(b, sizeof(b), "%s[%.2f,%.2f,%.1f,%.1f,%.1f,%.1f]", primera ? "" : ",",
             m.anguloRef, m.angulo, m.ref1, m.rpm1, m.ref2, m.rpm2);
    json += b;
    primera = false;
  }
  json += "]}";
  responderJSON(json);
}

static void rutaMotores()
{
  habilitarMotores(servidor.arg("on") == "1");
  responderJSON("{}");
}

static void rutaParametros()
{
  if (servidor.method() == HTTP_POST) {
    ParametrosControl p = obtenerParametros();
    leerArgumento("kpa", p.kpAngulo);
    leerArgumento("kia", p.kiAngulo);
    leerArgumento("kda", p.kdAngulo);
    leerArgumento("aja", p.ajusteAngulo);
    leerArgumento("kpg", p.kpGiro);
    leerArgumento("kig", p.kiGiro);
    leerArgumento("kpx", p.kpPosicion);
    leerArgumento("kix", p.kiPosicion);
    leerArgumento("kdx", p.kdPosicion);
    leerArgumento("xref", p.posicionRef);
    fijarParametros(p);
  }
  responderJSON(parametrosJSON());
}

static void rutaBateria()
{
  bool ok = true;
  if (servidor.method() == HTTP_POST) {
    float v = NAN;
    leerArgumento("v", v);
    ok = fijarVoltajeBateria(v);
  }
  char b[96];
  snprintf(b, sizeof(b), "{\"ok\":%d,\"v\":%.2f,\"min\":%.1f,\"max\":%.1f}",
           ok, obtenerVoltajeBateria(), V_BATERIA_MIN, V_BATERIA_MAX);
  responderJSON(b);
}

static void tareaServidorWeb(void *arg)
{
  servidor.on("/", HTTP_GET, rutaRaiz);
  servidor.on("/datos", HTTP_GET, rutaDatos);
  servidor.on("/motores", HTTP_POST, rutaMotores);
  servidor.on("/parametros", rutaParametros);
  servidor.on("/bateria", rutaBateria);
  servidor.onNotFound([]() { servidor.send(404, "text/plain", "No encontrado"); });
  servidor.begin();

  for (;;) {
    servidor.handleClient();
    vTaskDelay(pdMS_TO_TICKS(2));  // cede el núcleo al WiFi
  }
}

void iniciarServidorWeb()
{
  xTaskCreatePinnedToCore(tareaServidorWeb, "servidor_web", PILA_COMUNICACION, NULL,
                          PRIORIDAD_COMUNICACION, NULL, NUCLEO_COMUNICACION);
}
