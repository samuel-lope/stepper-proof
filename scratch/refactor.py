import re

with open('stepcontrol-v2/stepcontrol-v2.ino', 'r') as f:
    content = f.read()

# 1. Replace UI variables
old_ui = """// ==========================================
// VARIÁVEIS DA INTERFACE (UI)
// ==========================================
LiquidCrystal_I2C lcd(0x27, 16, 2);
String inputBuffer = "";
String lastCommand = "";
const int MAX_INPUT_LEN = 32;
String currentMsg = "Sis Inicializado";
unsigned long lastScrollTime = 0;
int scrollIndexBottom = 0;"""

new_ui = """// ==========================================
// VARIÁVEIS DA INTERFACE (UI)
// ==========================================
#define MAX_INPUT_LEN 64
#define MSG_MAX_LEN 64
#define SCROLL_INTERVAL_BASE 500
#define MSG_TIMEOUT_MS 8000

LiquidCrystal_I2C lcd(0x27, 16, 2);

struct {
    uint8_t lcdNeedsUpdate : 1;
    uint8_t isMessageLong : 1;
    uint8_t isFastActMode : 1;
    uint8_t msgIsStatus : 1;
    uint8_t menuActive : 1;
    uint8_t menuSelection : 1;
} systemFlags;

char inputBuffer[MAX_INPUT_LEN + 1];
uint8_t inputLen = 0;
char lastCommand[MAX_INPUT_LEN + 1];
char currentMsg[MSG_MAX_LEN + 1];

unsigned long lastScrollTime = 0;
uint16_t scrollIndexBottom = 0;
unsigned long lastMsgTime = 0;
uint8_t saveComboState = 0;"""

content = content.replace(old_ui, new_ui)

# 2. Prototypes
old_proto = """// PROTÓTIPOS
// ==========================================
void updateLCD();
void setUIMessage(String msg);
void interpretarComando(char *linha);"""

new_proto = """// PROTÓTIPOS
// ==========================================
void updateLCD();
void handleDisplayTasks();
void setUIMessage(const char *msg);
void scrollTextToBuffer(char *out, const char *text, uint8_t textLen, uint16_t idx);
void interpretarComando(char *linha);
void diagnosticoEEPROM();
void processMenuKey(char key);
void enterMenu();
ComandoMotor parseLineToMotorCommand(char *linha);"""

content = content.replace(old_proto, new_proto)

# 3. Setup
old_setup = """    inicializarPresetsEEPROM();

    setUIMessage("A0: Inicializado");
}"""

new_setup = """    inicializarPresetsEEPROM();

    inputBuffer[0] = '\\0';
    lastCommand[0] = '\\0';
    strcpy_P(currentMsg, PSTR("Pronto."));

    systemFlags.lcdNeedsUpdate = 1;
    systemFlags.isMessageLong = 0;
    systemFlags.isFastActMode = 0;
    systemFlags.msgIsStatus = 0;
    systemFlags.menuActive = 0;
    systemFlags.menuSelection = 0;

    diagnosticoEEPROM();

    setUIMessage("A0: Inicializado");
}"""

content = content.replace(old_setup, new_setup)

with open('stepcontrol-v2/stepcontrol-v2.ino', 'w') as f:
    f.write(content)

print("UI, Prototypes and Setup updated.")
