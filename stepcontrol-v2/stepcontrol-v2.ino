/*
 * =================================================================================
 * Stepper Core - Controle de Motores Dedicado (Mega 2560)
 * =================================================================================
 * Placa: Arduino Mega 2560
 * Função: Motor Core, Fila SRAM, Interpretador H8P (API Desacoplada)
 * * MAPEAMENTO DE HARDWARE (BAIXO NÍVEL):
 * - Motor 1: PORTA (PA0=22 DIR, PA2=24 PUL, PA4=26 EN) -> Usando Timer 1
 * - Motor 2: PORTC (PC7=30 DIR, PC5=32 PUL, PC3=34 EN) -> Usando Timer 3
 * * Comunicação Serial: RX0/TX0 a 9600 bps com o Commander (ATmega328P)
 * * =================================================================================
 */

#include <Arduino.h>
#include <EEPROM.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/delay.h>

// ==========================================
// DEFINIÇÕES DE PINOS DOS MOTORES
// ==========================================
// Motor 1 (PORTA e PORTB)
#define M1_DIR_PIN PA0 // Pino 22
#define M1_PUL_PIN PB5 // Pino 11 (OC1A)
#define M1_EN_PIN  PA4 // Pino 26

// Motor 2 (PORTC e PORTE)
#define M2_DIR_PIN PC7 // Pino 30
#define M2_PUL_PIN PE3 // Pino 5 (OC3A)
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
bool m1_executando = false;
uint8_t m1_indice_atual = 0;
uint16_t m1_repeticoes_restantes = 0;
bool m1_em_pausa = false;
uint32_t m1_tempo_inicio_pausa = 0;
bool m1_comando_infinito = false;

// --- Estado Motor 2 ---
volatile uint32_t m2_passos_restantes = 0;
volatile uint8_t m2_em_movimento = 0;
bool m2_executando = false;
uint8_t m2_indice_atual = 0;
uint16_t m2_repeticoes_restantes = 0;
bool m2_em_pausa = false;
uint32_t m2_tempo_inicio_pausa = 0;
bool m2_comando_infinito = false;

// ==========================================
// BUFFER SERIAL
// ==========================================
#define MAX_INPUT_LEN 64
char serialBuffer[MAX_INPUT_LEN + 1];
uint8_t serialBufferIdx = 0;

// ==========================================
// PROTÓTIPOS
// ==========================================
void interpretarComando(char *linha);
void diagnosticoEEPROM();
ComandoMotor parseLineToMotorCommand(char *linha);
bool carregarProximoComando(uint8_t motor);
void moverMotor1(uint32_t passos, uint32_t intervalo_us, uint8_t direcao);
void moverMotor2(uint32_t passos, uint32_t intervalo_us, uint8_t direcao);
void inicializarPresetsEEPROM();
void gravarPresetEEPROM(uint8_t idx, ComandoMotor cmd);
ComandoMotor lerPresetEEPROM(uint8_t idx);
void executarFastAction(uint8_t idx);
void executarFastActionRepeat(uint8_t idx, int32_t custom_repeat_signed);
void readSerial();

// ==========================================
// SETUP
// ==========================================
void setup()
{
    Serial.begin(9600);

    // 1. Configura Pinos dos Motores como SAÍDA
    DDRA |= (1 << M1_DIR_PIN) | (1 << M1_EN_PIN);
    DDRB |= (1 << M1_PUL_PIN); // PB5 (Pin 11)
    
    DDRC |= (1 << M2_DIR_PIN) | (1 << M2_EN_PIN);
    DDRE |= (1 << M2_PUL_PIN); // PE3 (Pin 5)
    
    // Ativa pull-up no ENABLE (Motor Desabilitado inicialmente) e garante PUL/DIR em LOW
    PORTA |= (1 << M1_EN_PIN);
    PORTA &= ~(1 << M1_DIR_PIN);
    PORTB &= ~(1 << M1_PUL_PIN);
    
    PORTC |= (1 << M2_EN_PIN);
    PORTC &= ~(1 << M2_DIR_PIN);
    PORTE &= ~(1 << M2_PUL_PIN);

    cli();
    // 2. Configura Timer 1 (Motor 1) - Fast PWM Mode 14, Prescaler 8
    // WGM13=1, WGM12=1, WGM11=1, WGM10=0
    TCCR1A = (1 << WGM11);
    TCCR1B = (1 << WGM13) | (1 << WGM12) | (1 << CS11);
    TCNT1 = 0;

    // 3. Configura Timer 3 (Motor 2) - Fast PWM Mode 14, Prescaler 8
    // WGM33=1, WGM32=1, WGM31=1, WGM30=0
    TCCR3A = (1 << WGM31);
    TCCR3B = (1 << WGM33) | (1 << WGM32) | (1 << CS31);
    TCNT3 = 0;
    sei();

    inicializarPresetsEEPROM();

    diagnosticoEEPROM();

    Serial.println(F("A0: Inicializado"));
}

// ==========================================
// ROTINAS DE INTERRUPÇÃO (ISRs)
// ==========================================
ISR(TIMER1_COMPA_vect)
{
    m1_passos_restantes--;
    if (m1_passos_restantes == 0)
    {
        // Desconecta o pino OC1A do Timer (volta para operação normal de porta, que está em LOW)
        TCCR1A &= ~(1 << COM1A1);
        TIMSK1 &= ~(1 << OCIE1A);
        m1_em_movimento = 0;
    }
}

ISR(TIMER3_COMPA_vect)
{
    m2_passos_restantes--;
    if (m2_passos_restantes == 0)
    {
        // Desconecta o pino OC3A do Timer
        TCCR3A &= ~(1 << COM3A1);
        TIMSK3 &= ~(1 << OCIE3A);
        m2_em_movimento = 0;
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
        Serial.println(F("E0: Em Execucao"));
        return;
    }
    if (idx >= MAX_PRESETS)
    {
        Serial.println(F("E4: Preset Inv."));
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
        Serial.print(F("BB: FastAct ")); Serial.println(idx);
    }
}

void executarFastActionRepeat(uint8_t idx, int32_t custom_repeat_signed)
{
    if (m1_executando || m2_executando)
    {
        Serial.println(F("E0: Em Execucao"));
        return;
    }
    if (idx >= MAX_PRESETS)
    {
        Serial.println(F("E4: Preset Inv."));
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
        Serial.print(F("BC: FastActRep ")); Serial.println(idx);
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
    
    uint32_t interval_ticks = intervalo_us * 2;
    if (interval_ticks > 65535) interval_ticks = 65535; // Limite de 32.7ms
    if (interval_ticks < 100) interval_ticks = 100;    // Limite de segurança (50µs)

    ICR1 = interval_ticks - 1;
    OCR1A = 4; // 2µs pulse width fixo

    TCNT1 = ICR1 - 1; // Força overflow imediato para o primeiro pulso

    // Conecta OC1A: Set HIGH no BOTTOM, Clear no Compare Match
    TCCR1A |= (1 << COM1A1);
    
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
    
    uint32_t interval_ticks = intervalo_us * 2;
    if (interval_ticks > 65535) interval_ticks = 65535;
    if (interval_ticks < 100) interval_ticks = 100;

    ICR3 = interval_ticks - 1;
    OCR3A = 4; // 2µs pulse width fixo

    TCNT3 = ICR3 - 1; // Força overflow imediato para o primeiro pulso

    // Conecta OC3A: Set HIGH no BOTTOM, Clear no Compare Match
    TCCR3A |= (1 << COM3A1);
    
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
        Serial.print(F("D0: M1 L")); Serial.println(m1_indice_atual);
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
        Serial.print(F("D0: M2 L")); Serial.println(m2_indice_atual);
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
        Serial.println(F("B3: Pausa Global"));
    }

    if (is_global_paused)
    {
        if (millis() - tempo_inicio_global_pause >= global_pause_ms)
        {
            is_global_paused = false;
            global_pause_ms = 0; // Consome a pausa
            Serial.println(F("B0: Retomando..."));
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
            Serial.println(F("B5: Fila Executada"));
        }
    }
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

    if (strlen(linha) == 0) return;

    // COMANDOS DE INTERFACE / COMMANDER
    // O Commander pode mandar comandos como apagar fila (via C)
    if (strcmp(linha, "CLEAR") == 0)
    {
        qtd_comandos_na_fila = 0;
        Serial.println(F("C1:0"));
        return;
    }

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
                            Serial.println(F("B0: Executando"));
                        }
                    }
                    else if (m1_executando || m2_executando)
                    {
                        Serial.println(F("E0: Em Execucao"));
                    }
                    else
                    {
                        Serial.println(F("E1: Fila Vazia"));
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
                    Serial.println(F("B1: Motor Parado"));
                    Serial.println(F("C1:0"));
                    return;
                }
            }
            else
            {
                int32_t valor = atol(valor_str);

                // Comandos Globais
                if (chave == 0x03)
                {
                    repetir_todas_linhas = (valor == 1);
                    Serial.println(repetir_todas_linhas ? F("B4: Rep. ON") : F("B6: Rep. OFF"));
                    return;
                }
                else if (chave == 0x04)
                {
                    global_pause_ms = valor;
                    Serial.print(F("B2: Pausa ")); Serial.print(valor); Serial.println(F("ms"));
                    return;
                }
                else if (chave == 0x16)
                { // enableMotor
                    if (valor == 1)
                        PORTA &= ~(1 << M1_EN_PIN);
                    else if (valor == 2)
                        PORTC &= ~(1 << M2_EN_PIN);
                    Serial.print(F("B7: M")); Serial.print(valor); Serial.println(F(" Habilitado"));
                    return;
                }
                else if (chave == 0x17)
                { // disableMotor
                    if (valor == 1)
                        PORTA |= (1 << M1_EN_PIN);
                    else if (valor == 2)
                        PORTC |= (1 << M2_EN_PIN);
                    Serial.print(F("B8: M")); Serial.print(valor); Serial.println(F(" Desabilt."));
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
                    Serial.print(F("BA: L")); Serial.print(valor);
                    Serial.print(F(" S:")); Serial.print(p.step);
                    Serial.print(F(" V:")); Serial.println(p.vel);
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
                            Serial.print(F("C0: Slot ")); Serial.print(idx_eeprom); Serial.println(F(" Salvo"));
                        }
                        else
                        {
                            Serial.println(F("E1: Slot Invalido"));
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
            Serial.println(F("E3: Erro Sintaxe"));
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
        }
        else
        {
            Serial.println(F("E2: Fila Cheia"));
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

void readSerial()
{
    while (Serial.available() > 0)
    {
        char c = Serial.read();
        
        if (c == '\n' || c == '\r')
        {
            if (serialBufferIdx > 0)
            {
                serialBuffer[serialBufferIdx] = '\0';
                interpretarComando(serialBuffer);
                serialBufferIdx = 0;
            }
        }
        else if (serialBufferIdx < MAX_INPUT_LEN)
        {
            serialBuffer[serialBufferIdx++] = c;
        }
    }
}

void loop()
{
    readSerial();
    manageSteppers();
}
