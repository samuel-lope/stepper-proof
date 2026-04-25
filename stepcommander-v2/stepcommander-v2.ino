/*
 * =================================================================================
 * Stepper Commander - Interface H8P (v2.1 - Fully Optimized)
 * =================================================================================
 *
 * Board: Arduino Uno / Nano (Atmega328P)
 * Função: Envio de comandos H8P e recebimento de alertas do controlador principal
 *
 * Arquitetura:
 * - Leitura Serial Não-Bloqueante: char-por-char via buffer estático C.
 * - Multitarefa Cooperativa: millis() como timer base para todos os processos.
 * - RAM Otimizada: char[] fixos no lugar de String (elimina heap fragmentation).
 *   Uso de PROGMEM/PSTR() para strings constantes e bitfields para flags.
 *
 * Melhorias v2.1 aplicadas:
 * - [OPT-1] Leitura EEPROM com EEPROM.get() (tipo-seguro).
 * - [OPT-2] scrollIndexBottom promovido para uint16_t (evita overflow 255→0).
 * - [OPT-3] Timeout de mensagem de status (restaura "Pronto." após inatividade).
 * - [OPT-4] Validação de slot EEPROM virgem (0xFF), evita envio de lixo serial.
 * - [OPT-5] String substituída por char[] fixo (elimina fragmentação de heap).
 * - [OPT-6] Numeração de seções corrigida (era "10", agora "9").
 * - [OPT-7] Diagnóstico de slots EEPROM no setup() via Serial.
 * - [OPT-8] Scroll adaptativo: velocidade proporcional ao comprimento da mensagem.
 *
 * Periféricos:
 * - Teclado Matricial 4x4 (Linhas 5,4,3,2 / Colunas 9,8,7,6)
 * - Display LCD 16x2 I2C (SDA=A4, SCL=A5, Endereço: 0x27)
 * - SoftwareSerial (RX=10, TX=11) -> Conecta ao TX/RX da placa Principal
 * - Opcional: TM1638plus no barramento SPI emulado por software (A0, A1, A2)
 *
 * Mapeamento de Teclas:
 * - '*'      -> ':'          | '#'      -> ','
 * - '* + #'  -> ENTER        | '* + C'  -> Clear
 * - '* + D'  -> Backspace    | '* + A'  -> Inserir '-'
 * - '* + B'  -> Toggle Fast Act Mode
 * - '* + A + C + [0-9]' -> Salvar comando no Slot EEPROM
 * - No modo Fast Act: [0-9] executa o Slot correspondente
 * =================================================================================
 */

#include <Arduino.h>
#include <EEPROM.h>
#include <Keypad.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>
#include <TM1638plus.h>
#include <Wire.h>

// =================================================================================
// 1. MACROS E DEFINIÇÕES (HARDWARE & CONFIG)
// =================================================================================
#define MAX_INPUT_LEN 32
#define SERIAL_BUF_SIZE 32
#define MSG_MAX_LEN 32
#define SCROLL_INTERVAL_BASE 500 // [OPT-8] Intervalo base do scroll em ms
#define MSG_TIMEOUT_MS 8000      // [OPT-3] Tempo para mensagem de status expirar

// Configurações da EEPROM (10 Slots de Macro)
#define EEPROM_START_ADDR 0
#define SLOT_SIZE 33 // 32 chars + null terminator
#define NUM_SLOTS 10

// Display LCD I2C (Endereço 0x27, 16 Colunas, 2 Linhas)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Comunicação Serial H8P (pinos físicos fixos)
SoftwareSerial mainSerial(10, 11);

// Módulo TM1638 (7 segmentos - opcional)
#define TM_STB A0
#define TM_CLK A1
#define TM_DIO A2
TM1638plus tm(TM_STB, TM_CLK, TM_DIO, false);

// Matriz do Teclado 4x4
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}};
byte rowPins[ROWS] = {5, 4, 3, 2};
byte colPins[COLS] = {9, 8, 7, 6};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// =================================================================================
// 2. MÁQUINA DE ESTADOS E GERENCIAMENTO DE MEMÓRIA (RAM)
// =================================================================================

// Bitfields: 4 flags em 1 único byte de RAM
struct
{
    uint8_t lcdNeedsUpdate : 1;
    uint8_t tmNeedsUpdate : 1;
    uint8_t isMessageLong : 1;
    uint8_t isFastActMode : 1;
    uint8_t msgIsStatus : 1; // [OPT-3] Mensagem temporária (pode expirar)
} systemFlags;

// [OPT-5] Buffers estáticos char[] — sem String, sem heap, sem fragmentação
char inputBuffer[MAX_INPUT_LEN + 1]; // Buffer de entrada do teclado
uint8_t inputLen = 0;                // Comprimento atual do buffer de entrada
char lastCommand[MAX_INPUT_LEN + 1]; // Último comando enviado (repetição com *+#)
char currentMsg[MSG_MAX_LEN + 1];    // Mensagem exibida na linha 2 do LCD

// Controle do Marquee (scroll cooperativo via millis)
unsigned long lastScrollTime = 0;
uint16_t scrollIndexBottom = 0; // [OPT-2] uint16_t: evita overflow a cada ~100s

// [OPT-3] Controle de timeout de mensagem de status
unsigned long lastMsgTime = 0;

// Estado do combo *+A+C+[digit] para salvar no EEPROM
// 0=idle, 1=recebeu *+A, 2=recebeu *+A+C (aguarda dígito)
uint8_t saveComboState = 0;

// Buffer estático para recepção Serial não-bloqueante
char rxBuffer[SERIAL_BUF_SIZE];
uint8_t rxIndex = 0;

// =================================================================================
// 3. PROTÓTIPOS
// =================================================================================
void processSerialRX();
void parseH8PMessage(const char *msg);
void processKeypad();
void handleDisplayTasks();
void updateLCD();
void updateTM1638();
void saveSlot(uint8_t slot);
void runSlot(uint8_t slot);
void setStatusMsg(const char *msg);
void scrollTextToBuffer(char *out, const char *text, uint8_t textLen, uint16_t idx);
void diagnosticEEPROM(); // [OPT-7]

// =================================================================================
// 4. SETUP INICIAL
// =================================================================================
void setup()
{
    Serial.begin(9600);
    mainSerial.begin(9600);

    lcd.init();
    lcd.backlight();
    // tm.displayBegin(); // (Opcional)
    // tm.reset();        // (Opcional)

    // Estado inicial limpo
    inputBuffer[0] = '\0';
    lastCommand[0] = '\0';
    strcpy_P(currentMsg, PSTR("Pronto."));

    systemFlags.lcdNeedsUpdate = 1;
    systemFlags.tmNeedsUpdate = 0;
    systemFlags.isMessageLong = 0;
    systemFlags.isFastActMode = 0;
    systemFlags.msgIsStatus = 0;

    Serial.println(F("StepCommander v2.1 [Optimized]"));

    diagnosticEEPROM(); // [OPT-7] Exibe slots gravados ao iniciar
}

// =================================================================================
// 5. LOOP PRINCIPAL (COOPERATIVE MULTITASKING)
// =================================================================================
void loop()
{
    processSerialRX();    // 1. Lê bytes da UART sem bloquear
    processKeypad();      // 2. Escaneia o teclado matricial
    handleDisplayTasks(); // 3. Atualiza displays (sob demanda / timer)
}

// =================================================================================
// 6. PROCESSAMENTO SERIAL NÃO-BLOQUEANTE
// =================================================================================
void processSerialRX()
{
    while (mainSerial.available() > 0)
    {
        char inChar = (char)mainSerial.read();

        if (inChar == '\n' || inChar == '\r')
        {
            if (rxIndex > 0)
            {
                rxBuffer[rxIndex] = '\0';
                parseH8PMessage(rxBuffer);
                rxIndex = 0;
            }
        }
        else if (rxIndex < (SERIAL_BUF_SIZE - 1))
        {
            rxBuffer[rxIndex++] = inChar;
        }
    }
}

void parseH8PMessage(const char *msg)
{
    Serial.print(F("RX: "));
    Serial.println(msg);

    // Dicionário H8P com PSTR (strings em Flash, não consomem SRAM)
    if (strncmp(msg, "A0", 2) == 0)
        strcpy_P(currentMsg, PSTR("Sis Inicializado"));
    else if (strncmp(msg, "B0", 2) == 0)
        strcpy_P(currentMsg, PSTR("Iniciando Fila"));
    else if (strncmp(msg, "B1", 2) == 0)
        strcpy_P(currentMsg, PSTR("Motor Parado"));
    else if (strncmp(msg, "B2", 2) == 0)
        strcpy_P(currentMsg, PSTR("Pausa Global"));
    else if (strncmp(msg, "B4", 2) == 0)
        strcpy_P(currentMsg, PSTR("Repeat ON"));
    else if (strncmp(msg, "B5", 2) == 0)
        strcpy_P(currentMsg, PSTR("Fila Executada"));
    else if (strncmp(msg, "B6", 2) == 0)
        strcpy_P(currentMsg, PSTR("Repeat OFF"));
    else if (strncmp(msg, "B7", 2) == 0)
        strcpy_P(currentMsg, PSTR("Motor Habilitado"));
    else if (strncmp(msg, "B8", 2) == 0)
        strcpy_P(currentMsg, PSTR("Motor Desabiltd."));
    else if (strncmp(msg, "B9", 2) == 0 ||
             strncmp(msg, "BA", 2) == 0)
        strcpy_P(currentMsg, PSTR("Preset EEPROM"));
    else if (strncmp(msg, "E0", 2) == 0)
        strcpy_P(currentMsg, PSTR("Err: Em Execucao"));
    else if (strncmp(msg, "E1", 2) == 0)
        strcpy_P(currentMsg, PSTR("Err: Fila Vazia"));
    else if (strncmp(msg, "E2", 2) == 0)
        strcpy_P(currentMsg, PSTR("Err: Fila Cheia"));
    else if (strncmp(msg, "E3", 2) == 0)
        strcpy_P(currentMsg, PSTR("Erro de Sintaxe"));
    else if (strncmp(msg, "E4", 2) == 0)
        strcpy_P(currentMsg, PSTR("Preset Invalido"));
    else if (strncmp(msg, "BB", 2) == 0 ||
             strncmp(msg, "BC", 2) == 0)
        strcpy_P(currentMsg, PSTR("FastAct. Exec"));
    else if (strncmp(msg, "C0", 2) == 0)
        snprintf_P(currentMsg, sizeof(currentMsg), PSTR("Fila: Linha %c"), msg[3]);
    else
    {
        strncpy(currentMsg, msg, MSG_MAX_LEN);
        currentMsg[MSG_MAX_LEN] = '\0';
    }

    Serial.print(F("-> "));
    Serial.println(currentMsg);

    systemFlags.isMessageLong = (strlen(currentMsg) > 16) ? 1 : 0;
    systemFlags.msgIsStatus = 0; // Mensagem H8P não expira por timeout
    scrollIndexBottom = 0;
    lastMsgTime = millis();
    systemFlags.lcdNeedsUpdate = 1;
}

// =================================================================================
// 7. LÓGICA DE INTERFACE: TECLADO (EVENTOS)
// =================================================================================
void processKeypad()
{
    char key = keypad.getKey();
    if (!key)
        return;

    Serial.print(F("Tecla: "));
    Serial.println(key);

    // Modo Fast Act: teclas numéricas disparam macros diretamente
    if (systemFlags.isFastActMode && isDigit(key))
    {
        runSlot(key - '0');
        return;
    }

    // --- Combo *+A+C+[digit] para salvar no EEPROM ---
    if (saveComboState == 1)
    {
        // Recebeu *+A, agora espera 'C' para continuar combo
        if (key == 'C')
        {
            saveComboState = 2; // Avança: aguarda dígito
            systemFlags.lcdNeedsUpdate = 1;
            return;
        }
        else
        {
            // Combo cancelado — aplica ação original do *+A (inserir '-')
            saveComboState = 0;
            if (inputLen < MAX_INPUT_LEN)
            {
                inputBuffer[inputLen++] = '-';
                inputBuffer[inputLen] = '\0';
            }
            // Continua processando a tecla atual normalmente (cai no fluxo abaixo)
        }
    }
    else if (saveComboState == 2)
    {
        // Recebeu *+A+C, agora espera dígito para salvar
        if (isDigit(key))
        {
            saveComboState = 0;
            if (inputLen > 0)
            {
                saveSlot(key - '0');
                inputLen = 0;
                inputBuffer[0] = '\0';
            }
            else
            {
                setStatusMsg("Buffer Vazio!");
            }
            systemFlags.lcdNeedsUpdate = 1;
            return;
        }
        else
        {
            // Combo cancelado — não há ação pendente a replay
            saveComboState = 0;
            // Continua processando a tecla atual normalmente
        }
    }

    // Verifica se o prefixo '*' está ativo (buffer termina com ':')
    bool hasStar = (inputLen > 0 && inputBuffer[inputLen - 1] == ':');

    if (hasStar)
    {
        if (key == '#')
        {
            // [* + #] = ENTER — Remove ':' e envia o comando
            inputBuffer[--inputLen] = '\0';

            if (inputLen == 0 && lastCommand[0] != '\0')
            {
                strncpy(inputBuffer, lastCommand, MAX_INPUT_LEN);
                inputLen = strlen(inputBuffer);
            }
            else if (inputLen > 0)
            {
                strncpy(lastCommand, inputBuffer, MAX_INPUT_LEN);
                lastCommand[MAX_INPUT_LEN] = '\0';
            }

            if (inputLen > 0)
            {
                mainSerial.println(inputBuffer);
                Serial.print(F("TX: "));
                Serial.println(inputBuffer);
            }
            inputLen = 0;
            inputBuffer[0] = '\0';
        }
        else if (key == 'B')
        {
            // [* + B] = Toggle Fast Act Mode
            inputBuffer[--inputLen] = '\0';
            systemFlags.isFastActMode = !systemFlags.isFastActMode;
            setStatusMsg(systemFlags.isFastActMode ? "FAST ACT ON" : "FAST ACT OFF");
        }
        else if (key == 'C')
        {
            // [* + C] = Clear
            inputLen = 0;
            inputBuffer[0] = '\0';
        }
        else if (key == 'D')
        {
            // [* + D] = Backspace — Remove ':' e o caractere anterior
            if (inputLen >= 2)
                inputBuffer[inputLen -= 2] = '\0';
            else
            {
                inputLen = 0;
                inputBuffer[0] = '\0';
            }
        }
        else if (key == 'A')
        {
            // [* + A] = Inicia combo de salvar (*+A+C+[digit])
            // Remove o ':' do buffer e entra no estado de combo
            inputBuffer[--inputLen] = '\0';
            saveComboState = 1;
        }
        else if (key == '*')
        {
            // Duplo '*': adiciona mais um ':'
            if (inputLen < MAX_INPUT_LEN)
            {
                inputBuffer[inputLen++] = ':';
                inputBuffer[inputLen] = '\0';
            }
        }
        else if (isDigit(key))
        {
            // Dígito após '*': mantém ':' e adiciona o dígito
            if (inputLen < MAX_INPUT_LEN)
            {
                inputBuffer[inputLen++] = key;
                inputBuffer[inputLen] = '\0';
            }
        }
        else
        {
            // Tecla normal após '*': mantém ':' e adiciona o caractere
            if (inputLen < MAX_INPUT_LEN)
            {
                inputBuffer[inputLen++] = key;
                inputBuffer[inputLen] = '\0';
            }
        }
    }
    else
    {
        // Pressionamento convencional (sem prefixo '*' ativo)
        char toAppend = 0;
        if (key == '#')
            toAppend = ',';
        else if (key == '*')
            toAppend = ':';
        else
            toAppend = key;

        if (toAppend && inputLen < MAX_INPUT_LEN)
        {
            inputBuffer[inputLen++] = toAppend;
            inputBuffer[inputLen] = '\0';
        }
    }

    systemFlags.lcdNeedsUpdate = 1;
}

// =================================================================================
// 8. GERENCIADOR DE DISPLAYS (RENDERIZADOR COOPERATIVO)
// =================================================================================
void handleDisplayTasks()
{
    unsigned long now = millis();

    // [OPT-3] Timeout: restaura mensagem padrão após inatividade
    if (systemFlags.msgIsStatus && (now - lastMsgTime >= MSG_TIMEOUT_MS))
    {
        strcpy_P(currentMsg, systemFlags.isFastActMode ? PSTR("[FAST ACT]") : PSTR("Pronto."));
        systemFlags.isMessageLong = 0;
        systemFlags.msgIsStatus = 0;
        scrollIndexBottom = 0;
        systemFlags.lcdNeedsUpdate = 1;
    }

    // [OPT-8] Scroll com velocidade adaptativa ao comprimento da mensagem
    if (systemFlags.isMessageLong)
    {
        uint8_t msgLen = strlen(currentMsg);
        // Quanto mais longa a mensagem, mais rápido o scroll (mín. 200ms)
        uint16_t interval = (uint16_t)max(200, SCROLL_INTERVAL_BASE - (int)(msgLen * 10));
        if (now - lastScrollTime >= interval)
        {
            lastScrollTime = now;
            scrollIndexBottom++;
            systemFlags.lcdNeedsUpdate = 1;
        }
    }

    if (systemFlags.lcdNeedsUpdate)
    {
        updateLCD();
        systemFlags.lcdNeedsUpdate = 0;
    }

    /*
    if (systemFlags.tmNeedsUpdate) {
        updateTM1638();
        systemFlags.tmNeedsUpdate = 0;
    }
    */
}

void updateLCD()
{
    char dispTop[17];
    char dispBot[17];

    // --- Linha 1 (Topo): [F] Cmd: ou Cmd: + últimos N chars do inputBuffer ---
    const char *prefix = systemFlags.isFastActMode ? "[F] Cmd:" : "Cmd:";
    uint8_t prefixLen = systemFlags.isFastActMode ? 8 : 4;
    uint8_t maxInLen = 16 - prefixLen;

    strncpy(dispTop, prefix, 16);
    dispTop[prefixLen] = '\0';

    if (inputLen <= maxInLen)
    {
        strncat(dispTop, inputBuffer, maxInLen);
        uint8_t total = strlen(dispTop);
        while (total < 16)
            dispTop[total++] = ' ';
        dispTop[16] = '\0';
    }
    else
    {
        // Mostra apenas os últimos N caracteres (janela deslizante)
        strncat(dispTop, inputBuffer + (inputLen - maxInLen), maxInLen);
        dispTop[16] = '\0';
    }

    // --- Linha 2 (Rodapé): Marquee ou texto fixo ---
    uint8_t msgLen = strlen(currentMsg);
    if (msgLen <= 16)
    {
        strncpy(dispBot, currentMsg, 16);
        uint8_t total = msgLen;
        while (total < 16)
            dispBot[total++] = ' ';
        dispBot[16] = '\0';
    }
    else
    {
        // [OPT-1/OPT-2] Scroll sem alocação dinâmica, índice uint16_t
        scrollTextToBuffer(dispBot, currentMsg, msgLen, scrollIndexBottom);
    }

    lcd.setCursor(0, 0);
    lcd.print(dispTop);
    lcd.setCursor(0, 1);
    lcd.print(dispBot);
}

// Gera janela de 16 chars com scroll circular — sem String, sem heap
void scrollTextToBuffer(char *out, const char *text, uint8_t textLen, uint16_t idx)
{
    const uint8_t GAP = 4; // Espaços de separação entre loops do texto
    uint8_t totalLen = textLen + GAP;
    uint16_t start = idx % totalLen;

    for (uint8_t i = 0; i < 16; i++)
    {
        uint16_t pos = (start + i) % totalLen;
        out[i] = (pos < textLen) ? text[pos] : ' ';
    }
    out[16] = '\0';
}

void updateTM1638()
{
    char tmStr[MAX_INPUT_LEN + 1];
    uint8_t tmLen = 0;
    uint8_t rawChars = 0;

    for (uint8_t i = 0; i < inputLen && tmLen < MAX_INPUT_LEN; i++)
    {
        char c = inputBuffer[i];
        if (c == ':' || c == ',')
        {
            tmStr[tmLen++] = '.';
        }
        else
        {
            tmStr[tmLen++] = c;
            rawChars++;
        }
    }
    while (rawChars < 8 && tmLen < MAX_INPUT_LEN)
    {
        tmStr[tmLen++] = ' ';
        rawChars++;
    }
    tmStr[tmLen] = '\0';
    tm.displayText(tmStr);
}

// =================================================================================
// 9. PERSISTÊNCIA (EEPROM) [OPT-6: Numeração corrigida de "10" para "9"]
// =================================================================================

// Helper: Define mensagem de status temporária (expira por timeout)
void setStatusMsg(const char *msg)
{
    strncpy(currentMsg, msg, MSG_MAX_LEN);
    currentMsg[MSG_MAX_LEN] = '\0';
    systemFlags.isMessageLong = (strlen(currentMsg) > 16) ? 1 : 0;
    systemFlags.msgIsStatus = 1; // [OPT-3] Marcada como temporária
    scrollIndexBottom = 0;
    lastMsgTime = millis();
    systemFlags.lcdNeedsUpdate = 1;
}

void saveSlot(uint8_t slot)
{
    if (slot >= NUM_SLOTS)
        return;
    int addr = EEPROM_START_ADDR + (slot * SLOT_SIZE);

    // [Problema #3] Limpa todo o bloco antes de gravar (evita bytes residuais)
    for (uint8_t i = 0; i < SLOT_SIZE; i++)
        EEPROM.update(addr + i, '\0');

    uint8_t len = min(inputLen, (uint8_t)MAX_INPUT_LEN);
    for (uint8_t i = 0; i < len; i++)
        EEPROM.update(addr + i, inputBuffer[i]);
    // Null terminator já garantido pela limpeza acima

    Serial.print(F("EEPROM Write Slot "));
    Serial.print(slot);
    Serial.print(F(": "));
    Serial.println(inputBuffer);

    char msg[20];
    snprintf_P(msg, sizeof(msg), PSTR("Slot %d Salvo!"), slot);
    setStatusMsg(msg);
}

void runSlot(uint8_t slot)
{
    if (slot >= NUM_SLOTS)
        return;
    int addr = EEPROM_START_ADDR + (slot * SLOT_SIZE);

    // [OPT-1] Leitura tipo-segura via EEPROM.get()
    char buffer[SLOT_SIZE];
    EEPROM.get(addr, buffer);
    buffer[SLOT_SIZE - 1] = '\0'; // Segurança absoluta

    // [OPT-4] Detecta EEPROM virgem (0xFF) ou slot vazio ('\0')
    if (buffer[0] == '\0' || (uint8_t)buffer[0] == 0xFF)
    {
        char msg[20];
        snprintf_P(msg, sizeof(msg), PSTR("Slot %d Vazio!"), slot);
        setStatusMsg(msg);
        return;
    }

    mainSerial.println(buffer);
    Serial.print(F("FastAct Slot "));
    Serial.print(slot);
    Serial.print(F(": "));
    Serial.println(buffer);

    // Feedback: ">> [comando]" (truncado para caber no LCD com scroll)
    char msg[MSG_MAX_LEN + 1];
    snprintf_P(msg, sizeof(msg), PSTR(">> %s"), buffer);
    setStatusMsg(msg);
}

// [OPT-7] Diagnóstico de todos os slots na inicialização
void diagnosticEEPROM()
{
    Serial.println(F("--- EEPROM Slots ---"));
    for (uint8_t s = 0; s < NUM_SLOTS; s++)
    {
        int addr = EEPROM_START_ADDR + (s * SLOT_SIZE);
        char buffer[SLOT_SIZE];
        EEPROM.get(addr, buffer);
        buffer[SLOT_SIZE - 1] = '\0';

        Serial.print(F("Slot "));
        Serial.print(s);
        Serial.print(F(": "));
        if (buffer[0] == '\0' || (uint8_t)buffer[0] == 0xFF)
            Serial.println(F("[Vazio]"));
        else
            Serial.println(buffer);
    }
    Serial.println(F("--------------------"));
}
