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

// Descomente para ativar prints de debug via Serial USB
// #define DEBUG_SERIAL

#ifdef DEBUG_SERIAL
  #define DBG(x) Serial.println(F(x))
  #define DBG_VAR(prefix, var) do { Serial.print(F(prefix)); Serial.println(var); } while(0)
#else
  #define DBG(x)
  #define DBG_VAR(prefix, var)
#endif

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
// NOTA: Magic byte alterado de 0x43 para 0x44 devido à mudança no tamanho do struct
#define EEPROM_MAGIC_BYTE 0x44
#define EEPROM_MAGIC_ADDR 1000

struct SavedLine {
  char command[34];
  bool active;
};
SavedLine savedLines[9];

// --- Máquina de Estados do Menu ---
enum MenuState : uint8_t {
  MENU_PRINCIPAL,
  INSERIR_LINE,
  INSERIR_STEPS,
  INSERIR_SPEED,
  INSERIR_MOTOR,
  INSERIR_DISABLE
};
MenuState currentState = MENU_PRINCIPAL;

// --- Variáveis de Construção e Execução ---
int8_t currentLine = 1;    // Linha atual em edição (1 a 9)
int8_t runTarget = 0;      // 0 = Todas as linhas; 1 a 9 = Linha específica para RUN/DEL

char valPassos[10];
uint8_t lenPassos = 0;

char valVelocidade[6];
uint8_t lenVelocidade = 0;

char valMotor = 0;         // '1' ou '2', 0 = não definido
char valDisable = 0;       // '1' ou '2', 0 = não definido (Parâmetro 15: 1=Yes(15:0), 2=No(15:1))

char lastStatus[17];       // LCD = 16 cols + null

bool isRunning = false;
bool wasRunning = false;
int8_t animDirection = 1;  // 1 = Positivo (Esq. para Dir.), -1 = Negativo (Dir. para Esq.)

// --- Custom Chars para Animação de Setas (PROGMEM) ---
// Seta apontando para a Esquerda (<)
const byte arrowLeft[] PROGMEM = {0b00001, 0b00011, 0b00111, 0b01111,
                                  0b01111, 0b00111, 0b00011, 0b00001};

// Seta apontando para a Direita (>)
const byte arrowRight[] PROGMEM = {0b10000, 0b11000, 0b11100, 0b11110,
                                   0b11110, 0b11100, 0b11000, 0b10000};

// --- Protótipos ---
void initEEPROM();
void updateLCD();
void handleKeyPress(char key);
void processIncomingMessage();
void drawAnimation();

// --- Helpers para buffers char[] ---
static void bufAppend(char *buf, uint8_t &len, uint8_t maxLen, char c) {
  if (len < maxLen - 1) {
    buf[len++] = c;
    buf[len] = '\0';
  }
}

static void bufBackspace(char *buf, uint8_t &len) {
  if (len > 0) {
    buf[--len] = '\0';
  }
}

static void bufClear(char *buf, uint8_t &len) {
  buf[0] = '\0';
  len = 0;
}

void setup() {
  Serial.begin(9600);
  mainSerial.begin(9600);

  lcd.init();
  lcd.backlight();

  // Carregamento dos caracteres customizados na VRAM do LCD (leitura PROGMEM)
  byte buf[8];
  memcpy_P(buf, arrowLeft, 8);
  lcd.createChar(0, buf);
  memcpy_P(buf, arrowRight, 8);
  lcd.createChar(1, buf);

  // Inicializa buffers
  valPassos[0] = '\0';
  valVelocidade[0] = '\0';
  strncpy_P(lastStatus, PSTR("Ready."), sizeof(lastStatus));

  initEEPROM();

  DBG("Commander Menu Iniciado (Com EEPROM).");
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
    for (uint8_t i = 0; i < 9; i++) {
      savedLines[i].active = false;
      memset(savedLines[i].command, 0, sizeof(savedLines[i].command));
      EEPROM.put(i * sizeof(SavedLine), savedLines[i]);
    }
    EEPROM.write(EEPROM_MAGIC_ADDR, EEPROM_MAGIC_BYTE);
    DBG("EEPROM formatada para uso do Commander.");
  } else {
    for (uint8_t i = 0; i < 9; i++) {
      EEPROM.get(i * sizeof(SavedLine), savedLines[i]);
    }
    DBG("Dados carregados da EEPROM local.");
  }
}

unsigned long lastAnimTime = 0;
uint8_t animFrame = 0;

void drawAnimation() {
  // Atualiza as posições a cada 150 milissegundos para um movimento fluido
  if (millis() - lastAnimTime <= 150) return;
  lastAnimTime = millis();
  animFrame = (animFrame + 1) & 0x0F; // mod 16 via bitmask

  lcd.setCursor(0, 1);
  uint8_t charIdx = (animDirection == -1) ? 1 : 0;
  int8_t head = (animDirection == -1)
    ? (int8_t)(animFrame & 0x0F)
    : (int8_t)((15 - (animFrame & 0x0F)) & 0x0F);

  for (uint8_t i = 0; i < 16; i++) {
    bool isArrow = (i == head) ||
                   (i == ((head - animDirection + 16) % 16)) ||
                   (i == ((head - 2 * animDirection + 16) % 16));

    if (isArrow)
      lcd.write(charIdx);
    else
      lcd.print(' ');
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
      mainSerial.println(F("02"));
      strncpy_P(lastStatus, PSTR("Cmd: 02 (STOP)"), sizeof(lastStatus));
      isRunning = false;
      runTarget = 0;
    } else {
      bool sentAnything = false;
      animDirection =
          1; // Reseta para a direção Padrão (Positivo / Direita-Esquerda)

      if (runTarget == 0) {
        for (uint8_t i = 0; i < 9; i++) {
          if (savedLines[i].active) {
            // Descobre a direção baseando-se na linha armazenada
            if (strstr(savedLines[i].command, "10:-"))
              animDirection = -1;
            mainSerial.println(savedLines[i].command);
            delay(60);
            sentAnything = true;
          }
        }
      } else {
        uint8_t idx = runTarget - 1;
        if (savedLines[idx].active) {
          if (strstr(savedLines[idx].command, "10:-"))
            animDirection = -1;
          mainSerial.println(savedLines[idx].command);
          delay(60);
          sentAnything = true;
        }
      }

      if (sentAnything) {
        mainSerial.println(F("01"));
        if (runTarget == 0) {
          strncpy_P(lastStatus, PSTR("RUN L:ALL"), sizeof(lastStatus));
        } else {
          snprintf_P(lastStatus, sizeof(lastStatus), PSTR("RUN L:%d"), runTarget);
        }
        isRunning = true;
      } else {
        strncpy_P(lastStatus, PSTR("Err: Empty!"), sizeof(lastStatus));
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
      uint8_t idx = runTarget - 1;

      savedLines[idx].active = false;
      memset(savedLines[idx].command, 0, sizeof(savedLines[idx].command));
      EEPROM.put(idx * sizeof(SavedLine), savedLines[idx]);

      snprintf_P(lastStatus, sizeof(lastStatus), PSTR("Deleted: L%d"), runTarget);
      runTarget = 0;
      updateLCD();
      return;
    }

    if (lenPassos == 0)
      strncpy(valPassos, "0", sizeof(valPassos));
    else if (lenPassos == 1 && valPassos[0] == '-')
      strncpy(valPassos, "-0", sizeof(valPassos));

    char cmdOut[34];
    int pos = snprintf(cmdOut, sizeof(cmdOut), "%d-10:%s", currentLine, valPassos);
    if (lenVelocidade > 0)
      pos += snprintf(cmdOut + pos, sizeof(cmdOut) - pos, ",11:%s", valVelocidade);
    if (valMotor != 0)
      pos += snprintf(cmdOut + pos, sizeof(cmdOut) - pos, ",14:%c", valMotor);

    // Processamento do novo parâmetro Disable (1=Yes(15:0), 2=No(15:1))
    if (valDisable == '1')
      pos += snprintf(cmdOut + pos, sizeof(cmdOut) - pos, ",15:0");
    else if (valDisable == '2')
      pos += snprintf(cmdOut + pos, sizeof(cmdOut) - pos, ",15:1");

    uint8_t idx = currentLine - 1;
    savedLines[idx].active = true;
    strncpy(savedLines[idx].command, cmdOut, sizeof(savedLines[idx].command));
    savedLines[idx].command[sizeof(savedLines[idx].command) - 1] = '\0';
    EEPROM.put(idx * sizeof(SavedLine), savedLines[idx]);

    mainSerial.println(cmdOut);
    DBG_VAR("Gravado e Enviado: ", cmdOut);

    snprintf_P(lastStatus, sizeof(lastStatus), PSTR("Saved: L%d"), currentLine);

    currentLine++;
    if (currentLine > 9)
      currentLine = 1;

    bufClear(valPassos, lenPassos);
    bufClear(valVelocidade, lenVelocidade);
    valMotor = 0;
    valDisable = 0; // Limpa a escolha para a próxima linha
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

    if (currentState == INSERIR_STEPS) {
      if (key == 'D') {
        if (lenPassos > 0) {
          bufBackspace(valPassos, lenPassos);
        } else {
          valPassos[0] = '-'; // Transforma em anti-horário
          valPassos[1] = '\0';
          lenPassos = 1;
        }
      } else {
        bufAppend(valPassos, lenPassos, sizeof(valPassos), key);
      }
      updateLCD();
    } else if (currentState == INSERIR_SPEED) {
      if (key == 'D') {
        bufBackspace(valVelocidade, lenVelocidade);
      } else {
        bufAppend(valVelocidade, lenVelocidade, sizeof(valVelocidade), key);
      }
      updateLCD();
    } else if (currentState == INSERIR_MOTOR) {
      if (key == 'D') {
        valMotor = 0;
      } else if (key == '1' || key == '2') {
        valMotor = key;
      }
      updateLCD();
    } else if (currentState == INSERIR_DISABLE) {
      if (key == 'D') {
        valDisable = 0;
      } else if (key == '1' || key == '2') {
        valDisable = key;
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
    lcd.print(F("L:"));
    lcd.print(currentLine);
    lcd.print(F(" [A]=Config"));
    lcd.setCursor(0, 1);

    if (runTarget > 0) {
      lcd.print(F("L:"));
      lcd.print(runTarget);
      lcd.print(F(" *=RUN #=DEL"));
    } else {
      lcd.print(lastStatus);
    }
    break;

  case INSERIR_LINE:
    lcd.print(F("Line Select:"));
    lcd.setCursor(0, 1);
    lcd.print(F("> "));
    lcd.print(currentLine);
    lcd.blink();
    break;

  case INSERIR_STEPS:
    lcd.print(F("Steps (L:"));
    lcd.print(currentLine);
    lcd.print(F(")"));
    lcd.setCursor(0, 1);
    lcd.print(F("> "));
    lcd.print(valPassos);
    lcd.blink();
    break;

  case INSERIR_SPEED:
    lcd.print(F("Speed (us):"));
    lcd.setCursor(0, 1);
    lcd.print(F("> "));
    lcd.print(valVelocidade);
    lcd.blink();
    break;

  case INSERIR_MOTOR:
    lcd.print(F("Motor (1 or 2):"));
    lcd.setCursor(0, 1);
    lcd.print(F("> "));
    if (valMotor != 0) lcd.print(valMotor);
    lcd.blink();
    break;

  case INSERIR_DISABLE:
    lcd.print(F("Disable? 1:Y 2:N"));
    lcd.setCursor(0, 1);
    lcd.print(F("> "));
    if (valDisable != 0) lcd.print(valDisable);
    lcd.blink();
    break;
  }

  if (currentState == MENU_PRINCIPAL) {
    lcd.noBlink();
  }
}

void processIncomingMessage() {
  if (mainSerial.available()) {
    char msgBuf[32];
    uint8_t msgLen = 0;
    while (mainSerial.available() && msgLen < sizeof(msgBuf) - 1) {
      char c = mainSerial.read();
      if (c == '\n') break;
      msgBuf[msgLen++] = c;
    }
    msgBuf[msgLen] = '\0';
    // trim trailing \r
    if (msgLen > 0 && msgBuf[msgLen - 1] == '\r') msgBuf[--msgLen] = '\0';

    if (msgLen > 0) {
      if (strstr_P(msgBuf, PSTR("Iniciando"))) {
        isRunning = true;
      } else if (strstr_P(msgBuf, PSTR("Parando")) ||
                 strstr_P(msgBuf, PSTR("concluidas"))) {
        isRunning = false;
      }

      if (runTarget == 0) {
        snprintf_P(lastStatus, sizeof(lastStatus), PSTR("RX:%.13s"), msgBuf);
        if (currentState == MENU_PRINCIPAL && !isRunning) {
          updateLCD();
        }
      }
    }
  }
}