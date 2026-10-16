const int PIN_RGB = 48; // Pin del LED RGB nativo de tu GOOUUU [1.1]

void setup() {
  Serial.begin(115200); // Escucha a la misma velocidad el pin físico RX
}

void loop() {
  // Revisa si entró al menos 1 byte por el pin RX
  if (Serial.available() > 0) {
    char caracterRecibido = Serial.read(); // Lee el único carácter de forma directa

    if (caracterRecibido == '1') {
      rgbLedWrite(PIN_RGB, 0, 50, 0); // Enciende el LED de la GOOUUU en VERDE [1.1]
    } 
    else if (caracterRecibido == '0') {
      rgbLedWrite(PIN_RGB, 0, 0, 0);  // Apaga el LED RGB [1.1]
    }
  }
}
