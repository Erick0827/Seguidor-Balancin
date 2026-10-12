"""
Visualizador en escala de grises comprimida (JPEG) a alta velocidad con 
clasificación de posición (Izquierda, Centro, Derecha).
"""

import struct
import time
import cv2
import numpy as np
import serial

PUERTO = "COM11"  # Asegúrate de que coincida con tu puerto actual
BAUDIOS = 921600
MAGIC = b"\xAA\x55\xAA\x55"
MAX_JPEG = 500_000

ANCHO = 160
ALTO = 120
ROI_Y_INICIO = 95
ROI_Y_FIN = 115
UMBRAL_NEGRO = 100

# Margen de tolerancia en píxeles para considerar la línea "CENTRADA"
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


def main():
    ser = serial.Serial(PUERTO, BAUDIOS, timeout=2)
    ser.reset_input_buffer()
    print(
        f"Leyendo escala de grises comprimida en {PUERTO}. Presiona 'q' para salir."
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

            # --- PROCESAMIENTO DE CENTROIDE Y POSICIÓN ---
            roi = frame[ROI_Y_INICIO:ROI_Y_FIN, 0:ANCHO]
            _, thresh = cv2.threshold(roi, UMBRAL_NEGRO, 255, cv2.THRESH_BINARY_INV)
            M = cv2.moments(thresh)

            if M["m00"] > 1000:
                centroide_x = int(M["m10"] / M["m00"])
                error_linea = centroide_x - (ANCHO // 2)
                centroide_y = (ROI_Y_INICIO + ROI_Y_FIN) // 2

                # Clasificación de la posición de la línea
                if abs(error_linea) <= TOLERANCIA_CENTRO:
                    posicion_str = "CENTRADA"
                    color_texto = (0, 255, 0)  # Verde
                elif error_linea < 0:
                    posicion_str = "IZQUIERDA"
                    color_texto = (255, 0, 0)  # Azul
                else:
                    posicion_str = "DERECHA"
                    color_texto = (0, 0, 255)  # Rojo

                cv2.circle(frame_bgr, (centroide_x, centroide_y), 3, (0, 255, 0), -1)
                cv2.line(
                    frame_bgr,
                    (ANCHO // 2, centroide_y),
                    (centroide_x, centroide_y),
                    (0, 0, 255),
                    2,
                )
                texto_estado = f"Pos: {posicion_str} ({error_linea}px)"
            else:
                texto_estado = "SIN LINEA"
                color_texto = (0, 0, 255)

            cv2.rectangle(
                frame_bgr, (0, ROI_Y_INICIO), (ANCHO - 1, ROI_Y_FIN), (255, 0, 0), 1
            )
            cv2.line(
                frame_bgr, (ANCHO // 2, 0), (ANCHO // 2, ALTO), (128, 128, 128), 1
            )

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
                0.4,
                color_texto,
                1,
            )

            frame_resaltado = cv2.resize(
                frame_bgr, (ANCHO * 3, ALTO * 3), interpolation=cv2.INTER_NEAREST
            )
            cv2.imshow("ESP32-S3-CAM Clasificador de Linea", frame_resaltado)

            if cv2.waitKey(1) & 0xFF == ord("q"):
                break

    finally:
        ser.close()
        cv2.destroyAllWindows()


if __name__ == "__main__":
    main()