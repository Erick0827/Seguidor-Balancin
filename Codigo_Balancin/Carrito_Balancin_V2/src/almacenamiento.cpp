#include "almacenamiento.h"
#include <Preferences.h>

static const char *ESPACIO = "balancin";

bool cargarCalibracion(CalibracionGuardada &c)
{
  Preferences p;
  if (!p.begin(ESPACIO, true)) return false;  // solo lectura; falla si no existe

  bool hay = p.isKey("ang");
  if (hay) {
    c.biasGiro[0] = p.getFloat("bgx", 0);
    c.biasGiro[1] = p.getFloat("bgy", 0);
    c.biasGiro[2] = p.getFloat("bgz", 0);
    c.anguloEquilibrio = p.getFloat("ang", 0);
  }
  p.end();
  return hay;
}

bool guardarCalibracion(const CalibracionGuardada &c)
{
  Preferences p;
  if (!p.begin(ESPACIO, false)) return false;

  bool ok = p.putFloat("bgx", c.biasGiro[0]) > 0 &&
            p.putFloat("bgy", c.biasGiro[1]) > 0 &&
            p.putFloat("bgz", c.biasGiro[2]) > 0 &&
            p.putFloat("ang", c.anguloEquilibrio) > 0;  // "ang" al final: marca de calibración completa
  p.end();
  return ok;
}
