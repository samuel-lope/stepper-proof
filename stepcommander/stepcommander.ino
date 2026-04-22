/*
 * =================================================================================
 * Stepper Commander - Interface H8P
 * =================================================================================
 *
 * Board: Arduino Uno / Nano (Atmega328P)
 * Função: Envio de comandos H8P e recebimento de alertas do controlador principal
 *
 * Periféricos:
 * - Teclado Matricial 4x4 (Pinos: Linhas 9,8,7,6 / Colunas 5,4,3,2)
 * - Display LCD 16x2 I2C (SDA=A4, SCL=A5, Endereço: 0x27)
 * - SoftwareSerial (RX=10, TX=11) -> Conecta ao TX/RX da placa Principal
 *
 * Mapeamento de Teclas Especial:
 * - '*' -> ':'
 * - '#' -> ','
 * - '* + #' -> 'Enter' (Enviar Comando)
 * - 'C' -> Apagar Tudo (Clear)
 * - 'D' -> Apagar Último (Backspace)
 * =================================================================================
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <SoftwareSerial.h>

// --- Configurações do LCD ---
// Endereço 0x27, 16 colunas e 2 linhas
LiquidCrystal_I2C lcd(0x27, 16, 2);

// --- Configurações da Serial ---
// RX no pino 10 (conectar ao TX da placa principal)
// TX no pino 11 (conectar ao RX da placa principal)
SoftwareSerial mainSerial(10, 11);

// --- Configurações do Teclado Matricial ---
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

// Pinos conectados às linhas (R1, R2, R3, R4)
byte rowPins[ROWS] = {9, 8, 7, 6};
// Pinos conectados às colunas (C1, C2, C3, C4)
byte colPins[COLS] = {5, 4, 3, 2};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// --- Variáveis de Controle ---
String inputBuffer = "";
const int MAX_INPUT_LEN = 12; // 16 colunas do LCD - 4 ("Cmd:") = 12

void setup() {
  // Serial de Debug (opcional, pode ser vista no Monitor Serial do PC)
  Serial.begin(9600);
  
  // Serial para comunicação com a placa principal
  mainSerial.begin(9600);
  
  // Inicialização do LCD
  lcd.init();
  lcd.backlight();
  
  // Interface Inicial
  lcd.setCursor(0, 0);
  lcd.print("Cmd:");
  lcd.setCursor(0, 1);
  lcd.print("Pronto.");
  
  Serial.println("Commander Iniciado.");
}

void processIncomingMessage() {
  if (mainSerial.available()) {
    String msg = mainSerial.readStringUntil('\n');
    msg.trim(); // Remove espaços e quebras de linha (\r\n)
    
    if (msg.length() > 0) {
      Serial.println("Recebido: " + msg); // Debug
      
      lcd.setCursor(0, 1);
      lcd.print("                "); // Limpa a linha 2 (16 espaços)
      lcd.setCursor(0, 1);
      
      // Tradução dos códigos H8P para o usuário
      if (msg == "A0") {
        lcd.print("Sis Inicializado");
      } 
      else if (msg == "B0") {
        lcd.print("Iniciando Fila");
      } 
      else if (msg == "B1") {
        lcd.print("Motor Parado");
      } 
      else if (msg.startsWith("B2")) {
        lcd.print("Pausa Global");
      } 
      else if (msg == "B4") {
        lcd.print("Repeat ON");
      } 
      else if (msg == "B5") {
        lcd.print("Fila Executada");
      } 
      else if (msg == "B6") {
        lcd.print("Repeat OFF");
      } 
      else if (msg.startsWith("B7")) {
        lcd.print("Motor Habilitado");
      }
      else if (msg.startsWith("B8")) {
        lcd.print("Motor Desabiltd.");
      }
      else if (msg.startsWith("B9") || msg.startsWith("BA")) {
        lcd.print("Preset EEPROM");
      }
      else if (msg == "E0") {
        lcd.print("Err: Em Execucao");
      } 
      else if (msg == "E1") {
        lcd.print("Err: Fila Vazia");
      } 
      else if (msg == "E2") {
        lcd.print("Err: Fila Cheia");
      } 
      else if (msg == "E3") {
        lcd.print("Erro de Sintaxe");
      } 
      else if (msg == "E4") {
        lcd.print("Preset Invalido");
      } 
      else if (msg.startsWith("C0")) {
        lcd.print("Salvo: Slot " + msg.substring(3, 4)); // Mostra o ID do Slot
      } 
      else if (msg.startsWith("BB") || msg.startsWith("BC")) {
        lcd.print("FastAct. Exec");
      } 
      else {
        // Exibe o código puro caso não esteja mapeado
        lcd.print(msg.substring(0, 16)); 
      }
    }
  }
}

void updateLCD() {
  lcd.setCursor(4, 0);
  lcd.print("            "); // Limpa a área de input (12 espaços)
  lcd.setCursor(4, 0);
  lcd.print(inputBuffer);
}

void loop() {
  processIncomingMessage();
  
  char key = keypad.getKey();
  
  if (key) {
    // Tratamento especial: Enter é a combinação '*' seguido de '#'
    // Se o buffer terminar com ':' (que significa que o último botão pressionado foi '*')
    // e a tecla atual for '#', nós enviamos o comando.
    if (key == '#') {
      if (inputBuffer.endsWith(":")) {
        // Removemos o ':' do buffer, pois ele era apenas o '*'
        inputBuffer.remove(inputBuffer.length() - 1);
        
        // Envia o comando para a placa principal
        mainSerial.println(inputBuffer);
        Serial.println("Enviado: " + inputBuffer); // Debug
        
        // Feedback Visual
        lcd.setCursor(0, 1);
        lcd.print("                ");
        lcd.setCursor(0, 1);
        lcd.print("Enviado.");
        
        // Limpa o buffer após o envio
        inputBuffer = "";
      } else {
        // Pressionamento normal de '#' -> Vira vírgula ','
        if (inputBuffer.length() < MAX_INPUT_LEN) {
          inputBuffer += ",";
        }
      }
    } 
    else if (key == '*') {
      // Pressionamento normal de '*' -> Vira dois-pontos ':'
      if (inputBuffer.length() < MAX_INPUT_LEN) {
        inputBuffer += ":";
      }
    } 
    else if (key == 'D') {
      // D -> Backspace
      if (inputBuffer.length() > 0) {
        inputBuffer.remove(inputBuffer.length() - 1);
      }
    } 
    else if (key == 'C') {
      // C -> Clear
      inputBuffer = "";
    } 
    else {
      // Teclas numéricas normais (0-9, A, B)
      if (inputBuffer.length() < MAX_INPUT_LEN) {
        inputBuffer += key;
      }
    }
    
    updateLCD();
  }
}
