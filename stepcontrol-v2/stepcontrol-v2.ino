/*
 * =================================================================================
 * Stepper Commander - Controle Integrado Mega 2560 (Monolítico)
 * =================================================================================
 * Placa: Arduino Mega 2560
 * Função: Interface H8P, Parser de Comandos e Controle de Motores Dual Timer
 * * MAPEAMENTO DE HARDWARE (BAIXO NÍVEL):
 * - Motor 1: PORTA (PA0=22 DIR, PA1=23 PUL, PA2=24 EN) -> Usando Timer 1
 * - Motor 2: PORTA (PA3=25 DIR, PA4=26 PUL, PA5=27 EN) -> Usando Timer 3
 * - Teclado Matricial: PORTK (Linhas: A8-A11 | Colunas: A12-A15) -> Interrupção PCINT2
 * - Display LCD 16x2 I2C: Pinos de Hardware SDA(20) e SCL(21)
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
// DEFINIÇÕES DE PINOS DOS MOTORES (PORTA)
// ==========================================
#define M1_DIR_PIN PA0 // Pino 22
#define M1_PUL_PIN PA1 // Pino 23
#define M1_EN_PIN  PA2 // Pino 24

#define M2_DIR_PIN PA3 // Pino 25
#define M2_PUL_PIN PA4 // Pino 26
#define M2_EN_PIN  PA5 // Pino 27

// ==========================================
// ESTRUTURAS E EEPROM
// ==========================================
#define MAX_FILA 20
#define EEPROM_MAGIC_BYTE 0xA5
#define EEPROM_MAGIC_ADDR 0
#define EEPROM_PRESETS_ADDR 1
#define MAX_PRESETS 10

typedef struct {
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

// ==========================================
// VARIÁVEIS DA INTERFACE (UI)
// ==========================================
LiquidCrystal_I2C lcd(0x27, 16, 2);
String inputBuffer = "";
String lastCommand = "";
const int MAX_INPUT_LEN = 32;
String currentMsg = "Sis Inicializado";
unsigned long lastScrollTime = 0;
int scrollIndexBottom = 0;

// Variáveis do Teclado (PCINT)
volatile bool keyPressFlag = false;
char matrixKeys[4][4] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

// ==========================================
// PROTÓTIPOS
// ==========================================
void updateLCD();
void setUIMessage(String msg);
void interpretarComando(char *linha);
bool carregarProximoComando(uint8_t motor);
void moverMotor1(uint32_t passos, uint32_t intervalo_us, uint8_t direcao);
void moverMotor2(uint32_t passos, uint32_t intervalo_us, uint8_t direcao);

// ==========================================
// SETUP
// ==========================================
void setup() {
    Serial.begin(9600); // Apenas para debug

    // 1. Configura LCD
    lcd.init();
    lcd.backlight();
    updateLCD();

    // 2. Configura Pinos dos Motores (PORTA) como SAÍDA e desliga (LOW)
    DDRA |= (1 << M1_PUL_PIN) | (1 << M1_DIR_PIN) | (1 << M1_EN_PIN) |
            (1 << M2_PUL_PIN) | (1 << M2_DIR_PIN) | (1 << M2_EN_PIN);
    PORTA &= ~((1 << M1_PUL_PIN) | (1 << M1_DIR_PIN) | (1 << M1_EN_PIN) |
               (1 << M2_PUL_PIN) | (1 << M2_DIR_PIN) | (1 << M2_EN_PIN));

    cli();
    // 3. Configura Timer 1 (Motor 1) - Prescaler 8 (2 MHz = 0.5us por tick)
    TCCR1A = 0;
    TCCR1B = (1 << CS11);
    TCNT1 = 0;

    // 4. Configura Timer 3 (Motor 2) - Prescaler 8 (2 MHz = 0.5us por tick)
    TCCR3A = 0;
    TCCR3B = (1 << CS31);
    TCNT3 = 0;

    // 5. Configura Teclado no PORTK com Interrupção (PCINT2)
    // PK0-PK3 (A8-A11) como Saídas (Linhas), PK4-PK7 (A12-A15) como Entradas (Colunas)
    DDRK = 0x0F; 
    PORTK = 0xF0; // Linhas em LOW, Colunas com Pull-up ativado
    
    PCICR |= (1 << PCIE2); // Habilita interrupção PCINT2 (PORTK)
    PCMSK2 |= 0xF0;        // Mascara para ativar PCINT apenas nas colunas (PK4 a PK7)
    sei();

    setUIMessage("Pronto.");
}

// ==========================================
// ROTINAS DE INTERRUPÇÃO (ISRs)
// ==========================================

// ISR Teclado (Dispara ao pressionar ou soltar tecla)
ISR(PCINT2_vect) {
    keyPressFlag = true;
}

// ISR Motor 1 (Timer 1)
ISR(TIMER1_COMPA_vect) {
    if (m1_delay_ticks > 0) {
        if (m1_delay_ticks > 65535) {
            OCR1A += 65535;
            m1_delay_ticks -= 65535;
        } else {
            OCR1A += m1_delay_ticks;
            m1_delay_ticks = 0;
        }
    } else {
        if (m1_passos_restantes > 0) {
            PORTA |= (1 << M1_PUL_PIN);   // PULSO ALTO
            _delay_us(3);                 // Exigência TB6600
            PORTA &= ~(1 << M1_PUL_PIN);  // PULSO BAIXO
            m1_passos_restantes--;

            if (m1_passos_restantes > 0) {
                m1_delay_ticks = m1_interval_ticks;
                if (m1_delay_ticks > 65535) {
                    OCR1A += 65535;
                    m1_delay_ticks -= 65535;
                } else {
                    OCR1A += m1_delay_ticks;
                    m1_delay_ticks = 0;
                }
            } else {
                TIMSK1 &= ~(1 << OCIE1A); // Desliga interrupção
                m1_em_movimento = 0;
            }
        } else {
            TIMSK1 &= ~(1 << OCIE1A);
            m1_em_movimento = 0;
        }
    }
}

// ISR Motor 2 (Timer 3)
ISR(TIMER3_COMPA_vect) {
    if (m2_delay_ticks > 0) {
        if (m2_delay_ticks > 65535) {
            OCR3A += 65535;
            m2_delay_ticks -= 65535;
        } else {
            OCR3A += m2_delay_ticks;
            m2_delay_ticks = 0;
        }
    } else {
        if (m2_passos_restantes > 0) {
            PORTA |= (1 << M2_PUL_PIN);   
            _delay_us(3);
            PORTA &= ~(1 << M2_PUL_PIN);  
            m2_passos_restantes--;

            if (m2_passos_restantes > 0) {
                m2_delay_ticks = m2_interval_ticks;
                if (m2_delay_ticks > 65535) {
                    OCR3A += 65535;
                    m2_delay_ticks -= 65535;
                } else {
                    OCR3A += m2_delay_ticks;
                    m2_delay_ticks = 0;
                }
            } else {
                TIMSK3 &= ~(1 << OCIE3A); 
                m2_em_movimento = 0;
            }
        } else {
            TIMSK3 &= ~(1 << OCIE3A);
            m2_em_movimento = 0;
        }
    }
}

// ==========================================
// CONTROLE DE HARDWARE (MOTORES)
// ==========================================
void moverMotor1(uint32_t passos, uint32_t intervalo_us, uint8_t direcao) {
    if (passos == 0 || m1_em_movimento) return;
    if (direcao) PORTA |= (1 << M1_DIR_PIN);
    else         PORTA &= ~(1 << M1_DIR_PIN);

    cli();
    m1_passos_restantes = passos;
    m1_em_movimento = 1;
    m1_interval_ticks = intervalo_us * 2; // Prescaler 8 = 0.5us p/ tick

    m1_delay_ticks = m1_interval_ticks;
    if (m1_delay_ticks > 65535) {
        OCR1A = TCNT1 + 65535;
        m1_delay_ticks -= 65535;
    } else {
        OCR1A = TCNT1 + m1_delay_ticks;
        m1_delay_ticks = 0;
    }

    TIFR1 |= (1 << OCF1A);   
    TIMSK1 |= (1 << OCIE1A); 
    sei();
}

void moverMotor2(uint32_t passos, uint32_t intervalo_us, uint8_t direcao) {
    if (passos == 0 || m2_em_movimento) return;
    if (direcao) PORTA |= (1 << M2_DIR_PIN);
    else         PORTA &= ~(1 << M2_DIR_PIN);

    cli();
    m2_passos_restantes = passos;
    m2_em_movimento = 1;
    m2_interval_ticks = intervalo_us * 2;

    m2_delay_ticks = m2_interval_ticks;
    if (m2_delay_ticks > 65535) {
        OCR3A = TCNT3 + 65535;
        m2_delay_ticks -= 65535;
    } else {
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
bool carregarProximoComando(uint8_t motor) {
    // Implementação básica para carregar e iniciar o próximo movimento da fila
    if(qtd_comandos_na_fila == 0) return false;
    
    if(motor == 1) {
        // Encontra próximo comando do Motor 1
        while(m1_indice_atual < qtd_comandos_na_fila && fila_comandos[m1_indice_atual].motor_id != 1) {
            m1_indice_atual++;
        }
        if(m1_indice_atual >= qtd_comandos_na_fila) return false;

        ComandoMotor cmd = fila_comandos[m1_indice_atual];
        moverMotor1(cmd.step, cmd.vel, cmd.dir);
        return true;
    } else {
        // Encontra próximo comando do Motor 2
        while(m2_indice_atual < qtd_comandos_na_fila && fila_comandos[m2_indice_atual].motor_id != 2) {
            m2_indice_atual++;
        }
        if(m2_indice_atual >= qtd_comandos_na_fila) return false;

        ComandoMotor cmd = fila_comandos[m2_indice_atual];
        moverMotor2(cmd.step, cmd.vel, cmd.dir);
        return true;
    }
}

void manageSteppers() {
    // Motor 1: Check se o movimento acabou e carrega o próximo se necessário
    if(m1_executando && !m1_em_movimento && !m1_em_pausa) {
        // Lógica simplificada: pausa após a linha
        if(fila_comandos[m1_indice_atual].pause_ms > 0) {
            m1_em_pausa = true;
            m1_tempo_inicio_pausa = millis();
        } else {
            m1_indice_atual++;
            if(!carregarProximoComando(1)) {
                m1_executando = false; // Fim da fila M1
            }
        }
    }
    
    // Tratamento de pausa assíncrona Motor 1
    if(m1_em_pausa) {
        if(millis() - m1_tempo_inicio_pausa >= fila_comandos[m1_indice_atual].pause_ms) {
            m1_em_pausa = false;
            m1_indice_atual++;
            if(!carregarProximoComando(1)) m1_executando = false;
        }
    }

    // (Lógica semelhante pode ser replicada aqui para o Motor 2 controlando m2_executando)
    // ...
    
    if(fila_iniciada && !m1_executando && !m2_executando) {
        fila_iniciada = false;
        setUIMessage("Fila Executada");
    }
}

// ==========================================
// TECLADO E INTERFACE (UI)
// ==========================================
void setUIMessage(String msg) {
    currentMsg = msg;
    scrollIndexBottom = 0;
    updateLCD();
    Serial.println(msg); // Debug
}

// Varredura Rápida do Teclado (Chamada apenas quando PCINT dispara)
char scanKeypadFast() {
    char key = 0;
    // Desabilita interrupção momentaneamente para escanear
    PCICR &= ~(1 << PCIE2);
    
    for (uint8_t r = 0; r < 4; r++) {
        PORTK = ~(1 << r) | 0xF0; // Põe 0 na linha atual, mantém PULL-UPs nas colunas
        _delay_us(10);            // Estabilização de sinal
        
        uint8_t cols = PINK >> 4; // Lê as colunas (A12-A15)
        if (cols != 0x0F) {
            for (uint8_t c = 0; c < 4; c++) {
                if (!(cols & (1 << c))) {
                    key = matrixKeys[r][c];
                    break;
                }
            }
        }
        if (key) break;
    }
    
    // Restaura estado de repouso (Linhas LOW) e religa PCINT
    PORTK = 0xF0; 
    PCIFR |= (1 << PCIF2); // Limpa flag pendente
    PCICR |= (1 << PCIE2); // Religa interrupção
    return key;
}

String getScrollText(String text, int index) {
    if (text.length() <= 16) {
        while (text.length() < 16) text += " ";
        return text;
    }
    String padded = text + "    ";
    int len = padded.length();
    int idx = index % len;
    String result = padded.substring(idx) + padded.substring(0, idx);
    return result.substring(0, 16);
}

void updateLCD() {
    String dispTop = "Cmd:";
    if (inputBuffer.length() <= 12) {
        dispTop += inputBuffer;
        while (dispTop.length() < 16) dispTop += " ";
    } else {
        dispTop += inputBuffer.substring(inputBuffer.length() - 12);
    }
    String dispBot = getScrollText(currentMsg, scrollIndexBottom);

    lcd.setCursor(0, 0); lcd.print(dispTop);
    lcd.setCursor(0, 1); lcd.print(dispBot);
}

void handleScroll() {
    if (millis() - lastScrollTime >= 400) {
        lastScrollTime = millis();
        if (currentMsg.length() > 16) {
            scrollIndexBottom++;
            updateLCD();
        }
    }
}

void processKeyInput(char key) {
    if (inputBuffer.endsWith(":")) {
        if (key == '#') {
            inputBuffer.remove(inputBuffer.length() - 1);
            if (inputBuffer.length() == 0 && lastCommand.length() > 0) inputBuffer = lastCommand;
            else if (inputBuffer.length() > 0) lastCommand = inputBuffer;

            if (inputBuffer.length() > 0) {
                // Ao invez de enviar por serial, passamos direto para o motor core!
                char cmdBuf[MAX_INPUT_LEN];
                inputBuffer.toCharArray(cmdBuf, MAX_INPUT_LEN);
                interpretarComando(cmdBuf); 
            }
            inputBuffer = "";
        }
        else if (key == 'C') inputBuffer = "";
        else if (key == 'D') {
            if (inputBuffer.length() >= 2) inputBuffer.remove(inputBuffer.length() - 2);
            else inputBuffer = "";
        }
        else if (key == 'A') inputBuffer.setCharAt(inputBuffer.length() - 1, '-');
        else if (key == '*') { if (inputBuffer.length() < MAX_INPUT_LEN) inputBuffer += ":"; }
        else { if (inputBuffer.length() < MAX_INPUT_LEN) inputBuffer += key; }
    } else {
        if (key == '#') { if (inputBuffer.length() < MAX_INPUT_LEN) inputBuffer += ","; }
        else if (key == '*') { if (inputBuffer.length() < MAX_INPUT_LEN) inputBuffer += ":"; }
        else { if (inputBuffer.length() < MAX_INPUT_LEN) inputBuffer += key; }
    }
    updateLCD();
}

// ==========================================
// PARSER (Interpretador Interno)
// ==========================================
void interpretarComando(char *linha) {
    // Remove espaços
    char *d = linha; do { while (*d == ' ') d++; } while ((*linha++ = *d++));
    linha = linha - (d - linha); // reset ponteiro

    ComandoMotor cmd = {0, 0, 0, 1, 0, 1}; // Default = M1
    bool cmd_valido = false;

    char *ponteiro_virgula;
    char *par = strtok_r(linha, ",", &ponteiro_virgula);

    while (par != NULL) {
        char *ponteiro_dois_pontos;
        char *chave_str = strtok_r(par, ":", &ponteiro_dois_pontos);
        char *valor_str = strtok_r(NULL, ":", &ponteiro_dois_pontos);

        if (chave_str != NULL) {
            uint8_t chave = (uint8_t)strtol(chave_str, NULL, 16);

            if (valor_str == NULL) {
                if (chave == 0x01) { // run
                    if (qtd_comandos_na_fila > 0 && (!m1_executando && !m2_executando)) {
                        m1_executando = true; m2_executando = true;
                        m1_indice_atual = 0; m2_indice_atual = 0;
                        if (!carregarProximoComando(1)) m1_executando = false;
                        if (!carregarProximoComando(2)) m2_executando = false;
                        if (m1_executando || m2_executando) {
                            fila_iniciada = true;
                            setUIMessage("Executando...");
                        }
                    } else if (m1_executando || m2_executando) {
                        setUIMessage("Err: Em Execucao");
                    } else {
                        setUIMessage("Err: Fila Vazia");
                    }
                    return;
                }
                else if (chave == 0x02) { // stop global
                    cli();
                    m1_repeticoes_restantes = 0; m2_repeticoes_restantes = 0;
                    qtd_comandos_na_fila = 0;
                    if (!m1_em_movimento) m1_executando = false;
                    if (!m2_em_movimento) m2_executando = false;
                    fila_iniciada = false;
                    sei();
                    setUIMessage("Motor Parado");
                    return;
                }
            } else {
                uint32_t valor = atol(valor_str);
                if (chave == 0x10) { cmd.step = valor; cmd_valido = true; }
                else if (chave == 0x11) { cmd.vel = valor; }
                else if (chave == 0x12) { cmd.dir = valor; }
                else if (chave == 0x14) { cmd.pause_ms = valor; }
                else if (chave == 0x15) { cmd.motor_id = valor; }
            }
        }
        par = strtok_r(NULL, ",", &ponteiro_virgula);
    }

    // Se montou um comando válido de fila, adiciona
    if(cmd_valido) {
        if(qtd_comandos_na_fila < MAX_FILA) {
            fila_comandos[qtd_comandos_na_fila] = cmd;
            qtd_comandos_na_fila++;
            setUIMessage("Add: M" + String(cmd.motor_id) + " Stp:" + String(cmd.step));
        } else {
            setUIMessage("Err: Fila Cheia");
        }
    }
}

// ==========================================
// LOOP PRINCIPAL (RTOS-Like)
// ==========================================
void loop() {
    // 1. Tratamento do Teclado assíncrono (Debounce Simplificado)
    if (keyPressFlag) {
        _delay_ms(20); // Debounce de hardware (teclados matriciais tremem muito)
        char key = scanKeypadFast();
        if (key) {
            processKeyInput(key);
            
            // Trava para não ler repetições descontroladas enquanto segura
            while((PINK >> 4) != 0x0F); 
            _delay_ms(20);
        }
        keyPressFlag = false;
    }

    // 2. Interface LCD
    handleScroll();

    // 3. Máquina de Estados dos Motores (Gerencia pausas e avanço da fila)
    manageSteppers();
}