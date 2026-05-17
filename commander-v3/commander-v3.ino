/*
 * =================================================================================
 * Stepper Commander - Interface de Menu Interativo LCD com EEPROM Local
 * =================================================================================
 * Board: Arduino Uno / Nano (Atmega328P)
 * Função: Interface IHM para construir, armazenar e enviar comandos.
 * * * GUIA DE USO DO MENU:
 * [A] - NAVEGAR: Alterna entre os menus (Line -> Steps -> Speed -> Motor ->
 * Disable -> Início). [B] - (Livre para uso futuro) [C] - (Livre para uso
 * futuro) [D] - APAGAR (Backspace): Apaga o último número. No menu principal,
 * limpa a seleção.
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
#define EEPROM_MAGIC_BYTE 0x43
#define EEPROM_MAGIC_ADDR 1000

struct SavedLine {
  bool active;
  char command[40];
};
SavedLine savedLines[9];

// --- Máquina de Estados do Menu ---
enum MenuState {
  MENU_PRINCIPAL,
  INSERIR_LINE,
  INSERIR_STEPS,
  INSERIR_SPEED,
  INSERIR_MOTOR,
  INSERIR_DISABLE
};
MenuState currentState = MENU_PRINCIPAL;

// --- Variáveis de Construção e Execução ---
int currentLine = 1; // Linha atual em edição (1 a 9)
int runTarget = 0; // 0 = Todas as linhas; 1 a 9 = Linha específica para RUN/DEL
String valPassos = "";
String valVelocidade = "";
String valMotor = "";
String valDisable = ""; // Parâmetro 15 (1=Yes(15:0), 2=No(15:1))
String lastStatus = "Ready.";

bool isRunning = false;
bool wasRunning = false;
int animDirection =
    1; // 1 = Positivo (Esq. para Dir.), -1 = Negativo (Dir. para Esq.)

// --- Custom Chars para Animação de Setas ---
// Seta apontando para a Esquerda (<)
byte arrowLeft[8] = {0b00001, 0b00011, 0b00111, 0b01111,
                     0b01111, 0b00111, 0b00011, 0b00001};

// Seta apontando para a Direita (>)
byte arrowRight[8] = {0b10000, 0b11000, 0b11100, 0b11110,
                      0b11110, 0b11100, 0b11000, 0b10000};

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
  lcd.createChar(0, arrowLeft);
  lcd.createChar(1, arrowRight);

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
    lcd.setCursor(0, 0);
    lcd.print(lastStatus); // Exibe o alvo da execução (Ex: RUN L:ALL)
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

void drawAnimation() {
  // Atualiza as posições a cada 150 milissegundos para um movimento fluido
  if (millis() - lastAnimTime > 150) {
    lastAnimTime = millis();
    animFrame++;
    if (animFrame > 15)
      animFrame = 0; // O display tem 16 colunas (0 a 15)

    lcd.setCursor(0, 1);
    for (int i = 0; i < 16; i++) {
      bool isArrow = false;

      if (animDirection == -1) {
        // --- MOTOR ANTI-HORÁRIO (Negativo) ---
        // Seta apontando para direita (>) com movimento da Esquerda para a
        // Direita
        int head = animFrame % 16;
        if (i == head || i == (head - 1 + 16) % 16 || i == (head - 2 + 16) % 16)
          isArrow = true;

        if (isArrow)
          lcd.write(byte(1));
        else
          lcd.print(" ");

      } else {
        // --- MOTOR HORÁRIO (Positivo) ---
        // Seta apontando para esquerda (<) com movimento da Direita para a
        // Esquerda
        int head = (15 - (animFrame % 16)) % 16;
        if (i == head || i == (head + 1) % 16 || i == (head + 2) % 16)
          isArrow = true;

        if (isArrow)
          lcd.write(byte(0));
        else
          lcd.print(" ");
      }
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
      animDirection =
          1; // Reseta para a direção Padrão (Positivo / Direita-Esquerda)

      if (runTarget == 0) {
        for (int i = 0; i < 9; i++) {
          if (savedLines[i].active) {
            // Descobre a direção baseando-se na linha armazenada
            if (String(savedLines[i].command).indexOf("10:-") != -1)
              animDirection = -1;
            mainSerial.println(savedLines[i].command);
            delay(60);
            sentAnything = true;
          }
        }
      } else {
        int idx = runTarget - 1;
        if (savedLines[idx].active) {
          if (String(savedLines[idx].command).indexOf("10:-") != -1)
            animDirection = -1;
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

    // Processamento do novo parâmetro Disable (1=Yes(15:0), 2=No(15:1))
    if (valDisable == "1")
      cmdOut += ",15:0";
    else if (valDisable == "2")
      cmdOut += ",15:1";

    int idx = currentLine - 1;
    savedLines[idx].active = true;
    cmdOut.toCharArray(savedLines[idx].command, 40);
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
    valDisable = ""; // Limpa a escolha para a próxima linha
    runTarget = 0;
    currentState = MENU_PRINCIPAL;
    updateLCD();
    return;
  }

  // ==========================================
  // NAVEGAÇÃO CICLICA E EDIÇÃO (A, D)
  // ==========================================

  if (key == 'A') {
    runTarget = 0;
    if (currentState == MENU_PRINCIPAL)
      currentState = INSERIR_LINE;
    else if (currentState == INSERIR_LINE)
      currentState = INSERIR_STEPS;
    else if (currentState == INSERIR_STEPS)
      currentState = INSERIR_SPEED;
    else if (currentState == INSERIR_SPEED)
      currentState = INSERIR_MOTOR;
    else if (currentState == INSERIR_MOTOR)
      currentState = INSERIR_DISABLE;
    else if (currentState == INSERIR_DISABLE)
      currentState = MENU_PRINCIPAL;
    updateLCD();
    return;
  }

  if ((key >= '0' && key <= '9') || key == 'D') {
    if (currentState == INSERIR_LINE) {
      if (key >= '1' && key <= '9') {
        currentLine = key - '0';
        updateLCD();
      }
      return;
    }

    String *activeStr = nullptr;

    if (currentState == INSERIR_STEPS)
      activeStr = &valPassos;
    else if (currentState == INSERIR_SPEED)
      activeStr = &valVelocidade;
    else if (currentState == INSERIR_MOTOR)
      activeStr = &valMotor;
    else if (currentState == INSERIR_DISABLE)
      activeStr = &valDisable;

    if (activeStr != nullptr) {
      if (key == 'D') {
        if (activeStr->length() > 0) {
          activeStr->remove(activeStr->length() - 1);
        } else if (currentState == INSERIR_STEPS) {
          *activeStr = "-"; // Transforma em anti-horário
        }
      } else {
        if (currentState == INSERIR_DISABLE || currentState == INSERIR_MOTOR) {
          if (key == '1' || key == '2') {
            *activeStr = key;
          }
        } else {
          if (activeStr->length() < 8) {
            *activeStr += key;
          }
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

  case INSERIR_DISABLE:
    lcd.print("Disable? 1:Y 2:N");
    lcd.setCursor(0, 1);
    lcd.print("> ");
    lcd.print(valDisable);
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