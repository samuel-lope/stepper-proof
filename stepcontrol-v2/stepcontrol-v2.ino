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
#include <util/atomic.h>
#include <util/delay.h>

// ==========================================

// ==========================================
// BROADCAST MACROS (Dual Serial)
// ==========================================
#define broadcast(x) { Serial.print(x); Serial1.print(x); }
#define broadcastln(x) { Serial.println(x); Serial1.println(x); }
#define broadcast2(x, y) { Serial.print(x, y); Serial1.print(x, y); }
#define broadcastln2(x, y) { Serial.println(x, y); Serial1.println(x, y); }

// DEFINIÇÕES DE PINOS DOS MOTORES
// ==========================================
// Motor 1 (PORTA)
#define M1_DIR_PIN PA0 // Pino 22
#define M1_PUL_PIN PA2 // Pino 24
#define M1_EN_PIN PA4  // Pino 26

// Motor 2 (PORTC)
#define M2_DIR_PIN PC7 // Pino 30
#define M2_PUL_PIN PC5 // Pino 32
#define M2_EN_PIN PC3  // Pino 34

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
bool m1_comando_infinito = false;
bool m1_em_pausa = false;
uint32_t m1_tempo_inicio_pausa = 0;
uint32_t m1_tempo_pausa_atual = 0;

// --- Estado Motor 2 ---
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

// ==========================================
// BUFFER SERIAL
// ==========================================
#define MAX_INPUT_LEN 64
char serialBuffer[MAX_INPUT_LEN + 1];
uint8_t serialBufferIdx = 0;

char serialBuffer1[MAX_INPUT_LEN + 1];
uint8_t serialBufferIdx1 = 0;

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
void avancarFilaM1();
void avancarFilaM2();

// ==========================================
// SETUP
// ==========================================
void setup() {
  Serial.begin(9600);
  Serial1.begin(9600); // Porta oficial do StepCommander (RX1=19, TX1=18)

  // 1. Configura Pinos dos Motores (PORTA e PORTC) como SAÍDA
  DDRA |= (1 << M1_PUL_PIN) | (1 << M1_DIR_PIN) | (1 << M1_EN_PIN);
  DDRC |= (1 << M2_PUL_PIN) | (1 << M2_DIR_PIN) | (1 << M2_EN_PIN);

  // Ativa pull-up no ENABLE (Motor Desabilitado inicialmente)
  PORTA |= (1 << M1_EN_PIN);
  PORTC |= (1 << M2_EN_PIN);

  // Garante que PUL e DIR iniciam em LOW
  PORTA &= ~((1 << M1_PUL_PIN) | (1 << M1_DIR_PIN));
  PORTC &= ~((1 << M2_PUL_PIN) | (1 << M2_DIR_PIN));

  cli();
  // 2. Configura Timer 1 (Motor 1) - Normal Mode, Prescaler 8
  TCCR1A = 0;
  TCCR1B = (1 << CS11);
  TCNT1 = 0;

  // 3. Configura Timer 3 (Motor 2) - Normal Mode, Prescaler 8
  TCCR3A = 0;
  TCCR3B = (1 << CS31);
  TCNT3 = 0;
  sei();

  // === TESTE DE DIAGNÓSTICO HARDWARE ===
  // Pulsa manualmente 200 passos em cada motor via GPIO direto (sem timer).
  // Se o motor girar aqui, o problema é no timer/ISR.
  // Se NÃO girar, o problema é fiação/pino errado.
  Serial.println(F("DIAG: Teste M1 pino 24 (PA2)..."));
  PORTA &= ~(1 << M1_EN_PIN); // Enable M1
  for (uint16_t i = 0; i < 200; i++) {
    PORTA |= (1 << M1_PUL_PIN);
    _delay_us(10);
    PORTA &= ~(1 << M1_PUL_PIN);
    _delay_us(490); // ~500us por passo = velocidade segura
  }
  PORTA |= (1 << M1_EN_PIN); // Disable M1
  Serial.println(F("DIAG: M1 done."));

  delay(500);

  Serial.println(F("DIAG: Teste M2 pino 32 (PC5)..."));
  PORTC &= ~(1 << M2_EN_PIN); // Enable M2
  for (uint16_t i = 0; i < 200; i++) {
    PORTC |= (1 << M2_PUL_PIN);
    _delay_us(10);
    PORTC &= ~(1 << M2_PUL_PIN);
    _delay_us(490);
  }
  PORTC |= (1 << M2_EN_PIN); // Disable M2
  Serial.println(F("DIAG: M2 done."));
  Serial.println(F("DIAG: Se os motores NAO giraram, verifique fiacao."));
  // === FIM DO DIAGNÓSTICO ===

  inicializarPresetsEEPROM();
  diagnosticoEEPROM();

  broadcastln(F("A0: Inicializado v3.0"));
}

// ==========================================
// ROTINAS DE INTERRUPÇÃO (ISRs)
// ==========================================
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
      PORTA |= (1 << M1_PUL_PIN);
      _delay_us(3);
      PORTA &= ~(1 << M1_PUL_PIN);
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
        TIMSK1 &= ~(1 << OCIE1A);
        m1_em_movimento = 0;
      }
    } else {
      TIMSK1 &= ~(1 << OCIE1A);
      m1_em_movimento = 0;
    }
  }
}

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
      PORTC |= (1 << M2_PUL_PIN);
      _delay_us(3);
      PORTC &= ~(1 << M2_PUL_PIN);
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
// EEPROM FAST ACTION PRESETS
// ==========================================
void inicializarPresetsEEPROM() {
  if (EEPROM.read(EEPROM_MAGIC_ADDR) == EEPROM_MAGIC_BYTE)
    return;

  ComandoMotor defaults[MAX_PRESETS] = {
      {1600, 400, 1, 1, 0, 1},    {800, 300, 0, 1, 0, 1},
      {3200, 500, 1, 2, 1000, 1}, {1600, 400, 1, 1, 0, 2},
      {800, 200, 0, 1, 500, 2},   {0, 0, 0, 0, 0, 1},
      {0, 0, 0, 0, 0, 1},         {0, 0, 0, 0, 0, 1},
      {0, 0, 0, 0, 0, 1},         {0, 0, 0, 0, 0, 1}};
  for (uint8_t i = 0; i < MAX_PRESETS; i++)
    gravarPresetEEPROM(i, defaults[i]);
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
    broadcastln(F("E0: Em Execucao"));
    return;
  }
  if (idx >= MAX_PRESETS) {
    broadcastln(F("E4: Preset Inv."));
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

  if (m1_executando || m2_executando) {
    fila_iniciada = true;
    broadcast(F("BB: FastAct "));
    broadcastln(idx);
  }
}

void executarFastActionRepeat(uint8_t idx, int32_t custom_repeat_signed) {
  if (m1_executando || m2_executando) {
    broadcastln(F("E0: Em Execucao"));
    return;
  }
  if (idx >= MAX_PRESETS) {
    broadcastln(F("E4: Preset Inv."));
    return;
  }

  ComandoMotor cmd = lerPresetEEPROM(idx);
  if (custom_repeat_signed < 0) {
    cmd.repeat = (uint16_t)(-custom_repeat_signed);
    cmd.dir = (cmd.dir == 1) ? 0 : 1;
  } else {
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

  if (m1_executando || m2_executando) {
    fila_iniciada = true;
    broadcast(F("BC: FastActRep "));
    broadcastln(idx);
  }
}

// ==========================================
// CONTROLE DE HARDWARE (MOTORES)
// ==========================================
void moverMotor1(uint32_t passos, uint32_t intervalo_us, uint8_t direcao) {
  if (passos == 0 || m1_em_movimento)
    return;

  if (direcao)
    PORTA |= (1 << M1_DIR_PIN);
  else
    PORTA &= ~(1 << M1_DIR_PIN);

  PORTA &= ~(1 << M1_EN_PIN); // Habilita o driver

  cli();
  m1_passos_restantes = passos;
  m1_em_movimento = 1;
  m1_interval_ticks = intervalo_us * 2;

  m1_delay_ticks = m1_interval_ticks;
  if (m1_delay_ticks > 65535) {
    OCR1A = TCNT1 + 65535;
    m1_delay_ticks -= 65535;
  } else {
    OCR1A = TCNT1 + m1_delay_ticks;
    m1_delay_ticks = 0;
  }

  TIFR1 = (1 << OCF1A);
  TIMSK1 |= (1 << OCIE1A);
  sei();
}

void moverMotor2(uint32_t passos, uint32_t intervalo_us, uint8_t direcao) {
  if (passos == 0 || m2_em_movimento)
    return;

  if (direcao)
    PORTC |= (1 << M2_DIR_PIN);
  else
    PORTC &= ~(1 << M2_DIR_PIN);

  PORTC &= ~(1 << M2_EN_PIN); // Habilita o driver

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

  TIFR3 = (1 << OCF3A);
  TIMSK3 |= (1 << OCIE3A);
  sei();
}

// ==========================================
// LÓGICA DE FILA E PAUSAS
// ==========================================
bool carregarProximoComando(uint8_t motor) {
  if (qtd_comandos_na_fila == 0)
    return false;

  if (motor == 1) {
    while (m1_indice_atual < qtd_comandos_na_fila &&
           fila_comandos[m1_indice_atual].motor_id != 1) {
      m1_indice_atual++;
    }
    if (m1_indice_atual >= qtd_comandos_na_fila)
      return false;

    ComandoMotor cmd = fila_comandos[m1_indice_atual];
    m1_comando_infinito = (cmd.repeat == 0);
    m1_repeticoes_restantes = cmd.repeat;

    broadcast(F("D0: M1 L"));
    broadcastln(m1_indice_atual);
    moverMotor1(cmd.step, cmd.vel, cmd.dir);
    return true;
  } else {
    while (m2_indice_atual < qtd_comandos_na_fila &&
           fila_comandos[m2_indice_atual].motor_id != 2) {
      m2_indice_atual++;
    }
    if (m2_indice_atual >= qtd_comandos_na_fila)
      return false;

    ComandoMotor cmd = fila_comandos[m2_indice_atual];
    m2_comando_infinito = (cmd.repeat == 0);
    m2_repeticoes_restantes = cmd.repeat;

    broadcast(F("D0: M2 L"));
    broadcastln(m2_indice_atual);
    moverMotor2(cmd.step, cmd.vel, cmd.dir);
    return true;
  }
}

void avancarFilaM1() {
  if (m1_comando_infinito) {
    carregarProximoComando(1);
    return;
  }
  if (m1_repeticoes_restantes > 0)
    m1_repeticoes_restantes--;
  if (m1_repeticoes_restantes > 0) {
    ComandoMotor cmd = fila_comandos[m1_indice_atual];
    moverMotor1(cmd.step, cmd.vel, cmd.dir);
  } else {
    m1_indice_atual++;
    if (!carregarProximoComando(1)) {
      if (repetir_todas_linhas) {
        m1_indice_atual = 0;
        if (!carregarProximoComando(1))
          m1_executando = false;
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
  if (m2_repeticoes_restantes > 0)
    m2_repeticoes_restantes--;
  if (m2_repeticoes_restantes > 0) {
    ComandoMotor cmd = fila_comandos[m2_indice_atual];
    moverMotor2(cmd.step, cmd.vel, cmd.dir);
  } else {
    m2_indice_atual++;
    if (!carregarProximoComando(2)) {
      if (repetir_todas_linhas) {
        m2_indice_atual = 0;
        if (!carregarProximoComando(2))
          m2_executando = false;
      } else {
        m2_executando = false;
      }
    }
  }
}

void manageSteppers() {
  // Pipeline M1
  if (m1_executando && !m1_em_movimento) {
    if (m1_em_pausa) {
      if (millis() - m1_tempo_inicio_pausa >= m1_tempo_pausa_atual) {
        m1_em_pausa = false;
        avancarFilaM1();
      }
    } else {
      uint32_t pausa = (fila_comandos[m1_indice_atual].pause_ms > 0)
                           ? fila_comandos[m1_indice_atual].pause_ms
                           : global_pause_ms;
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
      uint32_t pausa = (fila_comandos[m2_indice_atual].pause_ms > 0)
                           ? fila_comandos[m2_indice_atual].pause_ms
                           : global_pause_ms;
      if (pausa > 0) {
        m2_em_pausa = true;
        m2_tempo_inicio_pausa = millis();
        m2_tempo_pausa_atual = pausa;
      } else {
        avancarFilaM2();
      }
    }
  }

  // Fim de Fila
  if (fila_iniciada && !m1_executando && !m2_executando &&
      qtd_comandos_na_fila > 0) {
    qtd_comandos_na_fila = 0;
    fila_iniciada = false;
    broadcastln(F("B5: Fila Executada"));
  }
}

// ==========================================
// PARSER (Interpretador H8P Completo)
// ==========================================
void interpretarComando(char *linha) {
  // Remove espaços em branco in-place
  char *src = linha;
  char *dst = linha;
  while (*src) {
    if (*src != ' ') {
      *dst++ = *src;
    }
    src++;
  }
  *dst = '\0';

  if (strlen(linha) == 0)
    return;

  if (strcmp(linha, "CLEAR") == 0) {
    qtd_comandos_na_fila = 0;
    broadcastln(F("C1:0"));
    return;
  }

  ComandoMotor cmd = {0, 0, 0, 1, 0, 1};
  bool cmd_valido = false;
  bool save_to_eeprom = false;
  uint8_t eeprom_slot = 0;

  char *ponteiro_virgula;
  char *par = strtok_r(linha, ",", &ponteiro_virgula);

  while (par != NULL) {
    char *ponteiro_dois_pontos;
    char *chave_str = strtok_r(par, ":", &ponteiro_dois_pontos);
    char *valor_str = strtok_r(NULL, ":", &ponteiro_dois_pontos);
    char *terceiro_str = strtok_r(NULL, ":", &ponteiro_dois_pontos);

    if (chave_str != NULL) {
      uint8_t chave = (uint8_t)strtol(chave_str, NULL, 16);

      if (valor_str == NULL) {
        if (chave == 0x01) { // 01: run
          if (qtd_comandos_na_fila > 0 && (!m1_executando && !m2_executando)) {
            m1_executando = true;
            m2_executando = true;
            m1_indice_atual = 0;
            m2_indice_atual = 0;
            if (!carregarProximoComando(1))
              m1_executando = false;
            if (!carregarProximoComando(2))
              m2_executando = false;
            if (m1_executando || m2_executando) {
              fila_iniciada = true;
              broadcastln(F("B0: Executando"));
            }
          } else if (m1_executando || m2_executando) {
            broadcastln(F("E0: Em Execucao"));
          } else {
            broadcastln(F("E1: Fila Vazia"));
          }
          return;
        } else if (chave == 0x02) { // 02: stop global
          cli();
          m1_repeticoes_restantes = 0;
          m2_repeticoes_restantes = 0;
          m1_comando_infinito = false;
          m2_comando_infinito = false;
          repetir_todas_linhas = false;
          qtd_comandos_na_fila = 0;
          
          uint8_t mov1 = m1_em_movimento;
          uint8_t mov2 = m2_em_movimento;
          sei();
          
          if (!mov1) {
            m1_executando = false;
            m1_em_pausa = false;
          }
          if (!mov2) {
            m2_executando = false;
            m2_em_pausa = false;
          }
          fila_iniciada = false;
          broadcastln(F("B1: Motor Parado"));
          broadcastln(F("C1:0"));
          return;
        }
      } else {
        int32_t valor = atol(valor_str);

        if (chave == 0x03) {
          repetir_todas_linhas = (valor == 1);
          broadcastln(repetir_todas_linhas ? F("B4: Rep. ON") : F("B6: Rep. OFF"));
          return;
        } else if (chave == 0x04) {
          global_pause_ms = valor;
          broadcast(F("B2: Pausa "));
          broadcast(valor);
          broadcastln(F("ms"));
          return;
        } else if (chave == 0x16) { 
          if (valor == 1) PORTA &= ~(1 << M1_EN_PIN);
          else if (valor == 2) PORTC &= ~(1 << M2_EN_PIN);
          broadcast(F("B7: M")); broadcast(valor); broadcastln(F(" Habilitado"));
          return;
        } else if (chave == 0x17) {
          if (valor == 1) PORTA |= (1 << M1_EN_PIN);
          else if (valor == 2) PORTC |= (1 << M2_EN_PIN);
          broadcast(F("B8: M")); broadcast(valor); broadcastln(F(" Desabilt."));
          return;
        } else if (chave == 0x18) {
          executarFastAction(valor);
          return;
        } else if (chave == 0x1A) {
          ComandoMotor p = lerPresetEEPROM(valor);
          broadcast(F("BA: L")); broadcast(valor);
          broadcast(F(" S:")); broadcast(p.step);
          broadcast(F(" V:")); broadcastln(p.vel);
          return;
        } else if (chave == 0x1B) {
          int32_t custom_rep = (terceiro_str != NULL) ? atol(terceiro_str) : 1;
          executarFastActionRepeat(valor, custom_rep);
          return;
        } else if (chave == 0x1C) {
          if (terceiro_str != NULL) {
            uint8_t idx_eeprom = valor;
            uint8_t idx_sram = atol(terceiro_str);
            if (idx_sram < qtd_comandos_na_fila && idx_eeprom < MAX_PRESETS) {
              gravarPresetEEPROM(idx_eeprom, fila_comandos[idx_sram]);
              broadcast(F("C0: Slot ")); broadcast(idx_eeprom); broadcastln(F(" Salvo"));
            } else {
              broadcastln(F("E1: Slot Invalido"));
            }
          }
          return;
        }

        if (chave == 0x10) {
          cmd.step = valor;
          cmd_valido = true;
        } else if (chave == 0x11) {
          cmd.vel = valor;
        } else if (chave == 0x12) {
          cmd.dir = valor;
        } else if (chave == 0x13) {
          cmd.repeat = valor;
        } else if (chave == 0x14) {
          cmd.pause_ms = valor;
        } else if (chave == 0x15) {
          cmd.motor_id = valor;
        } else if (chave == 0x19) {
          save_to_eeprom = true;
          eeprom_slot = valor;
        }
      }
    }
    par = strtok_r(NULL, ",", &ponteiro_virgula);
  }

  if (save_to_eeprom) {
    if (cmd_valido && eeprom_slot < MAX_PRESETS) {
      gravarPresetEEPROM(eeprom_slot, cmd);
      broadcast(F("B9:")); broadcast(eeprom_slot);
      broadcast(F(",10:")); broadcast(cmd.step);
      broadcast(F(",11:")); broadcast(cmd.vel);
      broadcast(F(",12:")); broadcast(cmd.dir);
      broadcast(F(",13:")); broadcast(cmd.repeat);
      broadcast(F(",14:")); broadcast(cmd.pause_ms);
      broadcast(F(",15:")); broadcastln(cmd.motor_id);
    } else {
      broadcastln(F("E3: Erro Sintaxe"));
    }
    return;
  }

  if (cmd_valido) {
    if (qtd_comandos_na_fila < MAX_FILA) {
      fila_comandos[qtd_comandos_na_fila] = cmd;
      broadcast(F("C0:")); broadcastln(qtd_comandos_na_fila);
      qtd_comandos_na_fila++;
      broadcast(F("C1:")); broadcastln(qtd_comandos_na_fila);
    } else {
      broadcastln(F("E2: Fila Cheia"));
    }
  }
}

void diagnosticoEEPROM() {
  Serial.println(F("--- EEPROM Slots ---"));
  for (uint8_t s = 0; s < MAX_PRESETS; s++) {
    ComandoMotor cmd = lerPresetEEPROM(s);
    Serial.print(F("Slot "));
    Serial.print(s);
    Serial.print(F(": "));
    if (cmd.step == 0 && cmd.vel == 0)
      Serial.println(F("[Vazio]"));
    else {
      Serial.print(F("S:")); Serial.print(cmd.step);
      Serial.print(F(" V:")); Serial.print(cmd.vel);
      Serial.print(F(" D:")); Serial.print(cmd.dir);
      Serial.print(F(" R:")); Serial.print(cmd.repeat);
      Serial.print(F(" P:")); Serial.print(cmd.pause_ms);
      Serial.print(F(" M:")); Serial.println(cmd.motor_id);
    }
  }
  broadcastln(F("--------------------"));
}

void readSerial() {
  // Lida com a porta USB (PC)
  while (Serial.available() > 0) {
    char c = Serial.read();

    if (c == '\n' || c == '\r') {
      if (serialBufferIdx > 0) {
        serialBuffer[serialBufferIdx] = '\0';
        interpretarComando(serialBuffer);
        serialBufferIdx = 0;
      }
    } else if (serialBufferIdx < MAX_INPUT_LEN) {
      serialBuffer[serialBufferIdx++] = c;
    }
  }

  // Lida com a porta Serial1 (Commander ATmega328P em RX1/TX1)
  while (Serial1.available() > 0) {
    char c = Serial1.read();

    if (c == '\n' || c == '\r') {
      if (serialBufferIdx1 > 0) {
        serialBuffer1[serialBufferIdx1] = '\0';
        interpretarComando(serialBuffer1);
        serialBufferIdx1 = 0;
      }
    } else if (serialBufferIdx1 < MAX_INPUT_LEN) {
      serialBuffer1[serialBufferIdx1++] = c;
    }
  }
}

void loop() {
  readSerial();
  manageSteppers();

  static uint32_t last_debug = 0;
  if (millis() - last_debug > 1000) {
    last_debug = millis();
    uint32_t p1, p2;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
      p1 = m1_passos_restantes;
      p2 = m2_passos_restantes;
    }
    if (m1_executando || m2_executando) {
      Serial.print(F("DIAG-RUN: M1_Passos="));
      broadcast(p1);
      broadcast(F(" M2_Passos="));
      broadcastln(p2);
    }
  }
}
