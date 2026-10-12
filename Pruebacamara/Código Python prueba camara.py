"""
Visualiza en el PC lo que ve la ESP32-S3-CAM (OV5640) enviado por serial.

Requisitos:  pip install pyserial opencv-python numpy
Uso:         python ver_camara.py     (presiona 'q' para salir)

Cierra el Monitor Serie del Arduino IDE antes de ejecutarlo.
"""
import struct
import time

import cv2
import numpy as np
import serial

PUERTO = "COM11"
BAUDIOS = 921600          # Debe coincidir con el firmware
MAGIC = b"\xAA\x55\xAA\x55"
MAX_JPEG = 500_000         # descarta tramas corruptas


def sincronizar(ser):
    """Lee byte a byte hasta encontrar la cabecera de sincronización."""
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
            frame = cv2.imdecode(np.frombuffer(jpeg, np.uint8), cv2.IMREAD_COLOR)
            if frame is None:
                continue

            frames += 1
            if time.time() - t0 >= 1.0:
                fps = frames / (time.time() - t0)
                frames = 0
                t0 = time.time()

            cv2.putText(frame, f"{fps:.1f} fps", (10, 25),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)
            cv2.imshow("ESP32-S3-CAM OV5640", frame)

            if cv2.waitKey(1) & 0xFF == ord("q"):
                break
    finally:
        ser.close()
        cv2.destroyAllWindows()


if __name__ == "__main__":
    main()