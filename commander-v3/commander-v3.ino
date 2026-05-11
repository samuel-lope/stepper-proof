/*
 * =================================================================================
 * Stepper Commander - Interface de Menu Interativo LCD com EEPROM Local
 * =================================================================================
 * Board: Arduino Uno / Nano (Atmega328P)
 * Função: Interface IHM para construir, armazenar e enviar comandos.
 * * * GUIA DE USO DO MENU:
 * [A] - NAVEGAR: Alterna entre os menus (Line -> Steps -> Speed -> Motor ->
 * Início). [B] - (Livre para uso futuro) [C] - (Livre para uso futuro) [D] -
 * APAGAR (Backspace): Apaga o último número. No menu principal, limpa a
 * seleção.
 * [#] - GRAVAR E ENVIAR: Salva a linha construída na EEPROM local e envia para
 * a placa.
 * -> Se uma linha estiver selecionada (Ex: L:1), apaga (DEL) a linha da
 * memória.
 * [*] - RUN / STOP:
 * -> Se digitar um NÚMERO (1-9) antes: Envia a linha específica + comando 01.
 * -> Se NÃO digitar nada: Envia TODAS as linhas gravadas + comando 01.
 * -> Se estiver em RUN: Envia comando 02 (STOP).
 * =================================================================================
 */

#include <Arduino.h>
#include <EEPROM.h>
#include <Keypad.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>

// --- Configurações dos Periféricos ---
LiquidCrystal_I2C lcd(0x27, 16, 2);
SoftwareSerial mainSerial(10, 11);

const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {{'1', '2', '3', 'A'},
                         {'4', '5', '6', 'B'},
                         {'7', '8', '9', 'C'},
                         {'*', '0', '#', 'D'}};

byte rowPins[ROWS] = {5, 4, 3, 2};
byte colPins[COLS] = {9, 8, 7, 6};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// --- EEPROM e Dados Locais ---
#define EEPROM_MAGIC_BYTE 0x42
#define EEPROM_MAGIC_ADDR 1000

struct SavedLine {
  bool active;
  char command[30]; // Espaço suficiente para armazenar a linha de comando
                    // formatada
};
SavedLine savedLines[9];

// --- Máquina de Estados do Menu ---
enum MenuState {
  MENU_PRINCIPAL,
  INSERIR_LINE,
  INSERIR_STEPS,
  INSERIR_SPEED,
  INSERIR_MOTOR
};
MenuState currentState = MENU_PRINCIPAL;

// --- Variáveis de Construção e Execução ---
int currentLine = 1; // Linha atual em edição (1 a 9)
int runTarget = 0; // 0 = Todas as linhas; 1 a 9 = Linha específica para RUN/DEL
String valPassos = "";
String valVelocidade = "";
String valMotor = "";
String lastStatus = "Ready.";

bool isRunning = false; // Controla o status dos motores (Sincronizado)
bool wasRunning =
    false; // Usado para identificar transição de tela para a animação

// --- Custom Chars para Animação dos Motores (Pixel Art) ---
// Quadro 1: Engrenagem / Cruz (+)
byte f1_TL[8] = {B00000, B00010, B00010, B00010,
                 B00010, B00010, B11111, B00000};
byte f1_TR[8] = {B00000, B01000, B01000, B01000,
                 B01000, B01000, B11111, B00000};
byte f1_BL[8] = {B11111, B00010, B00010, B00010,
                 B00010, B00010, B00000, B00000};
byte f1_BR[8] = {B11111, B01000, B01000, B01000,
                 B01000, B01000, B00000, B00000};

// Quadro 2: Engrenagem / Diagonal (X)
byte f2_TL[8] = {B10000, B11000, B01100, B00110,
                 B00011, B00001, B00000, B00000};
byte f2_TR[8] = {B00001, B00011, B00110, B01100,
                 B11000, B10000, B00000, B00000};
byte f2_BL[8] = {B00000, B00000, B00001, B00011,
                 B00110, B01100, B11000, B10000};
byte f2_BR[8] = {B00000, B00000, B10000, B11000,
                 B01100, B00110, B00011, B00001};

// --- Protótipos ---
void initEEPROM();
void updateLCD();
void handleKeyPress(char key);
void processIncomingMessage();
void drawAnimation();

void setup() {
  Serial.begin(9600);
  mainSerial.begin(9600);

  lcd.init();
  lcd.backlight();

  // Carregamento dos caracteres customizados na VRAM do LCD
  lcd.createChar(0, f1_TL);
  lcd.createChar(1, f1_TR);
  lcd.createChar(2, f1_BL);
  lcd.createChar(3, f1_BR);
  lcd.createChar(4, f2_TL);
  lcd.createChar(5, f2_TR);
  lcd.createChar(6, f2_BL);
  lcd.createChar(7, f2_BR);

  initEEPROM();

  Serial.println("Commander Menu Iniciado (Com EEPROM).");
  updateLCD();
}

void loop() {
  char key = keypad.getKey();
  if (key) {
    handleKeyPress(key);
  }
  processIncomingMessage();

  // --- Lógica de Transição para Animação ---
  if (isRunning && !wasRunning) {
    lcd.clear();
    wasRunning = true;
    lcd.setCursor(6, 0);
    lcd.print("RUN ");
  } else if (!isRunning && wasRunning) {
    wasRunning = false;
    updateLCD(); // Restaura o menu normal quando os motores param
  }

  // --- Processamento da Animação (Não bloqueante) ---
  if (isRunning) {
    drawAnimation();
  }
}

void initEEPROM() {
  if (EEPROM.read(EEPROM_MAGIC_ADDR) != EEPROM_MAGIC_BYTE) {
    for (int i = 0; i < 9; i++) {
      savedLines[i].active = false;
      memset(savedLines[i].command, 0, sizeof(savedLines[i].command));
      EEPROM.put(i * sizeof(SavedLine), savedLines[i]);
    }
    EEPROM.write(EEPROM_MAGIC_ADDR, EEPROM_MAGIC_BYTE);
    Serial.println("EEPROM formatada para uso do Commander.");
  } else {
    for (int i = 0; i < 9; i++) {
      EEPROM.get(i * sizeof(SavedLine), savedLines[i]);
    }
    Serial.println("Dados carregados da EEPROM local.");
  }
}

unsigned long lastAnimTime = 0;
int animFrame = 0;
int animDots = 0;

void drawAnimation() {
  if (millis() - lastAnimTime > 200) {
    lastAnimTime = millis();
    animFrame = !animFrame;
    animDots++;
    if (animDots > 3)
      animDots = 0;

    lcd.setCursor(6, 1);
    if (animDots == 0)
      lcd.print("    ");
    if (animDots == 1)
      lcd.print(".   ");
    if (animDots == 2)
      lcd.print("..  ");
    if (animDots == 3)
      lcd.print("... ");

    if (animFrame == 0) {
      lcd.setCursor(1, 0);
      lcd.write(byte(0));
      lcd.write(byte(1));
      lcd.setCursor(1, 1);
      lcd.write(byte(2));
      lcd.write(byte(3));
      lcd.setCursor(12, 0);
      lcd.write(byte(0));
      lcd.write(byte(1));
      lcd.setCursor(12, 1);
      lcd.write(byte(2));
      lcd.write(byte(3));
    } else {
      lcd.setCursor(1, 0);
      lcd.write(byte(4));
      lcd.write(byte(5));
      lcd.setCursor(1, 1);
      lcd.write(byte(6));
      lcd.write(byte(7));
      lcd.setCursor(12, 0);
      lcd.write(byte(4));
      lcd.write(byte(5));
      lcd.setCursor(12, 1);
      lcd.write(byte(6));
      lcd.write(byte(7));
    }
  }
}

void handleKeyPress(char key) {
  // Bloqueia interações complexas do menu enquanto os motores rodam (Apenas *
  // funciona)
  if (isRunning && key != '*')
    return;

  // ==========================================
  // LÓGICA DO MENU PRINCIPAL (Navegação Numérica)
  // ==========================================
  if (currentState == MENU_PRINCIPAL) {
    if (key >= '1' && key <= '9') {
      runTarget = key - '0';
      updateLCD();
      return;
    }
    if (key == 'D') {
      if (runTarget > 0) {
        runTarget = 0;
        updateLCD();
        return;
      }
    }
  }

  // ==========================================
  // COMANDO RUN / STOP GLOBAL (*)
  // ==========================================
  if (key == '*') {
    if (isRunning && runTarget == 0) {
      mainSerial.println("02");
      lastStatus = "Cmd: 02 (STOP)";
      isRunning = false;
      runTarget = 0;
    } else {
      bool sentAnything = false;

      if (runTarget == 0) {
        for (int i = 0; i < 9; i++) {
          if (savedLines[i].active) {
            mainSerial.println(savedLines[i].command);
            delay(60);
            sentAnything = true;
          }
        }
      } else {
        int idx = runTarget - 1;
        if (savedLines[idx].active) {
          mainSerial.println(savedLines[idx].command);
          delay(60);
          sentAnything = true;
        }
      }

      if (sentAnything) {
        mainSerial.println("01");
        lastStatus =
            "RUN L:" + (runTarget == 0 ? String("ALL") : String(runTarget));
        isRunning = true;
      } else {
        lastStatus = "Err: Empty!";
      }
      runTarget = 0;
    }
    currentState = MENU_PRINCIPAL;
    if (!isRunning)
      updateLCD();
    return;
  }

  // ==========================================
  // COMANDO SALVAR/ENVIAR OU APAGAR LINHA (#)
  // ==========================================
  if (key == '#') {
    if (currentState == MENU_PRINCIPAL && runTarget > 0) {
      int idx = runTarget - 1;

      savedLines[idx].active = false;
      memset(savedLines[idx].command, 0, sizeof(savedLines[idx].command));
      EEPROM.put(idx * sizeof(SavedLine), savedLines[idx]);

      lastStatus = "Deleted: L" + String(runTarget);
      runTarget = 0;
      updateLCD();
      return;
    }

    if (valPassos == "")
      valPassos = "0";
    if (valPassos == "-")
      valPassos = "-0";

    String cmdOut = String(currentLine) + "-10:" + valPassos;
    if (valVelocidade.length() > 0)
      cmdOut += ",11:" + valVelocidade;
    if (valMotor.length() > 0)
      cmdOut += ",14:" + valMotor;

    int idx = currentLine - 1;
    savedLines[idx].active = true;
    cmdOut.toCharArray(savedLines[idx].command, 30);
    EEPROM.put(idx * sizeof(SavedLine), savedLines[idx]);

    mainSerial.println(cmdOut);
    Serial.println("Gravado e Enviado: " + cmdOut);

    lastStatus = "Saved: L" + String(currentLine);

    currentLine++;
    if (currentLine > 9)
      currentLine = 1;

    valPassos = "";
    valVelocidade = "";
    valMotor = "";
    runTarget = 0;
    currentState = MENU_PRINCIPAL;
    updateLCD();
    return;
  }

  // ==========================================
  // NAVEGAÇÃO CICLICA E EDIÇÃO (A, D)
  // ==========================================

  // Apenas a Tecla 'A' alterna as telas de forma cíclica
  if (key == 'A') {
    runTarget = 0; // Limpa o alvo caso esteja no menu principal
    if (currentState == MENU_PRINCIPAL)
      currentState = INSERIR_LINE;
    else if (currentState == INSERIR_LINE)
      currentState = INSERIR_STEPS;
    else if (currentState == INSERIR_STEPS)
      currentState = INSERIR_SPEED;
    else if (currentState == INSERIR_SPEED)
      currentState = INSERIR_MOTOR;
    else if (currentState == INSERIR_MOTOR)
      currentState = MENU_PRINCIPAL;
    updateLCD();
    return;
  }

  // Inserção de Dados Numéricos e Apagar (D)
  if ((key >= '0' && key <= '9') || key == 'D') {
    // Trata a seleção da linha (1 a 9) diretamente
    if (currentState == INSERIR_LINE) {
      if (key >= '1' && key <= '9') {
        currentLine = key - '0';
        updateLCD();
      }
      return; // Bloqueia outros números (ex: 0) ou apagar (D) na seleção de
              // linha
    }

    String *activeStr = nullptr;

    if (currentState == INSERIR_STEPS)
      activeStr = &valPassos;
    else if (currentState == INSERIR_SPEED)
      activeStr = &valVelocidade;
    else if (currentState == INSERIR_MOTOR)
      activeStr = &valMotor;

    if (activeStr != nullptr) {
      if (key == 'D') {
        if (activeStr->length() > 0) {
          activeStr->remove(activeStr->length() - 1);
        } else if (currentState == INSERIR_STEPS) {
          *activeStr = "-"; // Transforma em anti-horário
        }
      } else {
        if (activeStr->length() < 8) {
          *activeStr += key;
        }
      }
      updateLCD();
    }
  }
}

void updateLCD() {
  lcd.clear();
  lcd.setCursor(0, 0);

  switch (currentState) {
  case MENU_PRINCIPAL:
    lcd.print("L:");
    lcd.print(currentLine);
    lcd.print(" [A]=Config");
    lcd.setCursor(0, 1);

    if (runTarget > 0) {
      lcd.print("L:");
      lcd.print(runTarget);
      lcd.print(" *=RUN #=DEL");
    } else {
      lcd.print(lastStatus.substring(0, 16));
    }
    break;

  case INSERIR_LINE:
    lcd.print("Line Select:");
    lcd.setCursor(0, 1);
    lcd.print("> ");
    lcd.print(currentLine);
    lcd.blink();
    break;

  case INSERIR_STEPS:
    lcd.print("Steps (L:");
    lcd.print(currentLine);
    lcd.print(")");
    lcd.setCursor(0, 1);
    lcd.print("> ");
    lcd.print(valPassos);
    lcd.blink();
    break;

  case INSERIR_SPEED:
    lcd.print("Speed (us):");
    lcd.setCursor(0, 1);
    lcd.print("> ");
    lcd.print(valVelocidade);
    lcd.blink();
    break;

  case INSERIR_MOTOR:
    lcd.print("Motor (1 or 2):");
    lcd.setCursor(0, 1);
    lcd.print("> ");
    lcd.print(valMotor);
    lcd.blink();
    break;
  }

  if (currentState == MENU_PRINCIPAL) {
    lcd.noBlink();
  }
}

void processIncomingMessage() {
  if (mainSerial.available()) {
    String msg = mainSerial.readStringUntil('\n');
    msg.trim();
    if (msg.length() > 0) {
      if (msg.indexOf("Iniciando") != -1) {
        isRunning = true;
      } else if (msg.indexOf("Parando") != -1 ||
                 msg.indexOf("concluidas") != -1) {
        isRunning = false;
      }

      if (runTarget == 0) {
        lastStatus = "RX:" + msg;
        if (currentState == MENU_PRINCIPAL && !isRunning) {
          updateLCD();
        }
      }
    }
  }
}