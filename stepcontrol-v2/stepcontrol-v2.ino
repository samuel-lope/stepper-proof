/*
 * =================================================================================
 * Stepper Commander - Controle Integrado Mega 2560 (Monolítico)
 * =================================================================================
 * Placa: Arduino Mega 2560
 * Função: Interface H8P, Parser de Comandos e Controle de Motores Dual Timer
 * * MAPEAMENTO DE HARDWARE (BAIXO NÍVEL):
 * - Motor 1: PORTA (PA0=22 DIR, PA2=24 PUL, PA4=26 EN) -> Usando Timer 1
 * - Motor 2: PORTC (PC7=30 DIR, PC5=32 PUL, PC3=34 EN) -> Usando Timer 3
 * - Teclado Matricial: PORTK -> Interrupção PCINT2
 *   * Fiação Física Invertida: Linhas (A11, A10, A9, A8) | Colunas (A15, A14, A13, A12)
 * - Display LCD 16x2 I2C: Pinos de Hardware SDA(20) e SCL(21)
 * * =================================================================================
 * ÍNDICE DE CÓDIGOS HEXADECIMAIS (8-bits) - PROTOCOLO INTERNO H8P
 * =================================================================================
 * --- COMANDOS E PARÂMETROS (INPUT DO TECLADO / SERIAL) ---
 * 01 : run          (Inicia a execução da fila)
 * 02 : stop         (Parada de emergência e limpa fila)
 * 03 : repeatAll    (Booleano: 03:1 = ativa, 03:0 = desativa loop infinito)
 * 04 : pause        (Pausa global. Ex: 04:1000)
 * 10 : step         (Obrigatório - Quantidade de passos)
 * 11 : vel          (Obrigatório - Intervalo em microssegundos)
 * 12 : dir          (Opcional - Direção: 0 ou 1)
 * 13 : repeat       (Opcional - Repetições da linha. 0 = infinito)
 * 14 : pause        (Opcional - Pausa em ms após a linha)
 * 15 : motor        (Opcional - Motor Alvo 1 ou 2. Se vazio, motor 1)
 * 16 : enableMotor  (Habilita driver do motor. Ex: 16:1 ou 16:2 — EN LOW)
 * 17 : disableMotor (Desabilita driver do motor. Ex: 17:1 ou 17:2 — EN HIGH)
 * 18 : fastAction   (Executa preset EEPROM imediatamente. Ex: 18:0 a 18:9)
 * 19 : writePreset  (Grava preset na EEPROM. Ex: 19:2,10:800,11:300...)
 * 1A : readPreset   (Lê preset da EEPROM. Ex: 1A:2)
 * 1B : fastActionRep(Executa preset EEPROM com loop customizado. Ex: 1B:0:-4)
 * 1C : saveToEEPROM (Salva linha da SRAM na EEPROM. Ex: 1C:3:2)
 * * --- PRINCIPAIS ALERTAS E MENSAGENS NA UI (LCD / SERIAL) ---
 * A0 : Inicializado
 * B0 : Executando / Retomando...
 * B1 : Motor Parado
 * B2 : Pausa [X]ms
 * B3 : Pausa Global
 * B4 : Rep. ON
 * B5 : Fila Executada
 * B6 : Rep. OFF
 * B7 : M[X] Habilitado
 * B8 : M[X] Desabilt.
 * B9 : Preset [X] Gravado
 * BA : L[X] S:[X] V:[X] (Leitura de Preset)
 * BB : FastAct [idx]
 * BC : FastActRep [idx]
 * C0 : Slot [X] Salvo
 * C1 : Slot [X] (Fila SRAM)
 * D0 : M[X] L[X] (Debug Execução)
 * E0 : Err: Em Execucao
 * E1 : Err: Fila Vazia / Slot Invalido
 * E2 : Err: Fila Cheia
 * E3 : Err: Erro Sintaxe
 * E4 : Err: Preset Inv.
 * =================================================================================
 * MAPEAMENTO DE FUNÇÕES DO COMMANDER (TECLADO / UI)
 * =================================================================================
 * [IMPLEMENTADO] * + #          -> Enter (Enviar Comando da Linha)
 * [IMPLEMENTADO] * + C          -> Apagar Tudo (Clear Buffer)
 * [IMPLEMENTADO] * + D          -> Apagar Último (Backspace)
 * [IMPLEMENTADO] * + A          -> Inserir Sinal de Menos (-)
 * [IMPLEMENTADO] * + B          -> Alternar Modo Fast Act (Execução Direta de Macros)
 * [IMPLEMENTADO] # + A + N      -> Gravar Comando Atual na EEPROM (Slot N)
 * [IMPLEMENTADO] * + 000        -> Abrir Menu Interativo (SRAM Queue)
 * [IMPLEMENTADO] Teclas no Menu -> A/B (Navegar), # (Confirmar), * (Cancelar)
 * [IMPLEMENTADO] Num Fast Act   -> Teclas 0-9 executam slots EEPROM imediatamente
 * [IMPLEMENTADO] UI Multitarefa -> Scroll Adaptativo de textos e Timeout de 8s para Alertas
 * =================================================================================
 */

#include <Arduino.h>
#include <EEPROM.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/delay.h>

// ==========================================
// DEFINIÇÕES DE PINOS DOS MOTORES
// ==========================================
// Motor 1 (PORTA)
#define M1_DIR_PIN PA0 // Pino 22
#define M1_PUL_PIN PA2 // Pino 24
#define M1_EN_PIN  PA4 // Pino 26

// Motor 2 (PORTC)
#define M2_DIR_PIN PC7 // Pino 30
#define M2_PUL_PIN PC5 // Pino 32
#define M2_EN_PIN  PC3 // Pino 34

// ==========================================
// ESTRUTURAS E EEPROM
// ==========================================
#define MAX_FILA 20
#define EEPROM_MAGIC_BYTE 0xA5
#define EEPROM_MAGIC_ADDR 0
#define EEPROM_PRESETS_ADDR 1
#define MAX_PRESETS 10

typedef struct
{
    uint32_t step;
    uint32_t vel;
    uint8_t dir;
    uint16_t repeat;
    uint32_t pause_ms;
    uint8_t motor_id;
} ComandoMotor;

ComandoMotor fila_comandos[MAX_FILA];
uint8_t qtd_comandos_na_fila = 0;

// ==========================================
// ESTADO GLOBAL DO SISTEMA
// ==========================================
bool repetir_todas_linhas = false;
bool fila_iniciada = false;
uint32_t global_pause_ms = 0;
bool is_global_paused = false;
uint32_t tempo_inicio_global_pause = 0;

// --- Estado Motor 1 ---
volatile uint32_t m1_passos_restantes = 0;
volatile uint8_t m1_em_movimento = 0;
volatile uint32_t m1_delay_ticks = 0;
volatile uint32_t m1_interval_ticks = 0;
bool m1_executando = false;
uint8_t m1_indice_atual = 0;
uint16_t m1_repeticoes_restantes = 0;
bool m1_em_pausa = false;
uint32_t m1_tempo_inicio_pausa = 0;
bool m1_comando_infinito = false;

// --- Estado Motor 2 ---
volatile uint32_t m2_passos_restantes = 0;
volatile uint8_t m2_em_movimento = 0;
volatile uint32_t m2_delay_ticks = 0;
volatile uint32_t m2_interval_ticks = 0;
bool m2_executando = false;
uint8_t m2_indice_atual = 0;
uint16_t m2_repeticoes_restantes = 0;
bool m2_em_pausa = false;
uint32_t m2_tempo_inicio_pausa = 0;
bool m2_comando_infinito = false;

// ==========================================
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
uint8_t saveComboState = 0;

// Variáveis do Teclado (PCINT)
volatile bool keyPressFlag = false;
char matrixKeys[4][4] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}};

// ==========================================
// PROTÓTIPOS
// ==========================================
void updateLCD();
void handleDisplayTasks();
void setUIMessage(const char *msg);
void scrollTextToBuffer(char *out, const char *text, uint8_t textLen, uint16_t idx);
void interpretarComando(char *linha);
void diagnosticoEEPROM();
void processMenuKey(char key);
void enterMenu();
ComandoMotor parseLineToMotorCommand(char *linha);
bool carregarProximoComando(uint8_t motor);
void moverMotor1(uint32_t passos, uint32_t intervalo_us, uint8_t direcao);
void moverMotor2(uint32_t passos, uint32_t intervalo_us, uint8_t direcao);
void inicializarPresetsEEPROM();
void gravarPresetEEPROM(uint8_t idx, ComandoMotor cmd);
ComandoMotor lerPresetEEPROM(uint8_t idx);
void executarFastAction(uint8_t idx);
void executarFastActionRepeat(uint8_t idx, int32_t custom_repeat_signed);

// ==========================================
// SETUP
// ==========================================
void setup()
{
    Serial.begin(9600);

    // 1. Configura LCD
    lcd.init();
    lcd.backlight();
    updateLCD();

    // 2. Configura Pinos dos Motores (PORTA e PORTC) como SAÍDA e desliga (LOW - Enable ativo baixo normalmente, mas começa HIGH desabilitado)
    DDRA |= (1 << M1_PUL_PIN) | (1 << M1_DIR_PIN) | (1 << M1_EN_PIN);
    DDRC |= (1 << M2_PUL_PIN) | (1 << M2_DIR_PIN) | (1 << M2_EN_PIN);
    
    // Ativa pull-up no ENABLE (Motor Desabilitado inicialmente)
    PORTA |= (1 << M1_EN_PIN);
    PORTC |= (1 << M2_EN_PIN);
    
    // Garante que PUL e DIR iniciam em LOW
    PORTA &= ~((1 << M1_PUL_PIN) | (1 << M1_DIR_PIN));
    PORTC &= ~((1 << M2_PUL_PIN) | (1 << M2_DIR_PIN));

    cli();
    // 3. Configura Timer 1 (Motor 1) - Prescaler 8
    TCCR1A = 0;
    TCCR1B = (1 << CS11);
    TCNT1 = 0;

    // 4. Configura Timer 3 (Motor 2) - Prescaler 8
    TCCR3A = 0;
    TCCR3B = (1 << CS31);
    TCNT3 = 0;

    // 5. Configura Teclado no PORTK com Interrupção (PCINT2)
    DDRK = 0x0F;
    PORTK = 0xF0;

    PCICR |= (1 << PCIE2);
    PCMSK2 |= 0xF0;
    sei();

    inicializarPresetsEEPROM();

    inputBuffer[0] = '\0';
    lastCommand[0] = '\0';
    strcpy_P(currentMsg, PSTR("Pronto."));

    systemFlags.lcdNeedsUpdate = 1;
    systemFlags.isMessageLong = 0;
    systemFlags.isFastActMode = 0;
    systemFlags.msgIsStatus = 0;
    systemFlags.menuActive = 0;
    systemFlags.menuSelection = 0;

    diagnosticoEEPROM();

    setUIMessage("A0: Inicializado");
}

// ==========================================
// ROTINAS DE INTERRUPÇÃO (ISRs)
// ==========================================
ISR(PCINT2_vect)
{
    keyPressFlag = true;
}

ISR(TIMER1_COMPA_vect)
{
    if (m1_delay_ticks > 0)
    {
        if (m1_delay_ticks > 65535)
        {
            OCR1A += 65535;
            m1_delay_ticks -= 65535;
        }
        else
        {
            OCR1A += m1_delay_ticks;
            m1_delay_ticks = 0;
        }
    }
    else
    {
        if (m1_passos_restantes > 0)
        {
            PORTA |= (1 << M1_PUL_PIN);
            _delay_us(3);
            PORTA &= ~(1 << M1_PUL_PIN);
            m1_passos_restantes--;

            if (m1_passos_restantes > 0)
            {
                m1_delay_ticks = m1_interval_ticks;
                if (m1_delay_ticks > 65535)
                {
                    OCR1A += 65535;
                    m1_delay_ticks -= 65535;
                }
                else
                {
                    OCR1A += m1_delay_ticks;
                    m1_delay_ticks = 0;
                }
            }
            else
            {
                TIMSK1 &= ~(1 << OCIE1A);
                m1_em_movimento = 0;
            }
        }
        else
        {
            TIMSK1 &= ~(1 << OCIE1A);
            m1_em_movimento = 0;
        }
    }
}

ISR(TIMER3_COMPA_vect)
{
    if (m2_delay_ticks > 0)
    {
        if (m2_delay_ticks > 65535)
        {
            OCR3A += 65535;
            m2_delay_ticks -= 65535;
        }
        else
        {
            OCR3A += m2_delay_ticks;
            m2_delay_ticks = 0;
        }
    }
    else
    {
        if (m2_passos_restantes > 0)
        {
            PORTC |= (1 << M2_PUL_PIN);
            _delay_us(3);
            PORTC &= ~(1 << M2_PUL_PIN);
            m2_passos_restantes--;

            if (m2_passos_restantes > 0)
            {
                m2_delay_ticks = m2_interval_ticks;
                if (m2_delay_ticks > 65535)
                {
                    OCR3A += 65535;
                    m2_delay_ticks -= 65535;
                }
                else
                {
                    OCR3A += m2_delay_ticks;
                    m2_delay_ticks = 0;
                }
            }
            else
            {
                TIMSK3 &= ~(1 << OCIE3A);
                m2_em_movimento = 0;
            }
        }
        else
        {
            TIMSK3 &= ~(1 << OCIE3A);
            m2_em_movimento = 0;
        }
    }
}

// ==========================================
// EEPROM FAST ACTION PRESETS
// ==========================================
void inicializarPresetsEEPROM()
{
    if (EEPROM.read(EEPROM_MAGIC_ADDR) == EEPROM_MAGIC_BYTE)
        return;

    ComandoMotor defaults[MAX_PRESETS] = {
        {1600, 400, 1, 1, 0, 1}, {800, 300, 0, 1, 0, 1}, {3200, 500, 1, 2, 1000, 1}, {1600, 400, 1, 1, 0, 2}, {800, 200, 0, 1, 500, 2}, {0, 0, 0, 0, 0, 1}, {0, 0, 0, 0, 0, 1}, {0, 0, 0, 0, 0, 1}, {0, 0, 0, 0, 0, 1}, {0, 0, 0, 0, 0, 1}};
    for (uint8_t i = 0; i < MAX_PRESETS; i++)
        gravarPresetEEPROM(i, defaults[i]);
    EEPROM.write(EEPROM_MAGIC_ADDR, EEPROM_MAGIC_BYTE);
}

void gravarPresetEEPROM(uint8_t idx, ComandoMotor cmd)
{
    uint16_t addr = EEPROM_PRESETS_ADDR + (idx * sizeof(ComandoMotor));
    EEPROM.put(addr, cmd);
}

ComandoMotor lerPresetEEPROM(uint8_t idx)
{
    ComandoMotor cmd;
    uint16_t addr = EEPROM_PRESETS_ADDR + (idx * sizeof(ComandoMotor));
    EEPROM.get(addr, cmd);
    return cmd;
}

void executarFastAction(uint8_t idx)
{
    if (m1_executando || m2_executando)
    {
        setUIMessage("E0: Em Execucao");
        return;
    }
    if (idx >= MAX_PRESETS)
    {
        setUIMessage("E4: Preset Inv.");
        return;
    }

    ComandoMotor cmd = lerPresetEEPROM(idx);
    qtd_comandos_na_fila = 0;
    fila_comandos[0] = cmd;
    qtd_comandos_na_fila = 1;

    m1_executando = true;
    m2_executando = true;
    m1_indice_atual = 0;
    m2_indice_atual = 0;

    if (!carregarProximoComando(1))
        m1_executando = false;
    if (!carregarProximoComando(2))
        m2_executando = false;

    if (m1_executando || m2_executando)
    {
        fila_iniciada = true;
        char msg[20]; snprintf_P(msg, sizeof(msg), PSTR("BB: FastAct %d"), idx); setUIMessage(msg);
    }
}

void executarFastActionRepeat(uint8_t idx, int32_t custom_repeat_signed)
{
    if (m1_executando || m2_executando)
    {
        setUIMessage("E0: Em Execucao");
        return;
    }
    if (idx >= MAX_PRESETS)
    {
        setUIMessage("E4: Preset Inv.");
        return;
    }

    ComandoMotor cmd = lerPresetEEPROM(idx);
    if (custom_repeat_signed < 0)
    {
        cmd.repeat = (uint16_t)(-custom_repeat_signed);
        cmd.dir = (cmd.dir == 1) ? 0 : 1;
    }
    else
    {
        cmd.repeat = (uint16_t)custom_repeat_signed;
    }

    qtd_comandos_na_fila = 0;
    fila_comandos[0] = cmd;
    qtd_comandos_na_fila = 1;

    m1_executando = true;
    m2_executando = true;
    m1_indice_atual = 0;
    m2_indice_atual = 0;

    if (!carregarProximoComando(1))
        m1_executando = false;
    if (!carregarProximoComando(2))
        m2_executando = false;

    if (m1_executando || m2_executando)
    {
        fila_iniciada = true;
        char msg[25]; snprintf_P(msg, sizeof(msg), PSTR("BC: FastActRep %d"), idx); setUIMessage(msg);
    }
}

// ==========================================
// CONTROLE DE HARDWARE (MOTORES)
// ==========================================
void moverMotor1(uint32_t passos, uint32_t intervalo_us, uint8_t direcao)
{
    if (passos == 0 || m1_em_movimento)
        return;
    if (direcao)
        PORTA |= (1 << M1_DIR_PIN);
    else
        PORTA &= ~(1 << M1_DIR_PIN);
    PORTA &= ~(1 << M1_EN_PIN); // Habilita o driver (EN = LOW) automaticamente ao mover

    cli();
    m1_passos_restantes = passos;
    m1_em_movimento = 1;
    m1_interval_ticks = intervalo_us * 2;

    m1_delay_ticks = m1_interval_ticks;
    if (m1_delay_ticks > 65535)
    {
        OCR1A = TCNT1 + 65535;
        m1_delay_ticks -= 65535;
    }
    else
    {
        OCR1A = TCNT1 + m1_delay_ticks;
        m1_delay_ticks = 0;
    }

    TIFR1 |= (1 << OCF1A);
    TIMSK1 |= (1 << OCIE1A);
    sei();
}

void moverMotor2(uint32_t passos, uint32_t intervalo_us, uint8_t direcao)
{
    if (passos == 0 || m2_em_movimento)
        return;
    if (direcao)
        PORTC |= (1 << M2_DIR_PIN);
    else
        PORTC &= ~(1 << M2_DIR_PIN);
    PORTC &= ~(1 << M2_EN_PIN);

    cli();
    m2_passos_restantes = passos;
    m2_em_movimento = 1;
    m2_interval_ticks = intervalo_us * 2;

    m2_delay_ticks = m2_interval_ticks;
    if (m2_delay_ticks > 65535)
    {
        OCR3A = TCNT3 + 65535;
        m2_delay_ticks -= 65535;
    }
    else
    {
        OCR3A = TCNT3 + m2_delay_ticks;
        m2_delay_ticks = 0;
    }

    TIFR3 |= (1 << OCF3A);
    TIMSK3 |= (1 << OCIE3A);
    sei();
}

// ==========================================
// LÓGICA DE FILA E PAUSAS
// ==========================================
bool carregarProximoComando(uint8_t motor)
{
    if (qtd_comandos_na_fila == 0)
        return false;

    if (motor == 1)
    {
        while (m1_indice_atual < qtd_comandos_na_fila && fila_comandos[m1_indice_atual].motor_id != 1)
        {
            m1_indice_atual++;
        }
        if (m1_indice_atual >= qtd_comandos_na_fila)
            return false;

        ComandoMotor cmd = fila_comandos[m1_indice_atual];
        m1_comando_infinito = (cmd.repeat == 0);
        m1_repeticoes_restantes = (cmd.repeat > 0) ? (cmd.repeat - 1) : 0;

        moverMotor1(cmd.step, cmd.vel, cmd.dir);
        Serial.print("D0: M1 L"); Serial.println(m1_indice_atual);
        return true;
    }
    else
    {
        while (m2_indice_atual < qtd_comandos_na_fila && fila_comandos[m2_indice_atual].motor_id != 2)
        {
            m2_indice_atual++;
        }
        if (m2_indice_atual >= qtd_comandos_na_fila)
            return false;

        ComandoMotor cmd = fila_comandos[m2_indice_atual];
        m2_comando_infinito = (cmd.repeat == 0);
        m2_repeticoes_restantes = (cmd.repeat > 0) ? (cmd.repeat - 1) : 0;

        moverMotor2(cmd.step, cmd.vel, cmd.dir);
        Serial.print("D0: M2 L"); Serial.println(m2_indice_atual);
        return true;
    }
}

void manageSteppers()
{
    // Tratamento de Pausa Global
    if (global_pause_ms > 0 && !is_global_paused && !m1_em_movimento && !m2_em_movimento && fila_iniciada)
    {
        is_global_paused = true;
        tempo_inicio_global_pause = millis();
        setUIMessage("B3: Pausa Global");
    }

    if (is_global_paused)
    {
        if (millis() - tempo_inicio_global_pause >= global_pause_ms)
        {
            is_global_paused = false;
            global_pause_ms = 0; // Consome a pausa
            setUIMessage("B0: Retomando...");
            // Retoma motores
            if (m1_executando && !carregarProximoComando(1))
                m1_executando = false;
            if (m2_executando && !carregarProximoComando(2))
                m2_executando = false;
        }
        return; // Trava o processamento da fila enquanto estiver na pausa global
    }

    // Gerenciamento Motor 1
    if (m1_executando && !m1_em_movimento && !m1_em_pausa)
    {
        if (m1_comando_infinito || m1_repeticoes_restantes > 0)
        {
            if (!m1_comando_infinito)
                m1_repeticoes_restantes--;
            moverMotor1(fila_comandos[m1_indice_atual].step, fila_comandos[m1_indice_atual].vel, fila_comandos[m1_indice_atual].dir);
        }
        else
        {
            if (fila_comandos[m1_indice_atual].pause_ms > 0)
            {
                m1_em_pausa = true;
                m1_tempo_inicio_pausa = millis();
            }
            else
            {
                m1_indice_atual++;
                if (!carregarProximoComando(1))
                    m1_executando = false;
            }
        }
    }
    if (m1_em_pausa)
    {
        if (millis() - m1_tempo_inicio_pausa >= fila_comandos[m1_indice_atual].pause_ms)
        {
            m1_em_pausa = false;
            m1_indice_atual++;
            if (!carregarProximoComando(1))
                m1_executando = false;
        }
    }

    // Gerenciamento Motor 2
    if (m2_executando && !m2_em_movimento && !m2_em_pausa)
    {
        if (m2_comando_infinito || m2_repeticoes_restantes > 0)
        {
            if (!m2_comando_infinito)
                m2_repeticoes_restantes--;
            moverMotor2(fila_comandos[m2_indice_atual].step, fila_comandos[m2_indice_atual].vel, fila_comandos[m2_indice_atual].dir);
        }
        else
        {
            if (fila_comandos[m2_indice_atual].pause_ms > 0)
            {
                m2_em_pausa = true;
                m2_tempo_inicio_pausa = millis();
            }
            else
            {
                m2_indice_atual++;
                if (!carregarProximoComando(2))
                    m2_executando = false;
            }
        }
    }
    if (m2_em_pausa)
    {
        if (millis() - m2_tempo_inicio_pausa >= fila_comandos[m2_indice_atual].pause_ms)
        {
            m2_em_pausa = false;
            m2_indice_atual++;
            if (!carregarProximoComando(2))
                m2_executando = false;
        }
    }

    // Tratamento de RepeatAll ou Fim de Fila
    if (fila_iniciada && !m1_executando && !m2_executando && !is_global_paused)
    {
        if (repetir_todas_linhas && qtd_comandos_na_fila > 0)
        {
            m1_executando = true;
            m2_executando = true;
            m1_indice_atual = 0;
            m2_indice_atual = 0;
            if (!carregarProximoComando(1))
                m1_executando = false;
            if (!carregarProximoComando(2))
                m2_executando = false;
        }
        else
        {
            fila_iniciada = false;
            setUIMessage("B5: Fila Executada");
        }
    }
}

// ==========================================
// TECLADO E INTERFACE (UI)
// ==========================================
void setUIMessage(const char *msg)
{
    strncpy(currentMsg, msg, MSG_MAX_LEN);
    currentMsg[MSG_MAX_LEN] = '\0';
    systemFlags.isMessageLong = (strlen(currentMsg) > 16) ? 1 : 0;
    systemFlags.msgIsStatus = 1; // Temporária
    scrollIndexBottom = 0;
    lastMsgTime = millis();
    systemFlags.lcdNeedsUpdate = 1;
    Serial.println(msg);
}

char scanKeypadFast()
{
    char key = 0;
    PCICR &= ~(1 << PCIE2);
    for (uint8_t r = 0; r < 4; r++)
    {
        PORTK = ~(1 << r) | 0xF0;
        _delay_us(10);
        uint8_t cols = PINK >> 4;
        if (cols != 0x0F)
        {
            for (uint8_t c = 0; c < 4; c++)
            {
                if (!(cols & (1 << c)))
                {
                    key = matrixKeys[3 - r][3 - c];
                    break;
                }
            }
        }
        if (key)
            break;
    }
    PORTK = 0xF0;
    PCIFR |= (1 << PCIF2);
    PCICR |= (1 << PCIE2);
    return key;
}

void scrollTextToBuffer(char *out, const char *text, uint8_t textLen, uint16_t idx)
{
    const uint8_t GAP = 4;
    uint8_t totalLen = textLen + GAP;
    uint16_t start = idx % totalLen;

    for (uint8_t i = 0; i < 16; i++)
    {
        uint16_t pos = (start + i) % totalLen;
        out[i] = (pos < textLen) ? text[pos] : ' ';
    }
    out[16] = '\0';
}

void updateLCD()
{
    if (systemFlags.menuActive)
    {
        char dispTop[17];
        char dispBot[17];
        if (systemFlags.menuSelection == 0)
        {
            snprintf_P(dispTop, 17, PSTR(">Executar Fila[%d]"), qtd_comandos_na_fila);
            strcpy_P(dispBot, PSTR(" Limpar Fila    "));
        }
        else
        {
            snprintf_P(dispTop, 17, PSTR(" Executar Fila[%d]"), qtd_comandos_na_fila);
            strcpy_P(dispBot, PSTR(">Limpar Fila    "));
        }
        lcd.setCursor(0, 0);
        lcd.print(dispTop);
        lcd.setCursor(0, 1);
        lcd.print(dispBot);
        return;
    }

    char dispTop[17];
    char dispBot[17];

    char prefix[11];
    uint8_t prefixLen;

    if (systemFlags.isFastActMode)
    {
        strcpy_P(prefix, PSTR("[F]Cmd:"));
        prefixLen = 7;
    }
    else if (qtd_comandos_na_fila > 0)
    {
        snprintf_P(prefix, sizeof(prefix), PSTR("[%d]Cmd:"), qtd_comandos_na_fila);
        prefixLen = strlen(prefix);
    }
    else
    {
        strcpy_P(prefix, PSTR("Cmd:"));
        prefixLen = 4;
    }

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
        strncat(dispTop, inputBuffer + (inputLen - maxInLen), maxInLen);
        dispTop[16] = '\0';
    }

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
        scrollTextToBuffer(dispBot, currentMsg, msgLen, scrollIndexBottom);
    }

    lcd.setCursor(0, 0);
    lcd.print(dispTop);
    lcd.setCursor(0, 1);
    lcd.print(dispBot);
}

void handleDisplayTasks()
{
    unsigned long now = millis();

    if (systemFlags.msgIsStatus && (now - lastMsgTime >= MSG_TIMEOUT_MS))
    {
        if (systemFlags.isFastActMode)
            strcpy_P(currentMsg, PSTR("[FAST ACT]"));
        else
            strcpy_P(currentMsg, PSTR("Pronto."));
            
        systemFlags.isMessageLong = 0;
        systemFlags.msgIsStatus = 0;
        scrollIndexBottom = 0;
        systemFlags.lcdNeedsUpdate = 1;
    }

    if (systemFlags.isMessageLong)
    {
        uint8_t msgLen = strlen(currentMsg);
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
}

void enterMenu()
{
    systemFlags.menuActive = 1;
    systemFlags.menuSelection = 0;
    systemFlags.lcdNeedsUpdate = 1;
}

void processMenuKey(char key)
{
    if (key == 'A' || key == 'B')
    {
        systemFlags.menuSelection = !systemFlags.menuSelection;
    }
    else if (key == '#')
    {
        systemFlags.menuActive = 0;
        if (systemFlags.menuSelection == 0)
        {
            char cmdBuf[10];
            strcpy(cmdBuf, "01");
            interpretarComando(cmdBuf);
        }
        else
        {
            qtd_comandos_na_fila = 0;
            setUIMessage("SRAM Limpa!");
            Serial.println(F("C1:0"));
        }
    }
    else if (key == '*')
    {
        systemFlags.menuActive = 0;
        setUIMessage("Menu Fechado");
    }
    systemFlags.lcdNeedsUpdate = 1;
}

ComandoMotor parseLineToMotorCommand(char *linha)
{
    // Parsea uma linha no formato H8P e retorna o struct correspondente.
    char temp[MAX_INPUT_LEN + 1];
    
    // Remove espaços enquanto copia para temp
    char *src = linha;
    char *dst = temp;
    int len = 0;
    while (*src && len < MAX_INPUT_LEN)
    {
        if (*src != ' ')
        {
            *dst++ = *src;
            len++;
        }
        src++;
    }
    *dst = '\0';
    
    ComandoMotor cmd = {0, 0, 0, 1, 0, 1};
    char *ponteiro_virgula;
    char *par = strtok_r(temp, ",", &ponteiro_virgula);

    while (par != NULL)
    {
        char *ponteiro_dois_pontos;
        char *chave_str = strtok_r(par, ":", &ponteiro_dois_pontos);
        char *valor_str = strtok_r(NULL, ":", &ponteiro_dois_pontos);

        if (chave_str != NULL && valor_str != NULL)
        {
            uint8_t chave = (uint8_t)strtol(chave_str, NULL, 16);
            int32_t valor = atol(valor_str);

            switch (chave)
            {
            case 0x10: cmd.step = (uint32_t)valor; break;
            case 0x11: cmd.vel = (uint32_t)valor; break;
            case 0x12: cmd.dir = (uint8_t)valor; break;
            case 0x13: cmd.repeat = (uint16_t)valor; break;
            case 0x14: cmd.pause_ms = (uint32_t)valor; break;
            case 0x15: cmd.motor_id = (uint8_t)valor; break;
            }
        }
        par = strtok_r(NULL, ",", &ponteiro_virgula);
    }
    return cmd;
}

void processKeyInput(char key)
{
    if (systemFlags.menuActive)
    {
        processMenuKey(key);
        return;
    }

    if (systemFlags.isFastActMode && isDigit(key))
    {
        executarFastAction(key - '0');
        return;
    }

    if (saveComboState == 2)
    {
        if (isDigit(key))
        {
            saveComboState = 0;
            if (inputLen > 0)
            {
                ComandoMotor cmdToSave = parseLineToMotorCommand(inputBuffer);
                gravarPresetEEPROM(key - '0', cmdToSave);
                char msg[20];
                snprintf_P(msg, sizeof(msg), PSTR("Slot %d Salvo!"), key - '0');
                setUIMessage(msg);
                inputLen = 0;
                inputBuffer[0] = '\0';
            }
            else setUIMessage("Buffer Vazio!");
            systemFlags.lcdNeedsUpdate = 1;
            return;
        }
        else saveComboState = 0;
    }

    bool hasStar = (inputLen > 0 && inputBuffer[inputLen - 1] == ':');
    bool hasComma = (inputLen > 0 && inputBuffer[inputLen - 1] == ',');

    if (hasStar)
    {
        if (key == '#')
        {
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
                char cmdBuf[MAX_INPUT_LEN + 1];
                strncpy(cmdBuf, inputBuffer, MAX_INPUT_LEN);
                cmdBuf[MAX_INPUT_LEN] = '\0';
                interpretarComando(cmdBuf);
            }
            inputLen = 0;
            inputBuffer[0] = '\0';
        }
        else if (key == 'B')
        {
            inputBuffer[--inputLen] = '\0';
            systemFlags.isFastActMode = !systemFlags.isFastActMode;
            setUIMessage(systemFlags.isFastActMode ? "FAST ACT ON" : "FAST ACT OFF");
        }
        else if (key == 'C')
        {
            inputLen = 0;
            inputBuffer[0] = '\0';
        }
        else if (key == 'D')
        {
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
            inputBuffer[--inputLen] = '\0';
            if (inputLen < MAX_INPUT_LEN)
            {
                inputBuffer[inputLen++] = '-';
                inputBuffer[inputLen] = '\0';
            }
            else setUIMessage("LIMITE!");
        }
        else if (key == '*')
        {
            if (inputLen < MAX_INPUT_LEN)
            {
                inputBuffer[inputLen++] = ':';
                inputBuffer[inputLen] = '\0';
            }
            else setUIMessage("LIMITE!");
        }
        else if (isDigit(key))
        {
            if (inputLen < MAX_INPUT_LEN)
            {
                inputBuffer[inputLen++] = key;
                inputBuffer[inputLen] = '\0';
            }
            else setUIMessage("LIMITE!");
        }
        else
        {
            if (inputLen < MAX_INPUT_LEN)
            {
                inputBuffer[inputLen++] = key;
                inputBuffer[inputLen] = '\0';
            }
            else setUIMessage("LIMITE!");
        }
    }
    else if (hasComma && key == 'A')
    {
        inputBuffer[--inputLen] = '\0';
        saveComboState = 2;
        systemFlags.lcdNeedsUpdate = 1;
        return;
    }
    else
    {
        char toAppend = 0;
        if (key == '#') toAppend = ',';
        else if (key == '*') toAppend = ':';
        else toAppend = key;

        if (toAppend)
        {
            if (inputLen < MAX_INPUT_LEN)
            {
                inputBuffer[inputLen++] = toAppend;
                inputBuffer[inputLen] = '\0';
            }
            else setUIMessage("LIMITE!");
        }
    }

    if (inputLen == 4 && inputBuffer[0] == ':' && inputBuffer[1] == '0' &&
        inputBuffer[2] == '0' && inputBuffer[3] == '0')
    {
        inputLen = 0;
        inputBuffer[0] = '\0';
        enterMenu();
    }

    systemFlags.lcdNeedsUpdate = 1;
}

// ==========================================
// PARSER (Interpretador H8P Completo)
// ==========================================
void interpretarComando(char *linha)
{
    // Remove espaços em branco in-place
    char *src = linha;
    char *dst = linha;
    while (*src)
    {
        if (*src != ' ')
        {
            *dst++ = *src;
        }
        src++;
    }
    *dst = '\0';

    ComandoMotor cmd = {0, 0, 0, 1, 0, 1};
    bool cmd_valido = false;
    bool save_to_eeprom = false;
    uint8_t eeprom_slot = 0;

    char *ponteiro_virgula;
    char *par = strtok_r(linha, ",", &ponteiro_virgula);

    while (par != NULL)
    {
        char *ponteiro_dois_pontos;
        char *chave_str = strtok_r(par, ":", &ponteiro_dois_pontos);
        char *valor_str = strtok_r(NULL, ":", &ponteiro_dois_pontos);
        char *terceiro_str = strtok_r(NULL, ":", &ponteiro_dois_pontos); // Para comandos complexos como 1B e 1C

        if (chave_str != NULL)
        {
            uint8_t chave = (uint8_t)strtol(chave_str, NULL, 16);

            if (valor_str == NULL)
            {
                if (chave == 0x01)
                { // 01: run
                    if (qtd_comandos_na_fila > 0 && (!m1_executando && !m2_executando))
                    {
                        m1_executando = true;
                        m2_executando = true;
                        m1_indice_atual = 0;
                        m2_indice_atual = 0;
                        if (!carregarProximoComando(1))
                            m1_executando = false;
                        if (!carregarProximoComando(2))
                            m2_executando = false;
                        if (m1_executando || m2_executando)
                        {
                            fila_iniciada = true;
                            setUIMessage("B0: Executando");
                        }
                    }
                    else if (m1_executando || m2_executando)
                    {
                        setUIMessage("E0: Em Execucao");
                    }
                    else
                    {
                        setUIMessage("E1: Fila Vazia");
                    }
                    return;
                }
                else if (chave == 0x02)
                { // 02: stop global
                    cli();
                    m1_repeticoes_restantes = 0;
                    m2_repeticoes_restantes = 0;
                    m1_comando_infinito = false;
                    m2_comando_infinito = false;
                    repetir_todas_linhas = false;
                    qtd_comandos_na_fila = 0;
                    if (!m1_em_movimento)
                    {
                        m1_executando = false;
                        m1_em_pausa = false;
                    }
                    if (!m2_em_movimento)
                    {
                        m2_executando = false;
                        m2_em_pausa = false;
                    }
                    fila_iniciada = false;
                    sei();
                    setUIMessage("B1: Motor Parado");
                    Serial.println(F("C1:0"));
                    return;
                }
            }
            else
            {
                int32_t valor = atol(valor_str); // Usado int32 para suportar valores negativos em 1B

                // Comandos Globais
                if (chave == 0x03)
                {
                    repetir_todas_linhas = (valor == 1);
                    setUIMessage(repetir_todas_linhas ? "B4: Rep. ON" : "B6: Rep. OFF");
                    return;
                }
                else if (chave == 0x04)
                {
                    global_pause_ms = valor;
                    char msg[20]; snprintf_P(msg, sizeof(msg), PSTR("B2: Pausa %ldms"), valor); setUIMessage(msg);
                    return;
                }
                else if (chave == 0x16)
                { // enableMotor
                    if (valor == 1)
                        PORTA &= ~(1 << M1_EN_PIN);
                    else if (valor == 2)
                        PORTC &= ~(1 << M2_EN_PIN);
                    char msg[25]; snprintf_P(msg, sizeof(msg), PSTR("B7: M%ld Habilitado"), valor); setUIMessage(msg);
                    return;
                }
                else if (chave == 0x17)
                { // disableMotor
                    if (valor == 1)
                        PORTA |= (1 << M1_EN_PIN);
                    else if (valor == 2)
                        PORTC |= (1 << M2_EN_PIN);
                    char msg[25]; snprintf_P(msg, sizeof(msg), PSTR("B8: M%ld Desabilt."), valor); setUIMessage(msg);
                    return;
                }
                else if (chave == 0x18)
                {
                    executarFastAction(valor);
                    return;
                }
                else if (chave == 0x1A)
                { // readPreset
                    ComandoMotor p = lerPresetEEPROM(valor);
                    char msg[25]; snprintf_P(msg, sizeof(msg), PSTR("BA: L%ld S:%lu V:%lu"), valor, p.step, p.vel); setUIMessage(msg);
                    return;
                }
                else if (chave == 0x1B)
                {
                    int32_t custom_rep = (terceiro_str != NULL) ? atol(terceiro_str) : 1;
                    executarFastActionRepeat(valor, custom_rep);
                    return;
                }
                else if (chave == 0x1C)
                { // saveToEEPROM
                    if (terceiro_str != NULL)
                    {
                        uint8_t idx_eeprom = valor;
                        uint8_t idx_sram = atol(terceiro_str);
                        if (idx_sram < qtd_comandos_na_fila && idx_eeprom < MAX_PRESETS)
                        {
                            gravarPresetEEPROM(idx_eeprom, fila_comandos[idx_sram]);
                            char msg[25]; snprintf_P(msg, sizeof(msg), PSTR("C0: Slot %d Salvo"), idx_eeprom); setUIMessage(msg);
                        }
                        else
                        {
                            setUIMessage("E1: Slot Invalido");
                        }
                    }
                    return;
                }

                // Parâmetros de Linha (Comandos 10 a 15)
                if (chave == 0x10)
                {
                    cmd.step = valor;
                    cmd_valido = true;
                }
                else if (chave == 0x11)
                {
                    cmd.vel = valor;
                }
                else if (chave == 0x12)
                {
                    cmd.dir = valor;
                }
                else if (chave == 0x13)
                {
                    cmd.repeat = valor;
                }
                else if (chave == 0x14)
                {
                    cmd.pause_ms = valor;
                }
                else if (chave == 0x15)
                {
                    cmd.motor_id = valor;
                }
                else if (chave == 0x19)
                { // writePreset deferido para o final do parser
                    save_to_eeprom = true;
                    eeprom_slot = valor;
                }
            }
        }
        par = strtok_r(NULL, ",", &ponteiro_virgula);
    }

    if (save_to_eeprom)
    {
        if (cmd_valido && eeprom_slot < MAX_PRESETS)
        {
            gravarPresetEEPROM(eeprom_slot, cmd);
            char msg[25]; snprintf_P(msg, sizeof(msg), PSTR("Preset %d Salvo"), eeprom_slot);
            setUIMessage(msg);
            
            Serial.print(F("B9:")); Serial.print(eeprom_slot);
            Serial.print(F(",10:")); Serial.print(cmd.step);
            Serial.print(F(",11:")); Serial.print(cmd.vel);
            Serial.print(F(",12:")); Serial.print(cmd.dir);
            Serial.print(F(",13:")); Serial.print(cmd.repeat);
            Serial.print(F(",14:")); Serial.print(cmd.pause_ms);
            Serial.print(F(",15:")); Serial.println(cmd.motor_id);
        }
        else
        {
            setUIMessage("E3: Erro Sintaxe");
        }
        return; // Não adiciona à fila SRAM
    }

    if (cmd_valido)
    {
        if (qtd_comandos_na_fila < MAX_FILA)
        {
            fila_comandos[qtd_comandos_na_fila] = cmd;
            Serial.print(F("C0:")); Serial.println(qtd_comandos_na_fila);
            qtd_comandos_na_fila++;
            Serial.print(F("C1:")); Serial.println(qtd_comandos_na_fila);
            char msg[20]; snprintf_P(msg, sizeof(msg), PSTR("Slot %d (Fila)"), qtd_comandos_na_fila); setUIMessage(msg);
        }
        else
        {
            setUIMessage("E2: Fila Cheia");
        }
    }
}

// ==========================================
// LOOP PRINCIPAL
// ==========================================
void diagnosticoEEPROM()
{
    Serial.println(F("--- EEPROM Slots ---"));
    for (uint8_t s = 0; s < MAX_PRESETS; s++)
    {
        ComandoMotor cmd = lerPresetEEPROM(s);
        Serial.print(F("Slot "));
        Serial.print(s);
        Serial.print(F(": "));
        if (cmd.step == 0 && cmd.vel == 0)
            Serial.println(F("[Vazio]"));
        else
        {
            Serial.print(F("S:"));
            Serial.print(cmd.step);
            Serial.print(F(" V:"));
            Serial.print(cmd.vel);
            Serial.print(F(" D:"));
            Serial.print(cmd.dir);
            Serial.print(F(" R:"));
            Serial.print(cmd.repeat);
            Serial.print(F(" P:"));
            Serial.print(cmd.pause_ms);
            Serial.print(F(" M:"));
            Serial.println(cmd.motor_id);
        }
    }
    Serial.println(F("--------------------"));
}

void loop()
{
    if (keyPressFlag)
    {
        _delay_ms(20);
        char key = scanKeypadFast();
        if (key)
        {
            processKeyInput(key);
            while ((PINK >> 4) != 0x0F)
                ;
            _delay_ms(20);
        }
        keyPressFlag = false;
    }

    handleDisplayTasks();
    manageSteppers();
}