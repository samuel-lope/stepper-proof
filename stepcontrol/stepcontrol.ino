/*
 * =================================================================================
 * Controle Independente de 2 Motores de Passo (Dual-Channel Timer1 Freerunning)
 * =================================================================================
 *
 * ÍNDICE DE CÓDIGOS HEXADECIMAIS (8-bits)
 *
 * --- COMANDOS E PARÂMETROS (ENVIAR PARA O ARDUINO) ---
 * 01 : run       (Inicia a execução da fila)
 * 02 : stop      (Parada de emergência e limpa fila)
 * 03 : repeatAll (Booleano: 03:1 = ativa, 03:0 = desativa loop infinito)
 * 04 : pause     (Pausa global. Ex: 04:1000)
 * 10 : step      (Obrigatório - Quantidade de passos)
 * 11 : vel       (Obrigatório - Intervalo em microssegundos)
 * 12 : dir       (Opcional - Direção: 0 ou 1)
 * 13 : repeat    (Opcional - Repetições da linha. 0 = infinito)
 * 14 : pause     (Opcional - Pausa em ms após a linha)
 * 15 : motor     (Opcional - Motor Alvo 1 ou 2. Se vazio, motor 1)
 * 16 : enableMotor  (Habilita driver do motor. Ex: 16:1 ou 16:2 — EN LOW)
 * 17 : disableMotor (Desabilita driver do motor. Ex: 17:1 ou 17:2 — EN HIGH)
 * 18 : fastAction   (Executa preset EEPROM imediatamente. Ex: 18:0 a 18:9)
 * 19 : writePreset  (Grava preset na EEPROM. Ex: 19:2,10:800,11:300...)
 * 1A : readPreset   (Lê preset da EEPROM. Ex: 1A:2)
 * 1B : fastActionRep(Executa preset EEPROM com loop customizado. Ex: 1B:0:-4)
 * 1C : saveToEEPROM (Salva linha da SRAM na EEPROM. Ex: 1C:3:2)
 *
 * --- ALERTAS E RESPOSTAS (RECEBIDOS DO ARDUINO) ---
 * A0 : Sistema Inicializado com Sucesso
 * B0 : Iniciando execucao da fila
 * B1 : Motor PARADO e Fila limpa
 * B2 : Pausa global definida
 * B3 : [TELEMETRIA] Pausa em andamento (ms)
 * B4 : Modo repeatAll ATIVADO
 * B5 : Fila executada com sucesso
 * B6 : Modo repeatAll DESATIVADO
 * B7 : Motor habilitado (EN = LOW)
 * B8 : Motor desabilitado (EN = HIGH)
 * B9 : Preset EEPROM gravado (Eco de linha)
 * BA : Preset EEPROM lido (Dump de parâmetros)
 * BB : fastAction executado com sucesso
 * BC : fastActionRep executado com sucesso (override rep/dir)
 * C0 : Linha salva com sucesso (Retorna Slot e parâmetros)
 * C1 : [TELEMETRIA] Contagem de slots na SRAM
 * D0 : [TELEMETRIA] Linha ativada (MNN: Motor e Slot)
 * E0 : Erro: Motor já em execução
 * E1 : Erro: Fila dSRAM vazia / Slot SRAM inválido
 * E2 : Erro: Fila SRAM cheia
 * E3 : Erro de sintaxe (Parâmetros obrigatórios ausentes)
 * E4 : Erro: Índice de preset inválido (fora de 0-9)
 * =================================================================================
 */


#include <Arduino.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <string.h>
#include <stdlib.h>
#include <EEPROM.h>
#include <SoftwareSerial.h>

// --- Definição de Pinos (M1 e M2) ---
// Motor 1
#define M1_DIR_PIN PD2
#define M1_PUL_PIN PD3
#define M1_EN_PIN  PD6 // GPIO 6

// Motor 2
#define M2_DIR_PIN PD4
#define M2_PUL_PIN PD5
#define M2_EN_PIN  PD7 // GPIO 7

// --- Estruturas da Fila de Comandos ---
#define MAX_FILA 20
#define MAX_BUFFER_SERIAL 64

// --- EEPROM Fast Action Presets ---
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
// ESTADO GLOBAL DA MÁQUINA
// ==========================================
bool repetir_todas_linhas = false;
bool fila_iniciada = false; // Portão: só permite limpeza após RUN
uint32_t global_pause_ms = 0;

// Variáveis da Serial e Roteamento
enum CommandSource { SRC_USB, SRC_COMMANDER };
CommandSource current_src = SRC_USB;

SoftwareSerial cmdSerial(A0, A1); // RX, TX

char buffer_serial_usb[MAX_BUFFER_SERIAL];
uint8_t indice_buffer_usb = 0;

char buffer_serial_cmd[MAX_BUFFER_SERIAL];
uint8_t indice_buffer_cmd = 0;

// Funções de roteamento de respostas
void enviarRespostaHex(uint8_t codigo) {
    if (current_src == SRC_USB) Serial.println(codigo, HEX);
    else cmdSerial.println(codigo, HEX);
}

void enviarRespostaParam(uint8_t codigo, uint32_t parametro) {
    if (current_src == SRC_USB) {
        Serial.print(codigo, HEX); Serial.print(':'); Serial.println(parametro);
    } else {
        cmdSerial.print(codigo, HEX); cmdSerial.print(':'); cmdSerial.println(parametro);
    }
}

void enviarRespostaComando(uint8_t codigo, uint8_t id_ou_slot, ComandoMotor cmd) {
    if (current_src == SRC_USB) {
        Serial.print(codigo, HEX); Serial.print(':'); Serial.print(id_ou_slot);
        Serial.print(','); Serial.print(0x10, HEX); Serial.print(':'); Serial.print(cmd.step);
        Serial.print(','); Serial.print(0x11, HEX); Serial.print(':'); Serial.print(cmd.vel);
        Serial.print(','); Serial.print(0x12, HEX); Serial.print(':'); Serial.print(cmd.dir);
        Serial.print(','); Serial.print(0x13, HEX); Serial.print(':'); Serial.print(cmd.repeat);
        Serial.print(','); Serial.print(0x14, HEX); Serial.print(':'); Serial.print(cmd.pause_ms);
        Serial.print(','); Serial.print(0x15, HEX); Serial.print(':'); Serial.println(cmd.motor_id);
    } else {
        // Otimizado para LCD / Commander (só envia o código e o ID/Slot)
        cmdSerial.print(codigo, HEX); cmdSerial.print(':'); cmdSerial.println(id_ou_slot);
    }
}

void broadcastHex(uint8_t codigo) {
    Serial.println(codigo, HEX);
    cmdSerial.println(codigo, HEX);
}

void broadcastParam(uint8_t codigo, uint32_t parametro) {
    Serial.print(codigo, HEX); Serial.print(':'); Serial.println(parametro);
    cmdSerial.print(codigo, HEX); cmdSerial.print(':'); cmdSerial.println(parametro);
}

// ==========================================
// ESTADO DO MOTOR 1
// ==========================================
volatile uint32_t m1_passos_restantes = 0;
volatile uint8_t m1_em_movimento = 0;
volatile uint32_t m1_delay_ticks = 0;
volatile uint32_t m1_interval_ticks = 0;

bool m1_executando = false;
uint8_t m1_indice_atual = 0;
uint16_t m1_repeticoes_restantes = 0;
bool m1_comando_infinito = false;

bool m1_em_pausa = false;
uint32_t m1_tempo_inicio_pausa = 0;
uint32_t m1_tempo_pausa_atual = 0;

// ==========================================
// ESTADO DO MOTOR 2
// ==========================================
volatile uint32_t m2_passos_restantes = 0;
volatile uint8_t m2_em_movimento = 0;
volatile uint32_t m2_delay_ticks = 0;
volatile uint32_t m2_interval_ticks = 0;

bool m2_executando = false;
uint8_t m2_indice_atual = 0;
uint16_t m2_repeticoes_restantes = 0;
bool m2_comando_infinito = false;

bool m2_em_pausa = false;
uint32_t m2_tempo_inicio_pausa = 0;
uint32_t m2_tempo_pausa_atual = 0;

// Forward declarations
bool carregarProximoComando(uint8_t motor);
void moverMotor1(uint32_t passos, uint32_t intervalo_us, uint8_t direcao);
void moverMotor2(uint32_t passos, uint32_t intervalo_us, uint8_t direcao);
void inicializarPresetsEEPROM();
void gravarPresetEEPROM(uint8_t idx, ComandoMotor cmd);
ComandoMotor lerPresetEEPROM(uint8_t idx);
void executarFastAction(uint8_t idx);
void executarFastActionRepeat(uint8_t idx, int32_t custom_repeat_signed);

void setup() {
    // Configura Pinos como SAÍDA
    DDRD |= (1 << M1_PUL_PIN) | (1 << M1_DIR_PIN) | (1 << M1_EN_PIN) | 
            (1 << M2_PUL_PIN) | (1 << M2_DIR_PIN) | (1 << M2_EN_PIN);

    // Garante LOW nos pinos de pulso, dir e enable (Ativo baixo no TB6600)
    PORTD &= ~((1 << M1_PUL_PIN) | (1 << M1_DIR_PIN) | (1 << M1_EN_PIN) | 
               (1 << M2_PUL_PIN) | (1 << M2_DIR_PIN) | (1 << M2_EN_PIN));

    cli();
    TCCR1A = 0; // Modo Normal
    TCCR1B = (1 << CS11); // Modo Normal Freerunning com Prescaler = 8
    TCNT1  = 0;
    sei();

    inicializarPresetsEEPROM();

    Serial.begin(9600);
    cmdSerial.begin(9600);
    broadcastHex(0xA0); 
}

// ---------------------------------------------------------
// FUNCIONAMENTO DE BAIXO NÍVEL / TIMER DDS HARDWARE
// ---------------------------------------------------------

/**
 * @brief Configura o hardware e inicia o Timer1 para o Motor 1.
 * @param passos Quantidade de pulsos a serem disparados.
 * @param intervalo_us Tempo entre pulsos em microssegundos.
 * @param direcao Logica física (0 ou 1).
 */
void moverMotor1(uint32_t passos, uint32_t intervalo_us, uint8_t direcao) {
    if (passos == 0 || m1_em_movimento) return;

    if (direcao) PORTD |= (1 << M1_DIR_PIN);
    else         PORTD &= ~(1 << M1_DIR_PIN);

    cli();
    m1_passos_restantes = passos;
    m1_em_movimento = 1;
    m1_interval_ticks = intervalo_us * 2; // PS=8 gera 0.5us p/ tick (1us = 2 ticks)

    // Agendamento DDS do primeiro ciclo de relógio
    m1_delay_ticks = m1_interval_ticks;
    if (m1_delay_ticks > 65535) {
        OCR1A = TCNT1 + 65535;
        m1_delay_ticks -= 65535;
    } else {
        OCR1A = TCNT1 + m1_delay_ticks;
        m1_delay_ticks = 0;
    }
    
    TIFR1 |= (1 << OCF1A);   // Limpa qualquer interrupt acidental agendado no passado
    TIMSK1 |= (1 << OCIE1A); // Habilita interrupt COMPA
    sei();
}

/**
 * @brief Configura o hardware e inicia o Timer1 para o Motor 2 (OCR1B).
 */
void moverMotor2(uint32_t passos, uint32_t intervalo_us, uint8_t direcao) {
    if (passos == 0 || m2_em_movimento) return;

    if (direcao) PORTD |= (1 << M2_DIR_PIN);
    else         PORTD &= ~(1 << M2_DIR_PIN);

    cli();
    m2_passos_restantes = passos;
    m2_em_movimento = 1;
    m2_interval_ticks = intervalo_us * 2;

    m2_delay_ticks = m2_interval_ticks;
    if (m2_delay_ticks > 65535) {
        OCR1B = TCNT1 + 65535;
        m2_delay_ticks -= 65535;
    } else {
        OCR1B = TCNT1 + m2_delay_ticks;
        m2_delay_ticks = 0;
    }
    
    TIFR1 |= (1 << OCF1B);   // Limpa flag antiga
    TIMSK1 |= (1 << OCIE1B); // Habilita interrupt COMPB
    sei();
}

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
             PORTD |= (1 << M1_PUL_PIN); // START PULSE
             _delay_us(3);               // Duração do pulso TB6600
             PORTD &= ~(1 << M1_PUL_PIN); // END PULSE
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
                 TIMSK1 &= ~(1 << OCIE1A); // Encerra ISR se não há mais saltos
                 m1_em_movimento = 0;
             }
        } else {
             TIMSK1 &= ~(1 << OCIE1A);
             m1_em_movimento = 0;
        }
    }
}

ISR(TIMER1_COMPB_vect) {
    if (m2_delay_ticks > 0) {
        if (m2_delay_ticks > 65535) {
             OCR1B += 65535;
             m2_delay_ticks -= 65535;
        } else {
             OCR1B += m2_delay_ticks;
             m2_delay_ticks = 0;
        }
    } else {
        if (m2_passos_restantes > 0) {
             PORTD |= (1 << M2_PUL_PIN);
             _delay_us(3);
             PORTD &= ~(1 << M2_PUL_PIN);
             m2_passos_restantes--;
             
             if (m2_passos_restantes > 0) {
                 m2_delay_ticks = m2_interval_ticks;
                 if (m2_delay_ticks > 65535) {
                     OCR1B += 65535;
                     m2_delay_ticks -= 65535;
                 } else {
                     OCR1B += m2_delay_ticks;
                     m2_delay_ticks = 0;
                 }
             } else {
                 TIMSK1 &= ~(1 << OCIE1B);
                 m2_em_movimento = 0;
             }
        } else {
             TIMSK1 &= ~(1 << OCIE1B);
             m2_em_movimento = 0;
        }
    }
}

// ---------------------------------------------------------
// EEPROM FAST ACTION PRESETS
// ---------------------------------------------------------

void inicializarPresetsEEPROM() {
    if (EEPROM.read(EEPROM_MAGIC_ADDR) == EEPROM_MAGIC_BYTE) return;

    // Valores default para os 5 presets
    ComandoMotor defaults[MAX_PRESETS] = {
        {1600, 400, 1, 1, 0, 1},   // [0] M1, 1600 passos, 400µs, fwd, 1 rep
        {800,  300, 0, 1, 0, 1},   // [1] M1, 800 passos, 300µs, rev, 1 rep
        {3200, 500, 1, 2, 1000, 1},// [2] M1, 3200 passos, 500µs, fwd, 2 reps, 1s pausa
        {1600, 400, 1, 1, 0, 2},   // [3] M2, 1600 passos, 400µs, fwd, 1 rep
        {800,  200, 0, 1, 500, 2}, // [4] M2, 800 passos, 200µs, rev, 1 rep, 500ms pausa
        {0, 0, 0, 0, 0, 1},        // [5] Vazio
        {0, 0, 0, 0, 0, 1},        // [6] Vazio
        {0, 0, 0, 0, 0, 1},        // [7] Vazio
        {0, 0, 0, 0, 0, 1},        // [8] Vazio
        {0, 0, 0, 0, 0, 1}         // [9] Vazio
    };

    for (uint8_t i = 0; i < MAX_PRESETS; i++) {
        gravarPresetEEPROM(i, defaults[i]);
    }
    EEPROM.write(EEPROM_MAGIC_ADDR, EEPROM_MAGIC_BYTE);
}

void gravarPresetEEPROM(uint8_t idx, ComandoMotor cmd) {
    uint16_t addr = EEPROM_PRESETS_ADDR + (idx * sizeof(ComandoMotor));
    EEPROM.put(addr, cmd);
}

ComandoMotor lerPresetEEPROM(uint8_t idx) {
    ComandoMotor cmd;
    uint16_t addr = EEPROM_PRESETS_ADDR + (idx * sizeof(ComandoMotor));
    EEPROM.get(addr, cmd);
    return cmd;
}

void executarFastAction(uint8_t idx) {
    if (m1_executando || m2_executando) {
        enviarRespostaHex(0xE0); // Motor já em execução
        return;
    }

    ComandoMotor cmd = lerPresetEEPROM(idx);

    // Reset da fila e injeta o preset como linha única
    qtd_comandos_na_fila = 0;
    fila_comandos[0] = cmd;
    qtd_comandos_na_fila = 1;

    // Executa imediatamente
    m1_executando = true;
    m2_executando = true;
    m1_indice_atual = 0;
    m2_indice_atual = 0;

    if (!carregarProximoComando(1)) m1_executando = false;
    if (!carregarProximoComando(2)) m2_executando = false;

    if (m1_executando || m2_executando) {
        fila_iniciada = true;
        enviarRespostaComando(0xBB, idx, cmd);
    }
}

void executarFastActionRepeat(uint8_t idx, int32_t custom_repeat_signed) {
    if (m1_executando || m2_executando) {
        enviarRespostaHex(0xE0); // Motor já em execução
        return;
    }

    ComandoMotor cmd = lerPresetEEPROM(idx);
    
    // Suporte para inversão de direção via sinal negativo
    if (custom_repeat_signed < 0) {
        cmd.repeat = (uint16_t)(-custom_repeat_signed);
        cmd.dir = (cmd.dir == 1) ? 0 : 1; // Inverte o que estava na EEPROM
    } else {
        cmd.repeat = (uint16_t)custom_repeat_signed;
    }

    // Reset da fila e injeta o preset como linha única
    qtd_comandos_na_fila = 0;
    fila_comandos[0] = cmd;
    qtd_comandos_na_fila = 1;

    // Executa imediatamente
    m1_executando = true;
    m2_executando = true;
    m1_indice_atual = 0;
    m2_indice_atual = 0;

    if (!carregarProximoComando(1)) m1_executando = false;
    if (!carregarProximoComando(2)) m2_executando = false;

    if (m1_executando || m2_executando) {
        fila_iniciada = true;
        enviarRespostaComando(0xBC, idx, cmd);
    }
}

// ---------------------------------------------------------
// COMUNICAÇÃO SERIAL E FILA (CÓDIGO HEX)
// ---------------------------------------------------------

void constroiEAdicionaComando(ComandoMotor cmd, bool cmd_valido) {
    if (qtd_comandos_na_fila >= MAX_FILA) {
        enviarRespostaHex(0xE2); // Fila Cheia
        return;
    }

    if (cmd_valido && cmd.step > 0 && cmd.vel >= 50) {
        fila_comandos[qtd_comandos_na_fila] = cmd;
        
        enviarRespostaComando(0xC0, qtd_comandos_na_fila, cmd);
        
        qtd_comandos_na_fila++;
        Serial.print(0xC1, HEX); Serial.print(':'); Serial.println(qtd_comandos_na_fila);
    } else {
        enviarRespostaHex(0xE3); // Erro de sintaxe
    }
}

void removerEspacos(char* str) {
    char* d = str;
    do { while (*d == ' ') d++; } while ((*str++ = *d++));
}

void interpretarComando(char* linha) {
    removerEspacos(linha);

    ComandoMotor cmd = {0, 0, 0, 1, 0, 1}; // Default = M1
    bool cmd_valido = false;
    bool eh_parametro = false;

    char* ponteiro_virgula;
    char* par = strtok_r(linha, ",", &ponteiro_virgula);

    while (par != NULL) {
        char* ponteiro_dois_pontos;
        char* chave_str = strtok_r(par, ":", &ponteiro_dois_pontos);
        char* valor_str = strtok_r(NULL, ":", &ponteiro_dois_pontos);

        if (chave_str != NULL) {
            uint8_t chave = (uint8_t)strtol(chave_str, NULL, 16);

            if (valor_str == NULL) {
                if (chave == 0x01) { // run
                    if (qtd_comandos_na_fila > 0 && (!m1_executando && !m2_executando)) {
                        m1_executando = true;
                        m2_executando = true;
                        m1_indice_atual = 0;
                        m2_indice_atual = 0;

                        if (!carregarProximoComando(1)) m1_executando = false;
                        if (!carregarProximoComando(2)) m2_executando = false;

                        if (m1_executando || m2_executando) {
                            fila_iniciada = true;
                            broadcastHex(0xB0);
                        } else {
                            qtd_comandos_na_fila = 0; // Prevenção se a busca falhar
                        }
                    } else if (m1_executando || m2_executando) {
                        enviarRespostaHex(0xE0); // Já rodando
                    } else {
                        enviarRespostaHex(0xE1); // Vazio
                    }
                    return;
                }
                else if (chave == 0x02) { // stop global
                    cli();
                    TIMSK1 &= ~((1 << OCIE1A) | (1 << OCIE1B)); // Mata hardware timers
                    m1_em_movimento = 0;
                    m2_em_movimento = 0;
                    
                    m1_executando = false;
                    m2_executando = false;
                    m1_em_pausa = false;
                    m2_em_pausa = false;
                    
                    repetir_todas_linhas = false;
                    fila_iniciada = false;
                    qtd_comandos_na_fila = 0;
                    sei();
                    
                    broadcastHex(0xB1);
                    broadcastParam(0xC1, 0);
                    return;
                }
                // 0x03 agora requer valor (03:0 ou 03:1), tratado abaixo com os parametros
            } else {
                uint32_t valor = atol(valor_str);
                
                if (chave == 0x03) { // repeatAll booleano (03:1 = ON, 03:0 = OFF)
                    repetir_todas_linhas = (valor == 1);
                    broadcastHex(repetir_todas_linhas ? 0xB4 : 0xB6);
                    return;
                }
                else if (chave == 0x04) {
                    global_pause_ms = valor;
                    broadcastParam(0xB2, global_pause_ms);
                    return;
                }
                else if (chave == 0x10) { cmd.step = valor; cmd_valido = true; eh_parametro = true; }
                else if (chave == 0x11) { cmd.vel = valor; eh_parametro = true; }
                else if (chave == 0x12) { cmd.dir = valor; eh_parametro = true; }
                else if (chave == 0x13) { cmd.repeat = valor; eh_parametro = true; }
                else if (chave == 0x14) { cmd.pause_ms = valor; eh_parametro = true; }
                else if (chave == 0x15) { cmd.motor_id = valor; eh_parametro = true; }
                else if (chave == 0x16) { // enableMotor (EN = LOW, ativo baixo TB6600)
                    if (valor == 1)      { PORTD &= ~(1 << M1_EN_PIN); }
                    else if (valor == 2) { PORTD &= ~(1 << M2_EN_PIN); }
                    else { return; } // Ignora IDs inválidos
                    broadcastParam(0xB7, valor);
                    return;
                }
                else if (chave == 0x17) { // disableMotor (EN = HIGH)
                    if (valor == 1)      { PORTD |= (1 << M1_EN_PIN); }
                    else if (valor == 2) { PORTD |= (1 << M2_EN_PIN); }
                    else { return; } // Ignora IDs inválidos
                    broadcastParam(0xB8, valor);
                    return;
                }
                else if (chave == 0x18) { // fastAction — executa preset EEPROM
                    if (valor > 9) { enviarRespostaHex(0xE4); return; }
                    executarFastAction((uint8_t)valor);
                    return;
                }
                else if (chave == 0x19) { // writePreset — grava preset na EEPROM
                    if (valor > 9) { enviarRespostaHex(0xE4); return; }
                    uint8_t preset_idx = (uint8_t)valor;
                    ComandoMotor preset_cmd = {0, 0, 0, 1, 0, 1};
                    bool preset_valido = false;
                    char* inner_par = strtok_r(NULL, ",", &ponteiro_virgula);
                    while (inner_par != NULL) {
                        char* ip;
                        char* ik = strtok_r(inner_par, ":", &ip);
                        char* iv = strtok_r(NULL, ":", &ip);
                        if (ik && iv) {
                            uint8_t k = (uint8_t)strtol(ik, NULL, 16);
                            uint32_t v = atol(iv);
                            if (k == 0x10) { preset_cmd.step = v; preset_valido = true; }
                            else if (k == 0x11) { preset_cmd.vel = v; }
                            else if (k == 0x12) { preset_cmd.dir = v; }
                            else if (k == 0x13) { preset_cmd.repeat = v; }
                            else if (k == 0x14) { preset_cmd.pause_ms = v; }
                            else if (k == 0x15) { preset_cmd.motor_id = v; }
                        }
                        inner_par = strtok_r(NULL, ",", &ponteiro_virgula);
                    }
                    if (preset_valido && preset_cmd.vel >= 50) {
                        if (preset_cmd.motor_id != 1 && preset_cmd.motor_id != 2) preset_cmd.motor_id = 1;
                        gravarPresetEEPROM(preset_idx, preset_cmd);
                        enviarRespostaComando(0xB9, preset_idx, preset_cmd);
                    } else {
                        enviarRespostaHex(0xE3);
                    }
                    return;
                }
                else if (chave == 0x1A) { // readPreset — lê preset da EEPROM
                    if (valor > 9) { enviarRespostaHex(0xE4); return; }
                    ComandoMotor p = lerPresetEEPROM((uint8_t)valor);
                    enviarRespostaComando(0xBA, (uint8_t)valor, p);
                    return;
                }
                else if (chave == 0x1B) { // fastActionRep — executa preset EEPROM override de repetições (Ex: 1B:slot:rep)
                    char* rep_str = strtok_r(NULL, ":", &ponteiro_dois_pontos);
                    if (valor > 9 || rep_str == NULL) { enviarRespostaHex(0xE4); return; }
                    int32_t custom_rep_signed = atol(rep_str);
                    executarFastActionRepeat((uint8_t)valor, custom_rep_signed);
                    return;
                }
                else if (chave == 0x1C) { // saveToEEPROM - copia slot SRAM para EEPROM (Ex: 1C:eeprom_slot:sram_slot)
                    char* sram_str = strtok_r(NULL, ":", &ponteiro_dois_pontos);
                    if (valor > 9) { enviarRespostaHex(0xE4); return; } // Verifica índice da EEPROM
                    if (sram_str == NULL) { enviarRespostaHex(0xE3); return; } // Faltou parâmetro
                    
                    uint8_t sram_slot = (uint8_t)atol(sram_str);
                    if (sram_slot >= qtd_comandos_na_fila) {
                        enviarRespostaHex(0xE1); // Slot SRAM não preenchido ou vazio
                        return;
                    }
                    
                    ComandoMotor preset_cmd = fila_comandos[sram_slot];
                    gravarPresetEEPROM((uint8_t)valor, preset_cmd);
                    
                    // Retorna confirmação (mesmo pacote do B9)
                    enviarRespostaComando(0xB9, (uint8_t)valor, preset_cmd);
                    return;
                }
            }
        }
        par = strtok_r(NULL, ",", &ponteiro_virgula);
    }

    if (eh_parametro) {
        // Assegura robustez do ID apontado
        if (cmd.motor_id != 1 && cmd.motor_id != 2) cmd.motor_id = 1;
        constroiEAdicionaComando(cmd, cmd_valido);
    }
}

void processarSerial() {
    while (Serial.available() > 0) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            if (indice_buffer_usb > 0) {
                buffer_serial_usb[indice_buffer_usb] = '\0';
                current_src = SRC_USB;
                interpretarComando(buffer_serial_usb);
                indice_buffer_usb = 0;
            }
        } else if (indice_buffer_usb < MAX_BUFFER_SERIAL - 1) {
            buffer_serial_usb[indice_buffer_usb++] = c;
        }
    }

    while (cmdSerial.available() > 0) {
        char c = cmdSerial.read();
        if (c == '\n' || c == '\r') {
            if (indice_buffer_cmd > 0) {
                buffer_serial_cmd[indice_buffer_cmd] = '\0';
                current_src = SRC_COMMANDER;
                interpretarComando(buffer_serial_cmd);
                indice_buffer_cmd = 0;
            }
        } else if (indice_buffer_cmd < MAX_BUFFER_SERIAL - 1) {
            buffer_serial_cmd[indice_buffer_cmd++] = c;
        }
    }
}

// ---------------------------------------------------------
// MÁQUINA DE ESTADOS - BUSCA DA FILA
// ---------------------------------------------------------

bool carregarProximoComando(uint8_t motor) {
    uint8_t* indice_ptr = (motor == 1) ? &m1_indice_atual : &m2_indice_atual;
    bool* infinito_ptr = (motor == 1) ? &m1_comando_infinito : &m2_comando_infinito;
    uint16_t* rep_ptr = (motor == 1) ? &m1_repeticoes_restantes : &m2_repeticoes_restantes;
    
    // Procura por toda a fila local a partir do offset atual
    while ((int)*indice_ptr < (int)qtd_comandos_na_fila) {
        if (fila_comandos[*indice_ptr].motor_id == motor) {
            ComandoMotor cmd = fila_comandos[*indice_ptr];
            *infinito_ptr = (cmd.repeat == 0);
            *rep_ptr = cmd.repeat;
            
            // Telemetria: id customizado p/ diferenciar slots (Ex: M1 linha 2 -> D0:102, M2 linha 3 -> D0:203)
            Serial.print(0xD0, HEX); Serial.print(':'); Serial.println((motor * 100) + *indice_ptr); 
            
            if (motor == 1) moverMotor1(cmd.step, cmd.vel, cmd.dir);
            else            moverMotor2(cmd.step, cmd.vel, cmd.dir);
            
            return true;
        }
        (*indice_ptr)++;
    }
    return false; // Fim da fila deste motor
}

void avancarFilaM1() {
    if (m1_comando_infinito) {
        carregarProximoComando(1); 
        return;
    }

    if (m1_repeticoes_restantes > 0) m1_repeticoes_restantes--;
    
    if (m1_repeticoes_restantes > 0) {
        ComandoMotor cmd = fila_comandos[m1_indice_atual];
        moverMotor1(cmd.step, cmd.vel, cmd.dir);
    } else {
        m1_indice_atual++; 
        if (!carregarProximoComando(1)) {
            if (repetir_todas_linhas) {
                m1_indice_atual = 0;
                if (!carregarProximoComando(1)) m1_executando = false; 
            } else {
                m1_executando = false;
            }
        }
    }
}

void avancarFilaM2() {
    if (m2_comando_infinito) {
        carregarProximoComando(2); 
        return;
    }

    if (m2_repeticoes_restantes > 0) m2_repeticoes_restantes--;
    
    if (m2_repeticoes_restantes > 0) {
        ComandoMotor cmd = fila_comandos[m2_indice_atual];
        moverMotor2(cmd.step, cmd.vel, cmd.dir);
    } else {
        m2_indice_atual++; 
        if (!carregarProximoComando(2)) {
            if (repetir_todas_linhas) {
                m2_indice_atual = 0;
                if (!carregarProximoComando(2)) m2_executando = false; 
            } else {
                m2_executando = false;
            }
        }
    }
}

void maquinaDeEstadosMotor() {
    // Pipeline M1
    if (m1_executando && !m1_em_movimento) {
        if (m1_em_pausa) {
            if (millis() - m1_tempo_inicio_pausa >= m1_tempo_pausa_atual) {
                m1_em_pausa = false;
                avancarFilaM1(); 
            }
        } else {
            uint32_t pausa = (fila_comandos[m1_indice_atual].pause_ms > 0) ? 
                              fila_comandos[m1_indice_atual].pause_ms : global_pause_ms;
            if (pausa > 0) {
                m1_em_pausa = true;
                m1_tempo_inicio_pausa = millis();
                m1_tempo_pausa_atual = pausa;
            } else {
                avancarFilaM1();
            }
        }
    }
    
    // Pipeline M2
    if (m2_executando && !m2_em_movimento) {
        if (m2_em_pausa) {
            if (millis() - m2_tempo_inicio_pausa >= m2_tempo_pausa_atual) {
                m2_em_pausa = false;
                avancarFilaM2();
            }
        } else {
            uint32_t pausa = (fila_comandos[m2_indice_atual].pause_ms > 0) ? 
                              fila_comandos[m2_indice_atual].pause_ms : global_pause_ms;
            if (pausa > 0) {
                m2_em_pausa = true;
                m2_tempo_inicio_pausa = millis();
                m2_tempo_pausa_atual = pausa;
            } else {
                avancarFilaM2();
            }
        }
    }
    
    // Tratamento Global de Encerramento (Limpeza da Fila)
    // Só dispara se a execução foi de fato iniciada pelo comando RUN (01)
    if (fila_iniciada && !m1_executando && !m2_executando && qtd_comandos_na_fila > 0) {
        qtd_comandos_na_fila = 0;
        fila_iniciada = false;
        broadcastHex(0xB5);
    }
}

void loop() {
    processarSerial();
    maquinaDeEstadosMotor();
}