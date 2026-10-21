#include "boton.h"
#include "config.h"

// Lecturas iguales seguidas necesarias para aceptar un cambio
const int LECTURAS_ESTABLES = BOTON_ANTIRREBOTE_MS / TS_BASE_MS;

static bool presionado = false;  // estado ya filtrado
static int contador = 0;

void iniciarBoton()
{
  pinMode(PIN_BOTON_CALIBRAR, INPUT_PULLUP);
}

bool botonPulsado()
{
  bool lectura = digitalRead(PIN_BOTON_CALIBRAR) == LOW;  // a GND al pulsar

  if (lectura == presionado) {
    contador = 0;
    return false;
  }
  if (++contador < LECTURAS_ESTABLES) return false;

  contador = 0;
  presionado = lectura;
  return presionado;  // solo el flanco de pulsar
}
