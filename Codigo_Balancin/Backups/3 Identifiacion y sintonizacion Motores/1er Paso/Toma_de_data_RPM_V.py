import serial
import pandas as pd

PUERTO = "COM8"
BAUDIOS = 115200

NUM_MUESTRAS = 400
NUM_REPETICIONES = 30

ser = serial.Serial(PUERTO, BAUDIOS)

datos = {}

while True:

    linea = ser.readline().decode().strip()

    if linea.startswith("INICIO"):

        rep = int(linea.split(",")[1])

        muestras = []

        while True:

            linea = ser.readline().decode().strip()

            if linea.startswith("FIN"):
                break

            muestras.append(float(linea))

        datos[f"Repeticion {rep}"] = muestras

        print(f"Repeticion {rep} recibida")

        if rep == NUM_REPETICIONES:
            break

# Crear columna tiempo

tiempo = [i * 30 for i in range(NUM_MUESTRAS)]

# Crear columna PWM

Voltaje = []

for i in range(NUM_MUESTRAS):

    if i < 200:
        Voltaje.append(3.6)
    else:
        Voltaje.append(6.6)

# DataFrame

df = pd.DataFrame()

df["Tiempo"] = tiempo
df["Voltaje"] = Voltaje

for i in range(1, NUM_REPETICIONES + 1):
    df[f"Repeticion {i}"] = datos[f"Repeticion {i}"]

# Excel

df.to_excel(
    "Identificacion_Motor_2.xlsx",
    index=False
)

print("Archivo generado")