#include "calibracion.h"
#include "config.h"
#include "imu.h"
#include "mpu6050.h"
#include "almacenamiento.h"

const uint32_t CICLOS_ESPERA    = CAL_ESPERA_MS / TS_BASE_MS;
const uint32_t CICLOS_RESULTADO = CAL_MOSTRAR_RESULTADO_MS / TS_BASE_MS;

// Lo lee la web desde el núcleo 0; es un entero de 32 bits (lectura atómica)
static volatile EstadoCalibracion estado = CAL_INACTIVA;
static volatile float anguloCalibrado = 0;
static volatile float desviacionGiro = 0;

static uint32_t ciclos = 0;   // ciclos en el estado actual
static int validas = 0;
static int fallas = 0;
static double sumaGiro[3], sumaGiro2[3], sumaAcel[3];

static void cambiarEstado(EstadoCalibracion e)
{
  estado = e;
  ciclos = 0;
}

static void empezarMuestreo()
{
  validas = 0;
  fallas = 0;
  for (int i = 0; i < 3; i++) sumaGiro[i] = sumaGiro2[i] = sumaAcel[i] = 0;
  cambiarEstado(CAL_MUESTREO);
}

static void acumular(bool lecturaOk)
{
  if (!lecturaOk) {
    fallas++;
    return;
  }
  const LecturaMPU &l = ultimaLecturaIMU();
  for (int i = 0; i < 3; i++) {
    sumaGiro[i]  += l.giro[i];
    sumaGiro2[i] += (double)l.giro[i] * l.giro[i];
    sumaAcel[i]  += l.acel[i];
  }
  validas++;
}

// Pasos 4 a 6. true si se aplicó la calibración.
static bool terminar(float &anguloEquilibrio)
{
  if (fallas > CAL_FALLAS_MAX || validas == 0) {
    cambiarEstado(CAL_ERROR_MPU);
    return false;
  }

  // 4. Desviación estándar del giroscopio (el giro ya viene sin el bias anterior)
  float media[3], acel[3], desvMax = 0;
  for (int i = 0; i < 3; i++) {
    media[i] = sumaGiro[i] / validas;
    double var = sumaGiro2[i] / validas - (double)media[i] * media[i];
    float desv = sqrtf(var > 0 ? var : 0);
    if (desv > desvMax) desvMax = desv;
    acel[i] = sumaAcel[i] / validas;
  }
  desviacionGiro = desvMax;
  if (desvMax > CAL_DESVIACION_MAX) {
    cambiarEstado(CAL_ERROR_MOVIMIENTO);
    return false;
  }

  // 5. Nuevo bias = bias anterior + promedio residual; ángulo del acelerómetro promediado
  CalibracionGuardada c;
  obtenerOffsetGiro(c.biasGiro);
  for (int i = 0; i < 3; i++) c.biasGiro[i] += media[i];
  c.anguloEquilibrio = anguloDesdeAcelerometro(acel);

  fijarOffsetGiro(c.biasGiro);
  fijarAnguloIMU(c.anguloEquilibrio);
  anguloCalibrado = c.anguloEquilibrio;
  anguloEquilibrio = c.anguloEquilibrio;

  // 6. Guardar (los motores están apagados, la pausa de la flash no afecta)
  cambiarEstado(guardarCalibracion(c) ? CAL_OK : CAL_ERROR_FLASH);
  return true;
}

bool pasoCalibracion(bool pulsado, bool motoresHabilitados, bool lecturaOk,
                     float &anguloEquilibrio)
{
  ciclos++;

  switch (estado) {
    case CAL_ESPERA:
      if (ciclos >= CICLOS_ESPERA) empezarMuestreo();
      return false;

    case CAL_MUESTREO:
      acumular(lecturaOk);
      if (ciclos >= (uint32_t)CAL_MUESTRAS) return terminar(anguloEquilibrio);
      return false;

    default:  // inactiva o mostrando un resultado
      if (pulsado) {
        cambiarEstado(motoresHabilitados ? CAL_RECHAZADA : CAL_ESPERA);
      } else if (estado != CAL_INACTIVA && ciclos >= CICLOS_RESULTADO) {
        cambiarEstado(CAL_INACTIVA);
      }
      return false;
  }
}

bool calibrando()
{
  return estado == CAL_ESPERA || estado == CAL_MUESTREO;
}

EstadoCalibracion estadoCalibracion()
{
  return estado;
}

float ultimoAnguloCalibrado()
{
  return anguloCalibrado;
}

float ultimaDesviacionGiro()
{
  return desviacionGiro;
}
