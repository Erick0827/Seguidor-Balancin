"""
Visualiza en el PC la imagen en escala de grises (160x120) recibida por Serial
junto con el cálculo del centroide y el error de la línea negra.

Requisitos:  pip install pyserial opencv-python numpy
Uso:         python ver_camara.py     (presiona 'q' para salir)
"""

import struct
import time
import cv2
import numpy as np
import serial

PUERTO = "COM11"
BAUDIOS = 921600          # Debe coincidir con el firmware
MAGIC = b"\xAA\x55"

# Parámetros de la imagen de la ESP32
ANCHO = 160
ALTO = 120
TAMANO_FRAME = ANCHO * ALTO  # 19,200 bytes

# Región de Interés (ROI) configurada en la ESP32
ROI_Y_INICIO = 95
ROI_Y_FIN = 115


def sincronizar(ser):
    """Lee byte a byte hasta encontrar la cabecera de sincronización 0xAA 0x55."""
    buf = b""
    while True:
        b = ser.read(1)
        if not b:
            return False
        buf = (buf + b)[-2:]
        if buf == MAGIC:
            return True


def main():
    ser = serial.Serial(PUERTO, BAUDIOS, timeout=2)
    ser.reset_input_buffer()
    print(f"Conectado a {PUERTO} a {BAUDIOS} baudios. Presiona 'q' para salir.")

    frames = 0
    t0 = time.time()
    fps = 0.0

    try:
        while True:
            # 1. Sincronizar con la cabecera 0xAA 0x55
            if not sincronizar(ser):
                continue

            # 2. Leer la trama de datos de control (4 bytes: Error_High, Error_Low, Validez, Checksum)
            datos_control = ser.read(4)
            if len(datos_control) < 4:
                continue

            error_raw, linea_detectada, checksum = struct.unpack(">hBB", datos_control)

            # Verificar checksum simple (XOR)
            byte_h = datos_control[0]
            byte_l = datos_control[1]
            if (byte_h ^ byte_l ^ datos_control[2]) != checksum:
                print("Error de Checksum en la trama UART")
                continue

            # 3. Leer la imagen RAW en escala de grises
            raw_img = ser.read(TAMANO_FRAME)
            if len(raw_img) != TAMANO_FRAME:
                continue

            # Reconstruir la matriz de imagen
            frame_gray = np.frombuffer(raw_img, dtype=np.uint8).reshape((ALTO, ANCHO))

            # Convertir a BGR para poder dibujar guías en colores sobre la imagen
            frame_bgr = cv2.cvtColor(frame_gray, cv2.COLOR_GRAY2BGR)

            # --- DIBUJAR OVERLAY DE CONTROL Y DIAGNÓSTICO ---
            
            # Dibujar caja de la Región de Interés (ROI)
            cv2.rectangle(frame_bgr, (0, ROI_Y_INICIO), (ANCHO - 1, ROI_Y_FIN), (255, 0, 0), 1)

            # Dibujar la línea central de referencia (x = 80)
            cv2.line(frame_bgr, (ANCHO // 2, 0), (ANCHO // 2, ALTO), (128, 128, 128), 1)

            # Si la línea fue detectada por la ESP32, dibujar la posición del centroide
            if linea_detectada == 1:
                centroide_x = (ANCHO // 2) + error_raw
                centroide_y = (ROI_Y_INICIO + ROI_Y_FIN) // 2

                # Punto en el centroide detectado (Verde)
                cv2.circle(frame_bgr, (centroide_x, centroide_y), 4, (0, 255, 0), -1)

                # Línea de error desde el centro hasta la posición de la línea
                cv2.line(frame_bgr, (ANCHO // 2, centroide_y), (centroide_x, centroide_y), (0, 0, 255), 2)
                
                texto_estado = f"Error: {error_raw} px"
                color_texto = (0, 255, 0)
            else:
                texto_estado = "LINEA NO DETECTADA"
                color_texto = (0, 0, 255)

            # Cómputo de Cuadros por Segundo (FPS)
            frames += 1
            if time.time() - t0 >= 1.0:
                fps = frames / (time.time() - t0)
                frames = 0
                t0 = time.time()

            # Renderizar información en pantalla
            cv2.putText(frame_bgr, f"{fps:.1f} FPS", (5, 15),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.4, (255, 255, 0), 1)
            
            cv2.putText(frame_bgr, texto_estado, (5, ALTO - 5),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.4, color_texto, 1)

            # Escalar la ventana (x3) para visualizarla cómodamente en el monitor
            frame_resaltado = cv2.resize(frame_bgr, (ANCHO * 3, ALTO * 3), interpolation=cv2.INTER_NEAREST)

            cv2.imshow("ESP32-S3-CAM Line Tracker (QQVGA)", frame_resaltado)

            if cv2.waitKey(1) & 0xFF == ord("q"):
                break

    finally:
        ser.close()
        cv2.destroyAllWindows()


if __name__ == "__main__":
    main()