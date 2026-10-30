#include "imu.h"
#include "config.h"
#include "filtro_kalman.h"

// Tercer eje: el que apunta hacia adelante
const int EJE_AVANCE = 3 - EJE_RUEDAS - EJE_VERTICAL;

// Para que el ángulo del acelerómetro crezca en el mismo sentido que el
// giroscopio: si (ruedas, vertical, avance) siguen la regla de la mano
// derecha (p. ej. Y, Z, X), inclinar hacia adelante hace negativa la
// aceleración de avance.
const float SIGNO_ACEL = ((EJE_VERTICAL - EJE_RUEDAS + 3) % 3 == 1) ? -1.0f : 1.0f;

static LecturaMPU ultima = {};
static FiltroKalman kalman;
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
  configurarKalman(kalman, KALMAN_Q_ANGULO, KALMAN_Q_SESGO, KALMAN_R_MEDIDA);
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

  sumaGiro += SIGNO_GIRO * l.giro[EJE_VERTICAL];
  muestrasGiro++;
  return true;
}

void actualizarAnguloKalman()
{
  // Predicción con la velocidad de caída de la última lectura (mismo eje y signo)
  predecirKalman(kalman, velCaida, TS_KALMAN);

  // Corrección solo si el acelerómetro mide casi solo la gravedad
  const float *a = ultima.acel;
  float magnitud = sqrtf(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
  if (fabsf(magnitud - 1.0f) <= KALMAN_RECHAZO_G) {
    corregirKalman(kalman, anguloDesdeAcelerometro(a));
  }
}

const LecturaMPU &ultimaLecturaIMU()
{
  return ultima;
}

void fijarAnguloIMU(float a)
{
  // El driver ya resta el bias calibrado: el sesgo del Kalman parte de 0
  // y solo estima el residuo
  reiniciarKalman(kalman, a, 0);
  velCaida = 0;
  sumaGiro = 0;
  muestrasGiro = 0;
}

float anguloInclinacion()
{
  return kalman.angulo;
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
