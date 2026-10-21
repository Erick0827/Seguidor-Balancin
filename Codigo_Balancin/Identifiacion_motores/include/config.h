#pragma once

// ---------------- MPU6050 ----------------
#define PIN_MPU_SDA        12
#define PIN_MPU_SCL        11
#define MPU_DIRECCION      0x68   // AD0 a GND
#define MPU_ALPHA          0.98f  // Filtro complementario
#define FILTRO_ANG_MUESTRAS 3     // Media móvil del ángulo (máx 16)
#define FILTRO_VEL_MUESTRAS 7     // Media móvil de la velocidad de caída (máx 16)
#define OFFSET_ANGULO      0.0f   // Corrección de montaje inicial (grados); ajustable desde la web

// ---------------- Encoders ----------------
#define PIN_ENC1_A         10
#define PIN_ENC1_B         9
#define PIN_ENC2_A         18
#define PIN_ENC2_B         8
#define ENC_PPR            15     // Pulsos por vuelta del motor
#define ENC_REDUCCION      36     // Relación de la reductora
#define ENC_VENTANA_MS     PERIODO_VEL_MS  // Se mide en cada ciclo del lazo externo
#define FILTRO_ENC_MUESTRAS 7     // Media móvil de rpm (máx 16)
// >>> CAMBIAR AQUÍ el signo de lectura de cada encoder (true = invertido) <<<
#define ENC1_INVERTIR      false
#define ENC2_INVERTIR      true

// ---------------- TB6612FNG ----------------
#define PIN_PWMA           4
#define PIN_AIN2           5
#define PIN_AIN1           6
#define PIN_STBY           7
#define PIN_BIN1           15
#define PIN_BIN2           16
#define PIN_PWMB           17
// >>> CAMBIAR AQUÍ el sentido de giro de cada motor (true = invertido) <<<
#define MOTOR1_INVERTIR    false
#define MOTOR2_INVERTIR    false
#define PWM_FRECUENCIA     20000  // Hz (TB6612 admite hasta 100 kHz)
#define PWM_RESOLUCION     8      // bits -> duty 0..255
#define PWM_CANAL_A        0
#define PWM_CANAL_B        1
#define PWM_ZONA_MUERTA    40      // Duty mínimo para que el motor arranque; ajustable desde la web

// ---------------- WiFi (punto de acceso) ----------------
#define WIFI_SSID          "Balancin"
#define WIFI_PASS          "balancin123"  // Mínimo 8 caracteres
#define WIFI_CANAL         6
#define WEB_PUERTO         80

// ---------------- Pulsadores ----------------
#define PIN_BTN_CALIBRAR   14     // A GND, usa INPUT_PULLUP

// ---------------- Control en cascada ----------------
// Lazo interno (PID de ángulo, D con el giroscopio): rápido
#define PERIODO_CONTROL_MS 5      // 200 Hz
#define KP_INT_INICIAL     0.0f
#define KI_INT_INICIAL     0.0f
#define KD_INT_INICIAL     0.0f
#define INTEGRAL_INT_MAX   100.0f // Límite de la acción integral del interno (PWM)
#define BIAS_INICIAL       0.0f   // PWM constante sumado a la salida del interno
#define BIAS_MAX           100.0f // Límite del bias desde la web
#define PWM_LIMITE        255.0f // Límite del mando (máx 255)
#define RAMPA_MAX          20.0f  // Cambio máximo de salida por ciclo
#define ANGULO_MAX_SEGURO  30.0f  // Grados; fuera de esto se cortan motores

// Lazo externo (PID de velocidad): lento. Su salida es la referencia de ángulo del interno.
#define PERIODO_VEL_MS     50     // 20 Hz (10 veces más lento que el interno)
#define KP_EXT_INICIAL     0.0f
#define KI_EXT_INICIAL     0.0f
#define KD_EXT_INICIAL     0.0f
#define VEL_REF_MAX        100.0f // rpm máximas que se pueden pedir desde la web
#define ANGULO_REF_MAX     5.0f   // Grados máximos que el externo puede inclinar la referencia
#define VEL_INVERTIR       false  // Cambiar si el externo acelera en sentido contrario

// Gráficas: una muestra cada PERIODO_MUESTRA_MS (en el ciclo del lazo interno)
#define PERIODO_MUESTRA_MS 20     // 50 Hz
#define MUESTRAS_POR_ENVIO 100    // Máximo de muestras por respuesta de /datos

static_assert(PERIODO_VEL_MS % PERIODO_CONTROL_MS == 0, "PERIODO_VEL_MS debe ser múltiplo de PERIODO_CONTROL_MS");
static_assert(PERIODO_VEL_MS >= 5 * PERIODO_CONTROL_MS, "El lazo externo debe ser al menos 5 veces más lento que el interno");
static_assert(PERIODO_MUESTRA_MS % PERIODO_CONTROL_MS == 0, "PERIODO_MUESTRA_MS debe ser múltiplo de PERIODO_CONTROL_MS");
