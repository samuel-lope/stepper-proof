/*
 * =================================================================================
 * Controle de Motor de Passo TB6600 usando Timer1 de Hardware (ATmega328P)
 * =================================================================================
 *
 * ÍNDICE DE CÓDIGOS HEXADECIMAIS (8-bits)
 * Para economizar memória FLASH e SRAM, todas as strings de comunicação
 * foram substituídas por códigos HEX de 2 dígitos.
 *
 * --- COMANDOS E PARÂMETROS (ENVIAR PARA O ARDUINO) ---
 * 01 : run       (Inicia a execução da fila)
 * 02 : stop      (Parada de emergência e limpa fila)
 * 03 : repeatAll (Ativa loop infinito da fila inteira)
 * 04 : pause     (Pausa global. Ex: 04:1000)
 * 10 : step      (Obrigatório - Quantidade de passos)
 * 11 : vel       (Obrigatório - Intervalo em microssegundos)
 * 12 : dir       (Opcional - Direção: 0 ou 1)
 * 13 : repeat    (Opcional - Repetições da linha. 0 = infinito)
 * 14 : pause     (Opcional - Pausa em ms após a linha)
 * * Exemplo de envio via Serial (1600 passos, vel 500, dir 1): 
 * 10:1600,11:500,12:1
 * * --- ALERTAS E RESPOSTAS (RECEBIDOS DO ARDUINO) ---
 * A0 : Sistema Inicializado com Sucesso
 * B0 : Iniciando execucao da fila
 * B1 : Motor PARADO e Fila limpa
 * B2 : Pausa global definida (Retorna o valor junto)
 * B4 : Modo repeatAll ATIVADO
 * B5 : Fila executada com sucesso
 * C0 : Linha salva com sucesso (Retorna índice e parâmetros)
 * C1 : [TELEMETRIA] Status de Slot na Fila (uso RAM)
 * D0 : [TELEMETRIA] Linha ativada pelos Optoacopladores do Motor 
 * E0 : Erro: Motor ja esta em execucao
 * E1 : Erro: Fila vazia
 * E2 : Erro: Fila cheia
 * E3 : Erro de sintaxe (Falta '10' ou '11')
 * =================================================================================
 */

#include <Arduino.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <string.h>
#include <stdlib.h>

// Definição de Pinos - Usando os registradores da Porta D e B
#define DIR_PIN PD2
#define PUL_PIN PD3
#define EN_PIN PB0

// --- Variáveis de Controle do Motor (Voláteis para a ISR) ---
volatile uint32_t passos_restantes = 0;
volatile uint8_t motor_em_movimento = 0;

// --- Estruturas e Variáveis da Fila de Comandos ---
#define MAX_FILA 20
#define MAX_BUFFER_SERIAL 64

typedef struct {
    uint32_t step;
    uint32_t vel;
    uint8_t dir;
    uint16_t repeat;
    uint32_t pause_ms;
} ComandoMotor;

ComandoMotor fila_comandos[MAX_FILA];
uint8_t qtd_comandos_na_fila = 0;

// Variáveis de estado de execução
bool executando_fila = false;
uint8_t indice_comando_atual = 0;
uint16_t repeticoes_restantes_atual = 0;
bool comando_infinito = false;
bool repetir_todas_linhas = false; // Flag para o comando repeatAll

// Variáveis de Pausa
uint32_t global_pause_ms = 0;
bool em_pausa = false;
uint32_t tempo_inicio_pausa = 0;
uint32_t tempo_pausa_atual = 0;

// Variáveis da Serial
char buffer_serial[MAX_BUFFER_SERIAL];
uint8_t indice_buffer = 0;

void carregarComandoNaMaquina(uint8_t indice); // Forward declaration

void setup() {
    // Configura Pinos como SAÍDA
    DDRD |= (1 << PUL_PIN) | (1 << DIR_PIN);
    DDRB |= (1 << EN_PIN);

    // Garante que PUL e DIR iniciem em BAIXO, Enable em BAIXO (Ativo)
    PORTD &= ~((1 << PUL_PIN) | (1 << DIR_PIN));
    PORTB &= ~(1 << EN_PIN);

    // Desabilita interrupções globais temporariamente
    cli();
    TCCR1A = 0;
    TCCR1B = 0;
    TCNT1  = 0;
    sei();

    Serial.begin(115200);
    // Imprime codigo A0 em Hexadecimal no terminal indicando inicialização
    Serial.println(0xA0, HEX); 
}

/**
 * Função para disparar o hardware de movimentação do motor
 */
void moverMotor(uint32_t passos, uint32_t intervalo_us, uint8_t direcao) {
    if (passos == 0 || motor_em_movimento) return;

    // Define a direção fisicamente
    if (direcao) PORTD |= (1 << DIR_PIN);
    else         PORTD &= ~(1 << DIR_PIN);

    cli();
    passos_restantes = passos;
    motor_em_movimento = 1;
    TCCR1A = 0;
    TCNT1 = 0;
    TIFR1 |= (1 << OCF1A); // Limpa flag de interrupcao pendente

    if (intervalo_us <= 32767) {
        OCR1A = (intervalo_us * 2) - 1;
        TIMSK1 |= (1 << OCIE1A);
        TCCR1B = (1 << WGM12) | (1 << CS11); // Prescaler 8
    } else {
        OCR1A = (intervalo_us / 4) - 1;
        TIMSK1 |= (1 << OCIE1A);
        TCCR1B = (1 << WGM12) | (1 << CS11) | (1 << CS10); // Prescaler 64
    }
    sei();
}

/**
 * ISR: Disparada pelo Hardware quando o intervalo de tempo é atingido
 */
ISR(TIMER1_COMPA_vect) {
    if (passos_restantes > 0) {
        PORTD |= (1 << PUL_PIN); // HIGH
        _delay_us(3);            // Largura do pulso requerida pelo TB6600
        PORTD &= ~(1 << PUL_PIN); // LOW
        passos_restantes--;
    } else {
        // Desliga o timer
        TCCR1B &= ~((1 << CS12) | (1 << CS11) | (1 << CS10));
        motor_em_movimento = 0;
    }
}

/**
 * Remove espaços em branco de uma string para facilitar o parsing
 */
void removerEspacos(char* str) {
    char* d = str;
    do {
        while (*d == ' ') d++;
    } while ((*str++ = *d++));
}

/**
 * Interpreta uma linha serial baseada estritamente em códigos Hexadecimais
 */
void interpretarComando(char* linha) {
    removerEspacos(linha);

    ComandoMotor cmd = {0, 0, 0, 1, 0};
    bool cmd_valido = false;
    bool eh_parametro = false;

    char* ponteiro_virgula;
    char* par = strtok_r(linha, ",", &ponteiro_virgula);

    while (par != NULL) {
        char* ponteiro_dois_pontos;
        char* chave_str = strtok_r(par, ":", &ponteiro_dois_pontos);
        char* valor_str = strtok_r(NULL, ":", &ponteiro_dois_pontos);

        if (chave_str != NULL) {
            // Converte a string hexadecimal (ex: "10") para o byte numérico (0x10)
            uint8_t chave = (uint8_t)strtol(chave_str, NULL, 16);

            if (valor_str == NULL) {
                // COMANDOS DE SISTEMA (Sem valores associados)
                if (chave == 0x01) { // run
                    if (qtd_comandos_na_fila > 0 && !executando_fila) {
                        executando_fila = true;
                        indice_comando_atual = 0;
                        carregarComandoNaMaquina(0);
                        Serial.println(0xB0, HEX);
                    } else if (executando_fila) {
                        Serial.println(0xE0, HEX);
                    } else {
                        Serial.println(0xE1, HEX);
                    }
                    return;
                }
                else if (chave == 0x02) { // stop
                    cli();
                    TCCR1B &= ~((1 << CS12) | (1 << CS11) | (1 << CS10)); // Desliga clock
                    motor_em_movimento = 0;
                    executando_fila = false;
                    em_pausa = false;
                    repetir_todas_linhas = false;
                    qtd_comandos_na_fila = 0;
                    sei();
                    Serial.println(0xB1, HEX);
                    Serial.print(0xC1, HEX); Serial.print(':'); Serial.println(0); // Emite telemetria
                    return;
                }
                else if (chave == 0x03) { // repeatAll
                    repetir_todas_linhas = true;
                    Serial.println(0xB4, HEX);
                    return;
                }
            } else {
                // PARÂMETROS E CONFIGURAÇÕES
                uint32_t valor = atol(valor_str);
                
                if (chave == 0x04) { // pause global
                    global_pause_ms = valor;
                    Serial.print(0xB2, HEX); Serial.print(':'); Serial.println(global_pause_ms);
                    return;
                }
                else if (chave == 0x10) { cmd.step = valor; cmd_valido = true; eh_parametro = true; }
                else if (chave == 0x11) { cmd.vel = valor; eh_parametro = true; }
                else if (chave == 0x12) { cmd.dir = valor; eh_parametro = true; }
                else if (chave == 0x13) { cmd.repeat = valor; eh_parametro = true; }
                else if (chave == 0x14) { cmd.pause_ms = valor; eh_parametro = true; }
            }
        }
        par = strtok_r(NULL, ",", &ponteiro_virgula);
    }

    // Se construiu um comando de motor, salva na fila
    if (eh_parametro) {
        if (qtd_comandos_na_fila >= MAX_FILA) {
            Serial.println(0xE2, HEX);
            return;
        }

        if (cmd_valido && cmd.step > 0 && cmd.vel >= 50) {
            fila_comandos[qtd_comandos_na_fila] = cmd;
            
            // Impressão usando apenas caracteres simples ':' e ',' (1 byte) em vez de strings literais
            Serial.print(0xC0, HEX); Serial.print(':'); Serial.print(qtd_comandos_na_fila);
            Serial.print(','); Serial.print(0x10, HEX); Serial.print(':'); Serial.print(cmd.step);
            Serial.print(','); Serial.print(0x11, HEX); Serial.print(':'); Serial.print(cmd.vel);
            Serial.print(','); Serial.print(0x12, HEX); Serial.print(':'); Serial.print(cmd.dir);
            Serial.print(','); Serial.print(0x13, HEX); Serial.print(':'); Serial.print(cmd.repeat);
            Serial.print(','); Serial.print(0x14, HEX); Serial.print(':'); Serial.println(cmd.pause_ms);
            
            qtd_comandos_na_fila++;
            Serial.print(0xC1, HEX); Serial.print(':'); Serial.println(qtd_comandos_na_fila); // Emite telemetria de Uso RAM
        } else {
            Serial.println(0xE3, HEX);
        }
    }
}

/**
 * Lê caracteres da porta serial um por um (não-bloqueante)
 */
void processarSerial() {
    while (Serial.available() > 0) {
        char c = Serial.read();
        
        // Fim de linha (ENTER)
        if (c == '\n' || c == '\r') {
            if (indice_buffer > 0) {
                buffer_serial[indice_buffer] = '\0'; // Finaliza a string
                interpretarComando(buffer_serial);
                indice_buffer = 0; // Reseta o buffer
            }
        } 
        else if (indice_buffer < MAX_BUFFER_SERIAL - 1) {
            buffer_serial[indice_buffer++] = c; // Armazena caracter
        }
    }
}

/**
 * Prepara o estado para executar o comando atual da fila
 */
void carregarComandoNaMaquina(uint8_t indice) {
    ComandoMotor cmd = fila_comandos[indice];
    comando_infinito = (cmd.repeat == 0);
    repeticoes_restantes_atual = cmd.repeat;
    
    // Atualiza telemetria contínua do Slot Ativo para o Frontend
    Serial.print(0xD0, HEX); Serial.print(':'); Serial.println(indice);

    // Inicia o disparo físico
    moverMotor(cmd.step, cmd.vel, cmd.dir);
}

/**
 * Avança para a próxima repetição ou próxima linha da fila
 */
void avancarFila() {
    if (comando_infinito) {
        // Re-inicia o mesmo comando imediatamente
        carregarComandoNaMaquina(indice_comando_atual);
    } else {
        repeticoes_restantes_atual--; // Conta 1 ciclo finalizado
        
        if (repeticoes_restantes_atual > 0) {
            // Tem repetições da mesma linha pendentes
            moverMotor(fila_comandos[indice_comando_atual].step, 
                       fila_comandos[indice_comando_atual].vel, 
                       fila_comandos[indice_comando_atual].dir);
        } else {
            // Terminou as repetições desta linha, avança
            indice_comando_atual++;
            
            if (indice_comando_atual >= qtd_comandos_na_fila) {
                // Fila inteira concluída
                if (repetir_todas_linhas) {
                    // Recomeça a fila do zero sem parar
                    indice_comando_atual = 0;
                    carregarComandoNaMaquina(indice_comando_atual);
                } else {
                    executando_fila = false;
                    qtd_comandos_na_fila = 0; // Limpa a fila
                    Serial.println(0xB5, HEX);
                }
            } else {
                // Carrega e dispara próxima linha (line[N])
                carregarComandoNaMaquina(indice_comando_atual);
            }
        }
    }
}

/**
 * Gerencia a execução das linhas, pausas e repetições sem travar o processador
 */
void maquinaDeEstadosMotor() {
    if (executando_fila && !motor_em_movimento) {
        // O hardware do timer reportou que terminou os passos desta etapa
        
        if (em_pausa) {
            // Verifica se o tempo de pausa já passou (usando millis() que é não-bloqueante)
            if (millis() - tempo_inicio_pausa >= tempo_pausa_atual) {
                em_pausa = false;
                avancarFila(); // Pausa finalizada, avança para o próximo passo
            }
        } else {
            // Acabamos de terminar um movimento. Vamos checar se precisamos pausar
            uint32_t pausa_desejada = fila_comandos[indice_comando_atual].pause_ms > 0 ? 
                                      fila_comandos[indice_comando_atual].pause_ms : global_pause_ms;
            
            if (pausa_desejada > 0) {
                em_pausa = true;
                tempo_inicio_pausa = millis();
                tempo_pausa_atual = pausa_desejada;
                // Emite pingo de telemetria mostrando a pausa em MS
                Serial.print(0xB3, HEX); Serial.print(':'); Serial.println(tempo_pausa_atual);
            } else {
                // Sem pausa necessária, avança imediatamente
                avancarFila();
            }
        }
    }
}

void loop() {
    processarSerial();      // Ouve ordens sem bloquear
    maquinaDeEstadosMotor(); // Checa se precisa engatilhar a próxima ação
}