#include "M5Cardputer.h"
#include <HardwareSerial.h>

// =================================================================================
// CONFIGURAÇÕES GERAIS E HARDWARE
// =================================================================================
#define GROVE_RX 1
#define GROVE_TX 2
HardwareSerial groveSerial(1);

#define MAX_PRESETS 10
#define SERIAL_BUF_SIZE 128
#define TERM_MAX_LINES 6
#define TERM_LINE_LEN 40

// Cores (M5GFX usa RGB565)
#define COLOR_BG      TFT_BLACK
#define COLOR_CARD    M5.Display.color565(30, 30, 30)
#define COLOR_TEXT    TFT_WHITE
#define COLOR_ACCENT  M5.Display.color565(0, 150, 255)
#define COLOR_SUCCESS M5.Display.color565(0, 200, 50)
#define COLOR_ERROR   M5.Display.color565(255, 50, 50)
#define COLOR_WARNING M5.Display.color565(255, 200, 0)
#define COLOR_STOP    M5.Display.color565(255, 0, 0)

// Telas
enum AppScreen { SCREEN_SPLASH, SCREEN_GRID, SCREEN_TERMINAL };
AppScreen currentScreen = SCREEN_SPLASH;

// =================================================================================
// ESTRUTURAS DE DADOS
// =================================================================================
struct Preset {
    bool loaded;
    uint32_t step;
    uint32_t vel;
    uint8_t dir;
    uint16_t repeat;
    uint32_t pause_ms;
    uint8_t motor_id;
};
Preset presets[MAX_PRESETS];

// Buffers
char rxBuffer[SERIAL_BUF_SIZE];
uint8_t rxIndex = 0;
char statusBarMsg[64] = "Iniciando...";
uint16_t statusBarColor = COLOR_TEXT;

// Terminal
char termLines[TERM_MAX_LINES][TERM_LINE_LEN];
uint8_t termColors[TERM_MAX_LINES]; // 0=Normal, 1=TX, 2=RX OK, 3=RX Error
char termInput[TERM_LINE_LEN];
uint8_t termInputLen = 0;

// Controle de Inicialização (Fetch Presets)
bool fetchingPresets = false;
uint8_t fetchIndex = 0;
unsigned long lastFetchTime = 0;

bool needsRedraw = true;

// =================================================================================
// PROTÓTIPOS
// =================================================================================
void changeScreen(AppScreen newScreen);
void renderScreen();
void renderGrid();
void renderTerminal();
void renderTopBar();
void processSerialRX();
void parseH8PMessage(const char* msg);
void addTerminalLine(const char* line, uint8_t colorType);
void sendCommand(const char* cmd);

// =================================================================================
// SETUP
// =================================================================================
void setup() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);
    
    // Display
    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.fillScreen(COLOR_BG);
    
    // Serial
    Serial.begin(115200);
    groveSerial.begin(9600, SERIAL_8N1, GROVE_RX, GROVE_TX);

    for (int i = 0; i < MAX_PRESETS; i++) presets[i].loaded = false;
    for (int i = 0; i < TERM_MAX_LINES; i++) termLines[i][0] = '\0';
    termInput[0] = '\0';

    // Splash Screen
    M5Cardputer.Display.setTextSize(2);
    M5Cardputer.Display.setTextColor(COLOR_ACCENT);
    M5Cardputer.Display.drawCenterString("StepCommander", 120, 50, &fonts::Orbitron_Light_24);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(COLOR_TEXT);
    M5Cardputer.Display.drawCenterString("M5 Edition v1.0", 120, 80, 1);
    
    delay(1500);
    
    fetchingPresets = true;
    fetchIndex = 0;
    lastFetchTime = millis();
    changeScreen(SCREEN_GRID);
}

// =================================================================================
// LOOP PRINCIPAL
// =================================================================================
void loop() {
    M5Cardputer.update();
    processSerialRX();

    // Fetching presets on startup
    if (fetchingPresets && currentScreen == SCREEN_GRID) {
        if (millis() - lastFetchTime > 200) { // Timeout de 200ms por preset
            char cmd[10];
            snprintf(cmd, sizeof(cmd), "1A:%d", fetchIndex);
            groveSerial.println(cmd);
            Serial.printf("Fetching preset %d...\n", fetchIndex);
            
            fetchIndex++;
            lastFetchTime = millis();
            
            if (fetchIndex >= MAX_PRESETS) {
                fetchingPresets = false;
                strcpy(statusBarMsg, "Pronto.");
                statusBarColor = COLOR_SUCCESS;
                needsRedraw = true;
            }
        }
    }

    // Processamento do Teclado
    if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
        Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
        
        // STOP Global sempre disponível via tecla ESC ( ` na verdade, o cardputer usa a tecla ` (crase) como ESC no canto superior esquerdo)
        // Cardputer keyboard: The top-left key is ` / ~ which we can use as ESC/STOP.
        if (status.word.size() > 0 && status.word[0] == '`') {
            sendCommand("02"); // STOP
            strcpy(statusBarMsg, "STOP ENVIADO");
            statusBarColor = COLOR_STOP;
            needsRedraw = true;
        }
        else if (currentScreen == SCREEN_GRID) {
            if (status.word.size() > 0) {
                char c = status.word[0];
                if (c >= '0' && c <= '9') {
                    char cmd[10];
                    snprintf(cmd, sizeof(cmd), "18:%c", c);
                    sendCommand(cmd);
                } else if (c == 'r' || c == 'R') {
                    sendCommand("01"); // RUN
                } else if (c == 't' || c == 'T') {
                    changeScreen(SCREEN_TERMINAL);
                }
            }
        }
        else if (currentScreen == SCREEN_TERMINAL) {
            if (status.del) {
                if (termInputLen > 0) {
                    termInput[--termInputLen] = '\0';
                    needsRedraw = true;
                }
            } else if (status.enter) {
                if (termInputLen > 0) {
                    sendCommand(termInput);
                    addTerminalLine(termInput, 1); // 1 = TX color
                    termInput[0] = '\0';
                    termInputLen = 0;
                    needsRedraw = true;
                }
            } else if (status.word.size() > 0) {
                char c = status.word[0];
                // Voltar para Grid pressionando Tab no terminal (Tab não tem tecla dedicada, vamos usar '\t' ou um atalho Fn)
                // Usaremos CTRL+C (ou shift+C) não, vamos usar a tecla '\' para voltar ou algo.
                // Vou mapear a tecla '/' para voltar.
                if (c == '/') {
                    changeScreen(SCREEN_GRID);
                }
                else if (termInputLen < TERM_LINE_LEN - 1) {
                    termInput[termInputLen++] = c;
                    termInput[termInputLen] = '\0';
                    needsRedraw = true;
                }
            }
        }
    }

    if (needsRedraw) {
        renderScreen();
        needsRedraw = false;
    }
}

// =================================================================================
// FUNÇÕES AUXILIARES
// =================================================================================
void sendCommand(const char* cmd) {
    groveSerial.println(cmd);
    Serial.print("TX: ");
    Serial.println(cmd);
}

void changeScreen(AppScreen newScreen) {
    currentScreen = newScreen;
    M5Cardputer.Display.fillScreen(COLOR_BG);
    needsRedraw = true;
}

void addTerminalLine(const char* line, uint8_t colorType) {
    for (int i = 0; i < TERM_MAX_LINES - 1; i++) {
        strcpy(termLines[i], termLines[i+1]);
        termColors[i] = termColors[i+1];
    }
    strncpy(termLines[TERM_MAX_LINES - 1], line, TERM_LINE_LEN - 1);
    termLines[TERM_MAX_LINES - 1][TERM_LINE_LEN - 1] = '\0';
    termColors[TERM_MAX_LINES - 1] = colorType;
    needsRedraw = true;
}

// =================================================================================
// PROCESSAMENTO SERIAL (H8P)
// =================================================================================
void processSerialRX() {
    while (groveSerial.available() > 0) {
        char inChar = (char)groveSerial.read();
        
        if (inChar == '\n' || inChar == '\r') {
            if (rxIndex > 0) {
                rxBuffer[rxIndex] = '\0';
                parseH8PMessage(rxBuffer);
                rxIndex = 0;
            }
        } else if (rxIndex < (SERIAL_BUF_SIZE - 1)) {
            rxBuffer[rxIndex++] = inChar;
        }
    }
}

void parseH8PMessage(const char* msg) {
    Serial.printf("RX: %s\n", msg);
    
    char cleanMsg[SERIAL_BUF_SIZE];
    strncpy(cleanMsg, msg, sizeof(cleanMsg));
    
    uint8_t colorType = 2; // Default RX color (OK)
    statusBarColor = COLOR_TEXT;

    // Parsing BA:X (Leitura de Preset)
    if (strncmp(msg, "BA:", 3) == 0) {
        // Ex: BA:0,10:1600,11:400,12:1...
        int idx = msg[3] - '0';
        if (idx >= 0 && idx < MAX_PRESETS) {
            presets[idx].loaded = true;
            // Busca simplificada para fins de display
            const char* stepPtr = strstr(msg, "10:");
            if (stepPtr) presets[idx].step = atol(stepPtr + 3);
            const char* velPtr = strstr(msg, "11:");
            if (velPtr) presets[idx].vel = atol(velPtr + 3);
            
            needsRedraw = true;
        }
    }

    if (strncmp(msg, "A0", 2) == 0) { strcpy(statusBarMsg, "Sis Inicializado"); statusBarColor = COLOR_SUCCESS; }
    else if (strncmp(msg, "B0", 2) == 0) { strcpy(statusBarMsg, "Iniciando Fila"); statusBarColor = COLOR_WARNING; }
    else if (strncmp(msg, "B1", 2) == 0) { strcpy(statusBarMsg, "Motor Parado"); statusBarColor = COLOR_ACCENT; }
    else if (strncmp(msg, "B5", 2) == 0) { strcpy(statusBarMsg, "Fila Executada"); statusBarColor = COLOR_SUCCESS; }
    else if (msg[0] == 'E') { 
        // Erros
        colorType = 3;
        statusBarColor = COLOR_ERROR;
        if (strncmp(msg, "E0", 2) == 0) strcpy(statusBarMsg, "Err: Em Execucao");
        else if (strncmp(msg, "E1", 2) == 0) strcpy(statusBarMsg, "Err: Fila Vazia");
        else if (strncmp(msg, "E2", 2) == 0) strcpy(statusBarMsg, "Err: Fila Cheia");
        else strcpy(statusBarMsg, "Erro do Sistema");
    }
    else {
        strncpy(statusBarMsg, msg, sizeof(statusBarMsg));
    }

    if (currentScreen == SCREEN_TERMINAL) {
        addTerminalLine(msg, colorType);
    }
    needsRedraw = true;
}

// =================================================================================
// RENDERIZAÇÃO
// =================================================================================
void renderScreen() {
    if (currentScreen == SCREEN_GRID) {
        renderTopBar();
        renderGrid();
    } else if (currentScreen == SCREEN_TERMINAL) {
        renderTopBar();
        renderTerminal();
    }
}

void renderTopBar() {
    M5Cardputer.Display.fillRect(0, 0, 240, 16, COLOR_BG);
    M5Cardputer.Display.drawLine(0, 16, 240, 16, COLOR_CARD);
    
    // Botão STOP sempre visível como dica
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(COLOR_STOP);
    M5Cardputer.Display.drawString("[`] STOP", 2, 4);

    // Status message
    M5Cardputer.Display.setTextColor(statusBarColor);
    M5Cardputer.Display.drawRightString(statusBarMsg, 238, 4, 1);
}

void renderGrid() {
    // 240x135 display. Area starts at y=17
    // 4 columns, 3 rows for cards
    int cardW = 56;
    int cardH = 34;
    int padX = 3;
    int padY = 4;
    int startX = 4;
    int startY = 20;

    M5Cardputer.Display.setTextSize(1);

    for (int i = 0; i < 12; i++) {
        int row = i / 4;
        int col = i % 4;
        int x = startX + col * (cardW + padX);
        int y = startY + row * (cardH + padY);

        M5Cardputer.Display.fillRoundRect(x, y, cardW, cardH, 3, COLOR_CARD);

        if (i < 10) {
            // Preset Card (0-9)
            M5Cardputer.Display.setTextColor(COLOR_ACCENT);
            char numStr[4];
            snprintf(numStr, sizeof(numStr), "[%d]", i);
            M5Cardputer.Display.drawString(numStr, x + 4, y + 4);

            M5Cardputer.Display.setTextColor(COLOR_TEXT);
            if (presets[i].loaded) {
                char sStr[10];
                snprintf(sStr, sizeof(sStr), "S:%d", presets[i].step);
                M5Cardputer.Display.drawString(sStr, x + 4, y + 16);
            } else {
                M5Cardputer.Display.drawString("...", x + 4, y + 16);
            }
        } else if (i == 10) {
            // RUN
            M5Cardputer.Display.setTextColor(COLOR_SUCCESS);
            M5Cardputer.Display.drawCenterString("RUN", x + cardW/2, y + cardH/2 - 4, 1);
            M5Cardputer.Display.drawCenterString("(R)", x + cardW/2, y + cardH/2 + 6, 1);
        } else if (i == 11) {
            // TERMINAL
            M5Cardputer.Display.setTextColor(COLOR_TEXT);
            M5Cardputer.Display.drawCenterString(">_", x + cardW/2, y + cardH/2 - 4, 1);
            M5Cardputer.Display.drawCenterString("(T)", x + cardW/2, y + cardH/2 + 6, 1);
        }
    }
}

void renderTerminal() {
    // Area de mensagens (linhas 0 a TERM_MAX_LINES-1)
    int startY = 20;
    int lineH = 14;
    
    M5Cardputer.Display.fillRect(0, 17, 240, 135-17, COLOR_BG);
    M5Cardputer.Display.setTextSize(1);
    
    for (int i = 0; i < TERM_MAX_LINES; i++) {
        if (termLines[i][0] != '\0') {
            if (termColors[i] == 1) M5Cardputer.Display.setTextColor(COLOR_ACCENT); // TX
            else if (termColors[i] == 3) M5Cardputer.Display.setTextColor(COLOR_ERROR); // Erro RX
            else M5Cardputer.Display.setTextColor(COLOR_SUCCESS); // OK RX
            
            char prefix[3];
            if (termColors[i] == 1) strcpy(prefix, "> ");
            else strcpy(prefix, "< ");
            
            M5Cardputer.Display.drawString(prefix, 2, startY + i*lineH);
            M5Cardputer.Display.drawString(termLines[i], 16, startY + i*lineH);
        }
    }

    // Input line
    int inputY = startY + TERM_MAX_LINES * lineH + 4;
    M5Cardputer.Display.drawLine(0, inputY - 2, 240, inputY - 2, COLOR_CARD);
    
    M5Cardputer.Display.setTextColor(COLOR_TEXT);
    M5Cardputer.Display.drawString("Cmd:", 2, inputY);
    M5Cardputer.Display.drawString(termInput, 32, inputY);
    
    // Pisca cursor (simplificado)
    if ((millis() / 500) % 2 == 0) {
        int cursorX = 32 + M5Cardputer.Display.textWidth(termInput);
        M5Cardputer.Display.fillRect(cursorX, inputY, 6, 10, COLOR_TEXT);
    }
    
    // Dica para sair
    M5Cardputer.Display.setTextColor(COLOR_CARD);
    M5Cardputer.Display.drawRightString("[/] Sair", 238, inputY, 1);
}
