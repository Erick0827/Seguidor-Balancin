"""
Visualizador de alta velocidad con múltiples ROIs (Visión Anticipativa)
para clasificación de línea (Cercano, Medio, Lejano) y prevención de curvas.
"""

import struct
import time
import cv2
import numpy as np
import serial

# --- CONFIGURACIÓN DE COMUNICACIÓN ---
PUERTO = "COM11"  # Asegúrate de que sea tu puerto actual
BAUDIOS = 921600
MAGIC = b"\xAA\x55\xAA\x55"
MAX_JPEG = 500_000

# --- DIMENSIONES Y ZONAS DE ANÁLISIS (ROIS) ---
ANCHO = 160
ALTO = 120

# Tres niveles de altura para anticipación
ROI_CERCA_Y = (100, 115)  # Control inmediato (abajo)
ROI_MEDIO_Y = (75, 90)  # Anticipación media (centro)
ROI_LEJOS_Y = (50, 65)  # Vista lejana / detección de curvas (arriba)

# Límites horizontales para evitar las esquinas oscuras (viñeteo de la lente)
ROI_X_INICIO = 20
ROI_X_FIN = 140

UMBRAL_NEGRO = 90
TOLERANCIA_CENTRO = 8


def sincronizar(ser):
    buf = b""
    while True:
        b = ser.read(1)
        if not b:
            return False
        buf = (buf + b)[-4:]
        if buf == MAGIC:
            return True


def procesar_roi(frame, rango_y):
    """Calcula el centroide X para una franja horizontal específica"""
    roi = frame[rango_y[0] : rango_y[1], ROI_X_INICIO:ROI_X_FIN]
    _, thresh = cv2.threshold(roi, UMBRAL_NEGRO, 255, cv2.THRESH_BINARY_INV)
    M = cv2.moments(thresh)

    if M["m00"] > 400:  # Área mínima para considerarlo línea válida
        cx = int(M["m10"] / M["m00"]) + ROI_X_INICIO
        return cx
    return None


def main():
    ser = serial.Serial(PUERTO, BAUDIOS, timeout=2)
    ser.reset_input_buffer()
    print(
        f"Iniciando visor anticipativo en {PUERTO}. Presiona 'q' en la ventana para salir."
    )

    frames = 0
    t0 = time.time()
    fps = 0.0

    try:
        while True:
            if not sincronizar(ser):
                continue

            cabecera = ser.read(4)
            if len(cabecera) < 4:
                continue
            longitud = struct.unpack("<I", cabecera)[0]
            if longitud == 0 or longitud > MAX_JPEG:
                continue

            jpeg = ser.read(longitud)
            if len(jpeg) != longitud:
                continue
            if not (jpeg[:2] == b"\xff\xd8" and b"\xff\xd9" in jpeg[-8:]):
                continue

            frame = cv2.imdecode(np.frombuffer(jpeg, np.uint8), cv2.IMREAD_GRAYSCALE)
            if frame is None:
                continue

            frame_bgr = cv2.cvtColor(frame, cv2.COLOR_GRAY2BGR)

            # --- OBTENER CENTROIDES EN LAS TRES CAPAS ---
            cx_cerca = procesar_roi(frame, ROI_CERCA_Y)
            cx_medio = procesar_roi(frame, ROI_MEDIO_Y)
            cx_lejos = procesar_roi(frame, ROI_LEJOS_Y)

            # --- LÓGICA DE ANTICIPACIÓN Y CONTROL ---
            if cx_cerca is not None:
                error_actual = cx_cerca - (ANCHO // 2)

                # Clasificación base según la zona cercana
                if abs(error_actual) <= TOLERANCIA_CENTRO:
                    posicion_str = "CENTRADA"
                    color_texto = (0, 255, 0)  # Verde
                elif error_actual < 0:
                    posicion_str = "IZQUIERDA"
                    color_texto = (255, 0, 0)  # Azul
                else:
                    posicion_str = "DERECHA"
                    color_texto = (0, 0, 255)  # Rojo

                # Tendencia anticipativa si tenemos datos de la zona lejana
                if cx_lejos is not None:
                    tendencia = (
                        cx_lejos - cx_cerca
                    )  # Positivo = se tuerce a la derecha más adelante
                    if tendencia > 18:
                        aviso_curva = " -> [Curva Proyectada DERECHA]"
                    elif tendencia < -18:
                        aviso_curva = " -> [Curva Proyectada IZQUIERDA]"
                    else:
                        aviso_curva = " -> [Recta Adelante]"
                else:
                    aviso_curva = ""

                texto_estado = (
                    f"Pos: {posicion_str} ({error_actual}px){aviso_curva}"
                )

                # Dibujar puntos de referencia en la imagen
                cv2.circle(
                    frame_bgr,
                    (cx_cerca, (ROI_CERCA_Y[0] + ROI_CERCA_Y[1]) // 2),
                    3,
                    (0, 255, 0),
                    -1,
                )
                if cx_medio is not None:
                    cv2.circle(
                        frame_bgr,
                        (cx_medio, (ROI_MEDIO_Y[0] + ROI_MEDIO_Y[1]) // 2),
                        3,
                        (0, 255, 255),
                        -1,
                    )
                if cx_lejos is not None:
                    cv2.circle(
                        frame_bgr,
                        (cx_lejos, (ROI_LEJOS_Y[0] + ROI_LEJOS_Y[1]) // 2),
                        3,
                        (255, 0, 255),
                        -1,
                    )

                # Línea de error visual en el nivel cercano
                cv2.line(
                    frame_bgr,
                    (ANCHO // 2, (ROI_CERCA_Y[0] + ROI_CERCA_Y[1]) // 2),
                    (cx_cerca, (ROI_CERCA_Y[0] + ROI_CERCA_Y[1]) // 2),
                    (0, 0, 255),
                    2,
                )
            else:
                texto_estado = "SIN LINEA"
                color_texto = (0, 0, 255)

            # Dibujar los rectángulos de las 3 zonas de interés en pantalla
            cv2.rectangle(
                frame_bgr,
                (ROI_X_INICIO, ROI_CERCA_Y[0]),
                (ROI_X_FIN, ROI_CERCA_Y[1]),
                (255, 0, 0),
                1,
            )
            cv2.rectangle(
                frame_bgr,
                (ROI_X_INICIO, ROI_MEDIO_Y[0]),
                (ROI_X_FIN, ROI_MEDIO_Y[1]),
                (0, 255, 0),
                1,
            )
            cv2.rectangle(
                frame_bgr,
                (ROI_X_INICIO, ROI_LEJOS_Y[0]),
                (ROI_X_FIN, ROI_LEJOS_Y[1]),
                (0, 0, 255),
                1,
            )
            cv2.line(
                frame_bgr, (ANCHO // 2, 0), (ANCHO // 2, ALTO), (128, 128, 128), 1
            )

            # Control de FPS
            frames += 1
            if time.time() - t0 >= 1.0:
                fps = frames / (time.time() - t0)
                frames = 0
                t0 = time.time()

            cv2.putText(
                frame_bgr,
                f"{fps:.1f} FPS",
                (5, 15),
                cv2.FONT_HERSHEY_SIMPLEX,
                0.4,
                (255, 255, 0),
                1,
            )
            cv2.putText(
                frame_bgr,
                texto_estado,
                (5, ALTO - 5),
                cv2.FONT_HERSHEY_SIMPLEX,
                0.35,
                color_texto,
                1,
            )

            # Escalar ventana para verla cómoda
            frame_resaltado = cv2.resize(
                frame_bgr, (ANCHO * 3, ALTO * 3), interpolation=cv2.INTER_NEAREST
            )
            cv2.imshow("ESP32-S3-CAM Vision Anticipativa", frame_resaltado)

            if cv2.waitKey(1) & 0xFF == ord("q"):
                break

    finally:
        ser.close()
        cv2.destroyAllWindows()


if __name__ == "__main__":
    main()