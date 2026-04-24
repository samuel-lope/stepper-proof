/*
 * =================================================================================
 * Stepper Commander - Interface H8P
 * =================================================================================
 *
 * Board: Arduino Uno / Nano (Atmega328P)
 * Função: Envio de comandos H8P e recebimento de alertas do controlador
 * principal
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
 * - '* + C' -> Apagar Tudo (Clear)
 * - '* + D' -> Apagar Último (Backspace)
 * - '* + A' -> Inserir '-' (Menos)
 * - '* + B' -> (Livre)
 * =================================================================================
 */

#include <Arduino.h>
#include <Keypad.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>
#include <TM1638plus.h>
#include <Wire.h>

// --- Protótipos ---
void updateTM1638();
void updateLCD();
void processIncomingMessage();

// --- Configurações do LCD ---
// Endereço 0x27, 16 colunas e 2 linhas
LiquidCrystal_I2C lcd(0x27, 16, 2);

// --- Configurações da Serial ---
// RX no pino 10 (conectar ao TX da placa principal)
// TX no pino 11 (conectar ao RX da placa principal)
SoftwareSerial mainSerial(10, 11);

// --- Configurações do TM1638 (7 Segmentos) ---
#define TM_STB A0
#define TM_CLK A1
#define TM_DIO A2
bool high_freq = false; // Default para a maioria das placas
TM1638plus tm(TM_STB, TM_CLK, TM_DIO, high_freq);

// --- Configurações do Teclado Matricial ---
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {{'1', '2', '3', 'A'},
                         {'4', '5', '6', 'B'},
                         {'7', '8', '9', 'C'},
                         {'*', '0', '#', 'D'}};

// Pinos conectados às linhas (R1, R2, R3, R4)
byte rowPins[ROWS] = {5, 4, 3, 2};
// Pinos conectados às colunas (C1, C2, C3, C4)
byte colPins[COLS] = {9, 8, 7, 6};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// --- Variáveis de Controle ---
String inputBuffer = "";
String lastCommand = "";
const int MAX_INPUT_LEN = 12; // 16 colunas do LCD - 4 ("Cmd:") = 12

void setup() {
  // Serial de Debug (opcional, pode ser vista no Monitor Serial do PC)
  Serial.begin(9600);

  // Serial para comunicação com a placa principal
  mainSerial.begin(9600);

  // Inicialização do LCD (Comentado para teste de teclado)
  lcd.init();
  lcd.backlight();

  // Inicialização do TM1638 (Comentado para teste de teclado)
  // tm.displayBegin();
  // tm.reset();

  // Interface Inicial
  lcd.setCursor(0, 0);
  lcd.print("Cmd:");
  lcd.setCursor(0, 1);
  lcd.print("Pronto.");

  // updateTM1638(); // Exibe vazio

  Serial.println("Commander Iniciado.");
}

void processIncomingMessage() {
  if (mainSerial.available()) {
    String msg = mainSerial.readStringUntil('\n');
    msg.trim(); // Remove espaços e quebras de linha (\r\n)

    if (msg.length() > 0) {
      Serial.print("Recebido: ");
      Serial.println(msg); // Debug cru

      // Tradução dos códigos H8P para o terminal (LCD desativado)
      if (msg == "A0")
        Serial.println("-> Sis Inicializado");
      else if (msg == "B0")
        Serial.println("-> Iniciando Fila");
      else if (msg == "B1")
        Serial.println("-> Motor Parado");
      else if (msg.startsWith("B2"))
        Serial.println("-> Pausa Global");
      else if (msg == "B4")
        Serial.println("-> Repeat ON");
      else if (msg == "B5")
        Serial.println("-> Fila Executada");
      else if (msg == "B6")
        Serial.println("-> Repeat OFF");
      else if (msg.startsWith("B7"))
        Serial.println("-> Motor Habilitado");
      else if (msg.startsWith("B8"))
        Serial.println("-> Motor Desabiltd.");
      else if (msg.startsWith("B9") || msg.startsWith("BA"))
        Serial.println("-> Preset EEPROM");
      else if (msg == "E0")
        Serial.println("-> Err: Em Execucao");
      else if (msg == "E1")
        Serial.println("-> Err: Fila Vazia");
      else if (msg == "E2")
        Serial.println("-> Err: Fila Cheia");
      else if (msg == "E3")
        Serial.println("-> Erro de Sintaxe");
      else if (msg == "E4")
        Serial.println("-> Preset Invalido");
      else if (msg.startsWith("C0"))
        Serial.println("-> Salvo: Slot " + msg.substring(3, 4));
      else if (msg.startsWith("BB") || msg.startsWith("BC"))
        Serial.println("-> FastAct. Exec");
      else
        Serial.println("-> " + msg);
    }
  }
}

void updateLCD() {
  lcd.setCursor(4, 0);
  lcd.print("            "); // Limpa a área de input (12 espaços)
  lcd.setCursor(4, 0);
  lcd.print(inputBuffer);
}

void updateTM1638() {
  String tmString = "";
  // Converte ':' e ',' para '.' para que não ocupem um dígito inteiro no
  // display de 7 segmentos
  for (int i = 0; i < inputBuffer.length(); i++) {
    char c = inputBuffer.charAt(i);
    if (c == ':' || c == ',') {
      tmString += ".";
    } else {
      tmString += c;
    }
  }

  // Preenche o resto com espaços para garantir que os 8 dígitos sejam
  // sobrepostos corretamente
  String paddedStr = tmString;
  int rawChars = 0;
  for (int i = 0; i < tmString.length(); i++) {
    if (tmString.charAt(i) != '.')
      rawChars++;
  }

  while (rawChars < 8) {
    paddedStr += " ";
    rawChars++;
  }

  // A biblioteca exibe a string formatada no display
  tm.displayText(paddedStr.c_str());
}

void loop() {
  processIncomingMessage();

  char key = keypad.getKey();

  if (key) {
    Serial.print("Tecla Pressionada: ");
    Serial.println(key);
    // Tratamento especial com combinações usando '*'
    // O '*' adiciona um ':' no buffer.
    // Se o buffer termina com ':', verificamos as combinações.
    if (inputBuffer.endsWith(":")) {
      if (key == '#') {
        // * + # -> Enter (Enviar Comando)
        inputBuffer.remove(inputBuffer.length() - 1);

        if (inputBuffer.length() == 0 && lastCommand.length() > 0) {
          inputBuffer = lastCommand;
        } else if (inputBuffer.length() > 0) {
          lastCommand = inputBuffer;
        }

        if (inputBuffer.length() > 0) {
          // Envia o comando para a placa principal
          mainSerial.println(inputBuffer);
          Serial.println("Enviado: " + inputBuffer); // Debug
        }
        inputBuffer = "";
      } else if (key == 'C') {
        // * + C -> Clear
        inputBuffer = "";
      } else if (key == 'D') {
        // * + D -> Backspace
        if (inputBuffer.length() >= 2) {
          inputBuffer.remove(inputBuffer.length() - 2);
        } else {
          inputBuffer = "";
        }
      } else if (key == 'A') {
        // * + A -> Sinal de menos '-'
        inputBuffer.setCharAt(inputBuffer.length() - 1, '-');
      } else if (key == '*') {
        // Pressionamento repetido de '*'
        if (inputBuffer.length() < MAX_INPUT_LEN) {
          inputBuffer += ":";
        }
      } else {
        // Tecla normal após um '*' sem ser combinação especial
        if (inputBuffer.length() < MAX_INPUT_LEN) {
          inputBuffer += key;
        }
      }
    } else {
      // Fluxo normal (sem prefixo '*')
      if (key == '#') {
        // Pressionamento normal de '#' -> Vira vírgula ','
        if (inputBuffer.length() < MAX_INPUT_LEN) {
          inputBuffer += ",";
        }
      } else if (key == '*') {
        // Pressionamento normal de '*' -> Vira dois-pontos ':'
        if (inputBuffer.length() < MAX_INPUT_LEN) {
          inputBuffer += ":";
        }
      } else {
        // Teclas normais (0-9, A, B, C, D)
        if (inputBuffer.length() < MAX_INPUT_LEN) {
          inputBuffer += key;
        }
      }
    }

    updateLCD();
    // updateTM1638();
  }
}
