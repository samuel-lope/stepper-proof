/*
 * =================================================================================
 * Stepper Commander - Interface H8P (Low-Level Optimized)
 * =================================================================================
 *
 * Board: Arduino Uno / Nano (Atmega328P)
 * Função: Envio de comandos H8P e recebimento de alertas do controlador principal
 *
 * Arquitetura Atualizada:
 * - Leitura Serial Não-Bloqueante: A leitura de pacotes UART ocorre char-por-char,
 *   sem paralisar o loop principal (eliminação do readStringUntil).
 * - Multitarefa Cooperativa: Uso intensivo de millis() como timer base para
 *   processos concorrentes (LCD, Leitura RX, Leitura Matriz).
 * - Otimização de RAM: Uso do modificador F() para manter strings na Flash (PROGMEM)
 *   e uso de "bit fields" para as flags de controle de estado.
 *
 * Periféricos:
 * - Teclado Matricial 4x4 (Linhas 9,8,7,6 / Colunas 5,4,3,2)
 * - Display LCD 16x2 I2C (SDA=A4, SCL=A5, Endereço: 0x27)
 * - SoftwareSerial (RX=10, TX=11) -> Conecta ao TX/RX da placa Principal
 * - Opcional: TM1638plus no barramento SPI emulado por software (A0, A1, A2)
 *
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
#define SCROLL_INTERVAL_MS 400

// Configurações da EEPROM (Slots de Macro)
#define EEPROM_START_ADDR 0
#define SLOT_SIZE 33 // 32 chars + null terminator
#define NUM_SLOTS 10

// Display LCD I2C (Endereço 0x27, 16 Colunas, 2 Linhas)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Comunicação Serial H8P
SoftwareSerial mainSerial(10, 11);

// Modulo TM1638
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

// Uso de struct com bitfields para economizar RAM ao empacotar flags lógicas
struct
{
    uint8_t lcdNeedsUpdate : 1;
    uint8_t tmNeedsUpdate : 1;
    uint8_t isMessageLong : 1;
    uint8_t isFastActMode : 1;
} systemFlags;

// Buffers de Interface (Alocação dinâmica evitada ao máximo onde for custoso)
String inputBuffer = "";
String lastCommand = "";
String currentMsg = "Pronto.";

// Controle do Marquee (Animação de Scroll cooperativo via millis)
unsigned long lastScrollTime = 0;
uint8_t scrollIndexBottom = 0;

// Buffer Estático C-String para recepção Serial (NÃO bloqueante)
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
String getScrollText(const String &text, uint8_t index);

// =================================================================================
// 4. SETUP INICIAL
// =================================================================================
void setup()
{
    Serial.begin(9600);
    mainSerial.begin(9600);

    // Inicialização do Hardware
    lcd.init();
    lcd.backlight();

    // tm.displayBegin(); // (Opcional - Comentado para teste de teclado)
    // tm.reset();        // (Opcional - Comentado para teste de teclado)

    // Estado Inicial
    systemFlags.lcdNeedsUpdate = 1;
    systemFlags.tmNeedsUpdate = 0;
    systemFlags.isMessageLong = 0;
    systemFlags.isFastActMode = 0;

    Serial.println(F("Commander Iniciado. [Non-Blocking Architecture]"));
}

// =================================================================================
// 5. LOOP PRINCIPAL (COOPERATIVE MULTITASKING)
// =================================================================================
void loop()
{
    // 1. Verificar pacotes na Serial UART (Event-driven lógico)
    processSerialRX();

    // 2. Escaneamento Matricial do Teclado
    processKeypad();

    // 3. Renderização Dinâmica (Processado apenas via Timer ou mudança de flag)
    handleDisplayTasks();
}

// =================================================================================
// 6. PROCESSAMENTO SERIAL NÃO-BLOQUEANTE
// =================================================================================
void processSerialRX()
{
    // Escoa todo o buffer de hardware sem prender a thread
    while (mainSerial.available() > 0)
    {
        char inChar = (char)mainSerial.read();

        // Quebra de linha define final do pacote H8P
        if (inChar == '\n' || inChar == '\r')
        {
            if (rxIndex > 0)
            {
                rxBuffer[rxIndex] = '\0'; // Finaliza C-string clássica
                parseH8PMessage(rxBuffer);
                rxIndex = 0; // Limpa ponteiro para o próximo pacote
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
    Serial.print(F("RX (Cru): "));
    Serial.println(msg);

    // Dicionário H8P em PROGMEM para economizar RAM usando strncmp em vez de String
    if (strncmp(msg, "A0", 2) == 0)
        currentMsg = F("Sis Inicializado");
    else if (strncmp(msg, "B0", 2) == 0)
        currentMsg = F("Iniciando Fila");
    else if (strncmp(msg, "B1", 2) == 0)
        currentMsg = F("Motor Parado");
    else if (strncmp(msg, "B2", 2) == 0)
        currentMsg = F("Pausa Global");
    else if (strncmp(msg, "B4", 2) == 0)
        currentMsg = F("Repeat ON");
    else if (strncmp(msg, "B5", 2) == 0)
        currentMsg = F("Fila Executada");
    else if (strncmp(msg, "B6", 2) == 0)
        currentMsg = F("Repeat OFF");
    else if (strncmp(msg, "B7", 2) == 0)
        currentMsg = F("Motor Habilitado");
    else if (strncmp(msg, "B8", 2) == 0)
        currentMsg = F("Motor Desabiltd.");
    else if (strncmp(msg, "B9", 2) == 0 || strncmp(msg, "BA", 2) == 0)
        currentMsg = F("Preset EEPROM");
    else if (strncmp(msg, "E0", 2) == 0)
        currentMsg = F("Err: Em Execucao");
    else if (strncmp(msg, "E1", 2) == 0)
        currentMsg = F("Err: Fila Vazia");
    else if (strncmp(msg, "E2", 2) == 0)
        currentMsg = F("Err: Fila Cheia");
    else if (strncmp(msg, "E3", 2) == 0)
        currentMsg = F("Erro de Sintaxe");
    else if (strncmp(msg, "E4", 2) == 0)
        currentMsg = F("Preset Invalido");
    else if (strncmp(msg, "BB", 2) == 0 || strncmp(msg, "BC", 2) == 0)
        currentMsg = F("FastAct. Exec");
    else if (strncmp(msg, "C0", 2) == 0)
    {
        currentMsg = F("Salvo: Slot ");
        currentMsg += msg[3]; // Extrai o dígito referenciado no protocolo C0_X
    }
    else
    {
        // Fallback genérico caso o pacote não exista na tabela fixa
        currentMsg = String(msg);
    }

    Serial.print(F("-> Tratado: "));
    Serial.println(currentMsg);

    // Sinaliza o renderizador de que a interface deve ser atualizada
    systemFlags.isMessageLong = (currentMsg.length() > 16) ? 1 : 0;
    scrollIndexBottom = 0;
    systemFlags.lcdNeedsUpdate = 1;
}

// =================================================================================
// 7. LÓGICA DE INTERFACE: TECLADO (EVENTOS)
// =================================================================================
void processKeypad()
{
    char key = keypad.getKey();
    if (!key)
        return; // Se nenhum botão foi pressionado, retorna ao Loop principal

    Serial.print(F("Tecla: "));
    Serial.println(key);

    // No modo Fast Act, teclas numéricas disparam macros imediatamente
    if (systemFlags.isFastActMode && isDigit(key))
    {
        runSlot(key - '0');
        return;
    }

    // O '*' funciona como prefixo de comandos ou caracter separador (':')
    if (inputBuffer.endsWith(":"))
    {
        if (key == '#')
        {
            // [* + #] = ENTER (Enviar Comando H8P)
            inputBuffer.remove(inputBuffer.length() - 1);
            if (inputBuffer.length() == 0 && lastCommand.length() > 0)
                inputBuffer = lastCommand;
            else if (inputBuffer.length() > 0)
                lastCommand = inputBuffer;

            if (inputBuffer.length() > 0)
            {
                mainSerial.println(inputBuffer);
                Serial.print(F("TX: "));
                Serial.println(inputBuffer);
            }
            inputBuffer = "";
        }
        else if (key == 'B')
        { // [* + B] = Toggle Fast Act Mode
            inputBuffer.remove(inputBuffer.length() - 1);
            systemFlags.isFastActMode = !systemFlags.isFastActMode;
            currentMsg = systemFlags.isFastActMode ? F("FAST ACT ON") : F("FAST ACT OFF");
            scrollIndexBottom = 0;
            systemFlags.isMessageLong = (currentMsg.length() > 16) ? 1 : 0;
        }
        else if (isDigit(key))
        { // [* + 0-9] = Save to EEPROM Slot
            inputBuffer.remove(inputBuffer.length() - 1);
            if (inputBuffer.length() > 0)
            {
                saveSlot(key - '0');
                inputBuffer = "";
            }
        }
        else if (key == 'C')
        { // [* + C] = CLEAR ALL
            inputBuffer = "";
        }
        else if (key == 'D')
        { // [* + D] = BACKSPACE
            if (inputBuffer.length() >= 2)
                inputBuffer.remove(inputBuffer.length() - 2);
            else
                inputBuffer = "";
        }
        else if (key == 'A')
        { // [* + A] = Inserir SINAL DE MENOS (-)
            inputBuffer.setCharAt(inputBuffer.length() - 1, '-');
        }
        else if (key == '*')
        {
            if (inputBuffer.length() < MAX_INPUT_LEN)
                inputBuffer += ":";
        }
        else
        {
            if (inputBuffer.length() < MAX_INPUT_LEN)
                inputBuffer += key;
        }
    }
    else
    {
        // Pressionamento convencional (Sem prefixo ativo)
        if (key == '#')
        {
            if (inputBuffer.length() < MAX_INPUT_LEN)
                inputBuffer += ",";
        }
        else if (key == '*')
        {
            if (inputBuffer.length() < MAX_INPUT_LEN)
                inputBuffer += ":";
        }
        else
        {
            if (inputBuffer.length() < MAX_INPUT_LEN)
                inputBuffer += key;
        }
    }

    // Sinaliza à camada gráfica que houve mutação nos buffers
    systemFlags.lcdNeedsUpdate = 1;
}

// =================================================================================
// 8. GERENCIADOR DE DISPLAYS (RENDERIZADOR COOPERATIVO)
// =================================================================================
void handleDisplayTasks()
{
    unsigned long currentMillis = millis();

    // Lógica do Marquee (Scroll do texto que ultrapassa 16 caracteres no LCD)
    if (systemFlags.isMessageLong)
    {
        if (currentMillis - lastScrollTime >= SCROLL_INTERVAL_MS)
        {
            lastScrollTime = currentMillis;
            scrollIndexBottom++;
            systemFlags.lcdNeedsUpdate = 1;
        }
    }

    // Atualiza Fisicamente o I2C apenas sob demanda
    if (systemFlags.lcdNeedsUpdate)
    {
        updateLCD();
        systemFlags.lcdNeedsUpdate = 0;
    }

    /*
    // Chamada do display de 7 segmentos (Ative caso deseje espelhar os valores)
    if (systemFlags.tmNeedsUpdate) {
        updateTM1638();
        systemFlags.tmNeedsUpdate = 0;
    }
    */
}

void updateLCD()
{
    // Linha 1 (Topo): Cmd: [Últimos 12 char do Buffer]
    String dispTop = systemFlags.isFastActMode ? F("[F] Cmd:") : F("Cmd:");
    uint8_t prefixLen = systemFlags.isFastActMode ? 8 : 4;
    uint8_t maxInLen = 16 - prefixLen;

    if (inputBuffer.length() <= maxInLen)
    {
        dispTop += inputBuffer;
        while (dispTop.length() < 16)
            dispTop += " ";
    }
    else
    {
        dispTop += inputBuffer.substring(inputBuffer.length() - maxInLen);
    }

    // Linha 2 (Rodapé): Alertas Dinâmicos (Com ou sem Marquee/Scroll)
    String dispBot = getScrollText(currentMsg, scrollIndexBottom);

    lcd.setCursor(0, 0);
    lcd.print(dispTop);
    lcd.setCursor(0, 1);
    lcd.print(dispBot);
}

String getScrollText(const String &text, uint8_t index)
{
    if (text.length() <= 16)
    {
        String padded = text;
        while (padded.length() < 16)
            padded += " ";
        return padded;
    }

    // Cria um espaçamento visual de 4 casas para separar o loop da mensagem
    String padded = text + F("    ");
    uint8_t len = padded.length();
    uint8_t idx = index % len;

    String result = padded.substring(idx) + padded.substring(0, idx);
    return result.substring(0, 16);
}

void updateTM1638()
{
    // Transforma ":" e "," em pontuação de display (.) para economizar dígitos no TM
    String tmString = "";
    for (unsigned int i = 0; i < inputBuffer.length(); i++)
    {
        char c = inputBuffer.charAt(i);
        if (c == ':' || c == ',')
            tmString += ".";
        else
            tmString += c;
    }

    // Conta quantos caracteres 'reais' (não pontuações) ocupam um segmento inteiro
    String paddedStr = tmString;
    uint8_t rawChars = 0;
    for (unsigned int i = 0; i < tmString.length(); i++)
    {
        if (tmString.charAt(i) != '.')
            rawChars++;
    }

    // Padding com espaços vazios à direita
    while (rawChars < 8)
    {
        paddedStr += " ";
        rawChars++;
    }

    tm.displayText(paddedStr.c_str());
}

// =================================================================================
// 10. PERSISTÊNCIA (EEPROM)
// =================================================================================
void saveSlot(uint8_t slot)
{
    if (slot >= NUM_SLOTS)
        return;
    int addr = EEPROM_START_ADDR + (slot * SLOT_SIZE);

    // Grava o buffer caractere por caractere
    uint8_t len = inputBuffer.length();
    if (len > MAX_INPUT_LEN)
        len = MAX_INPUT_LEN;

    for (uint8_t i = 0; i < len; i++)
    {
        EEPROM.update(addr + i, inputBuffer[i]);
    }
    EEPROM.update(addr + len, '\0'); // Null terminator

    currentMsg = F("Slot ");
    currentMsg += slot;
    currentMsg += F(" Salvo!");

    systemFlags.isMessageLong = 0;
    scrollIndexBottom = 0;
    systemFlags.lcdNeedsUpdate = 1;

    Serial.print(F("EEPROM Write Slot "));
    Serial.print(slot);
    Serial.print(F(": "));
    Serial.println(inputBuffer);
}

void runSlot(uint8_t slot)
{
    if (slot >= NUM_SLOTS)
        return;
    int addr = EEPROM_START_ADDR + (slot * SLOT_SIZE);

    char buffer[SLOT_SIZE];
    for (uint8_t i = 0; i < SLOT_SIZE; i++)
    {
        buffer[i] = EEPROM.read(addr + i);
        if (buffer[i] == '\0')
            break;
    }
    buffer[SLOT_SIZE - 1] = '\0'; // Segurança

    if (strlen(buffer) > 0)
    {
        mainSerial.println(buffer);
        currentMsg = F("FastAct: ");
        currentMsg += buffer;

        Serial.print(F("FastAct Run Slot "));
        Serial.print(slot);
        Serial.print(F(": "));
        Serial.println(buffer);
    }
    else
    {
        currentMsg = F("Slot ");
        currentMsg += slot;
        currentMsg += F(" Vazio!");
    }

    systemFlags.isMessageLong = (currentMsg.length() > 16) ? 1 : 0;
    scrollIndexBottom = 0;
    systemFlags.lcdNeedsUpdate = 1;
}
