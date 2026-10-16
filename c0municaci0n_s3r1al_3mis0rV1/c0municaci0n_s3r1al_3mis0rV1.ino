const int PIN_FLASH = 48; // Tu pin de Flash validado

void setup() {
  Serial.begin(115200); // Inicializa el bus principal compartido
  pinMode(PIN_FLASH, OUTPUT);
}

void loop() {
  // 1. Ciclo Encendido: Enciende Flash local y envía un '1' limpio
  digitalWrite(PIN_FLASH, HIGH);
  Serial.print('1'); // Enviamos solo un carácter (sin saltos de línea \n)
  
  // Mensaje largo solo para ti en la laptop, no afecta a la otra placa
  Serial.println(" -> [TX] Comando 1 enviado."); 
  delay(2000);

  // 2. Ciclo Apagado: Apaga Flash local y envía un '0' limpio
  digitalWrite(PIN_FLASH, LOW);
  Serial.print('0'); // Enviamos solo un carácter
  
  Serial.println(" -> [TX] Comando 0 enviado.");
  delay(2000);
}
