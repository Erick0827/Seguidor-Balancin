"""
Visualizador fluido de alta velocidad (>30 FPS) para ESP32-S3-CAM
Procesa el centroide de la línea en tiempo real con OpenCV.
"""

import struct
import time
import cv2
import numpy as np
import serial

PUERTO = "COM11"
BAUDIOS = 921600
MAGIC = b"\xAA\x55\xAA\x55"
MAX_JPEG = 500_000

# Parámetros de la ROI (Región de Interés) y Umbral
ANCHO = 160
ALTO = 120
ROI_Y_INICIO = 95
ROI_Y_FIN = 115
UMBRAL_NEGRO = 100


def sincronizar(ser):
    buf = b""
    while True:
        b = ser.read(1)
        if not b:
            return False
        buf = (buf + b)[-4:]
        if buf == MAGIC:
            return True


def main():
    ser = serial.Serial(PUERTO, BAUDIOS, timeout=2)
    ser.reset_input_buffer()
    print(f"Leyendo {PUERTO} a {BAUDIOS} baudios. Presiona 'q' para salir.")

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

            # Decodificar la imagen comprimida
            frame = cv2.imdecode(np.frombuffer(jpeg, np.uint8), cv2.IMREAD_COLOR)
            if frame is None:
                continue

            # --- PROCESAMIENTO DE LÍNEA Y CENTROIDE EN PYTHON ---
            gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)

            # Extraer ROI (Filas de interés)
            roi = gray[ROI_Y_INICIO:ROI_Y_FIN, 0:ANCHO]

            # Binarizar: píxeles oscuros (< UMBRAL_NEGRO) pasan a ser blancos (255)
            _, thresh = cv2.threshold(roi, UMBRAL_NEGRO, 255, cv2.THRESH_BINARY_INV)

            # Calcular momentos de la ROI
            M = cv2.moments(thresh)

            if M["m00"] > 1000:  # Si se detectan suficientes píxeles de línea
                centroide_x = int(M["m10"] / M["m00"])
                error_linea = centroide_x - (ANCHO // 2)

                # Dibujar centroide (Verde) y línea de error (Roja)
                centroide_y = (ROI_Y_INICIO + ROI_Y_FIN) // 2
                cv2.circle(frame, (centroide_x, centroide_y), 3, (0, 255, 0), -1)
                cv2.line(
                    frame,
                    (ANCHO // 2, centroide_y),
                    (centroide_x, centroide_y),
                    (0, 0, 255),
                    2,
                )
                texto_estado = f"Error: {error_linea} px"
                color_texto = (0, 255, 0)
            else:
                texto_estado = "SIN LINEA"
                color_texto = (0, 0, 255)

            # Dibujar caja ROI (Azul) y línea central guía
            cv2.rectangle(
                frame, (0, ROI_Y_INICIO), (ANCHO - 1, ROI_Y_FIN), (255, 0, 0), 1
            )
            cv2.line(
                frame, (ANCHO // 2, 0), (ANCHO // 2, ALTO), (128, 128, 128), 1
            )

            # Cálculo de FPS
            frames += 1
            if time.time() - t0 >= 1.0:
                fps = frames / (time.time() - t0)
                frames = 0
                t0 = time.time()

            cv2.putText(
                frame,
                f"{fps:.1f} FPS",
                (5, 15),
                cv2.FONT_HERSHEY_SIMPLEX,
                0.4,
                (255, 255, 0),
                1,
            )
            cv2.putText(
                frame,
                texto_estado,
                (5, ALTO - 5),
                cv2.FONT_HERSHEY_SIMPLEX,
                0.4,
                color_texto,
                1,
            )

            # Escalar la ventana x3 para visualización cómoda
            frame_resaltado = cv2.resize(
                frame, (ANCHO * 3, ALTO * 3), interpolation=cv2.INTER_NEAREST
            )
            cv2.imshow("ESP32-S3-CAM (QQVGA JPEG High-FPS)", frame_resaltado)

            if cv2.waitKey(1) & 0xFF == ord("q"):
                break

    finally:
        ser.close()
        cv2.destroyAllWindows()


if __name__ == "__main__":
    main()