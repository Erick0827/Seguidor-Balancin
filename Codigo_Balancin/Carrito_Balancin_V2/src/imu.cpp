#include "imu.h"
#include "config.h"

// Tercer eje: el que apunta hacia adelante
const int EJE_AVANCE = 3 - EJE_RUEDAS - EJE_VERTICAL;

// Para que el ángulo del acelerómetro crezca en el mismo sentido que el
// giroscopio: si (ruedas, vertical, avance) siguen la regla de la mano
// derecha (p. ej. Y, Z, X), inclinar hacia adelante hace negativa la
// aceleración de avance.
const float SIGNO_ACEL = ((EJE_VERTICAL - EJE_RUEDAS + 3) % 3 == 1) ? -1.0f : 1.0f;

const float ALFA = TAU_COMPLEMENTARIO / (TAU_COMPLEMENTARIO + TS_BASE);

static LecturaMPU ultima = {};
static float angulo = 0;
static float velCaida = 0;
static float sumaGiro = 0;
static int muestrasGiro = 0;

float anguloDesdeAcelerometro(const float acel[3])
{
  float a = atan2f(SIGNO_ACEL * acel[EJE_AVANCE], acel[EJE_VERTICAL]);
  return SIGNO_ANGULO * a * RAD_TO_DEG;
}

bool iniciarIMU(const float *biasGiro)
{
  if (!iniciarMPU()) return false;

  if (biasGiro != NULL) fijarOffsetGiro(biasGiro);
  else if (!calibrarGiroscopio(MPU_MUESTRAS_CALIBRACION)) return false;

  if (!leerMPU(ultima)) return false;
  fijarAnguloIMU(anguloDesdeAcelerometro(ultima.acel));
  return true;
}

bool actualizarIMU()
{
  LecturaMPU l;
  if (!leerMPU(l)) return false;
  ultima = l;

  velCaida = SIGNO_ANGULO * l.giro[EJE_RUEDAS];
  angulo = ALFA * (angulo + velCaida * TS_BASE) + (1.0f - ALFA) * anguloDesdeAcelerometro(l.acel);

  sumaGiro += SIGNO_GIRO * l.giro[EJE_VERTICAL];
  muestrasGiro++;
  return true;
}

const LecturaMPU &ultimaLecturaIMU()
{
  return ultima;
}

void fijarAnguloIMU(float a)
{
  angulo = a;
  velCaida = 0;
  sumaGiro = 0;
  muestrasGiro = 0;
}

float anguloInclinacion()
{
  return angulo;
}

float velocidadCaida()
{
  return velCaida;
}

float tomarVelocidadGiro()
{
  float v = (muestrasGiro > 0) ? sumaGiro / muestrasGiro : 0;
  sumaGiro = 0;
  muestrasGiro = 0;
  return v;
}
