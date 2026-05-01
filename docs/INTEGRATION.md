# Guia de Integração: Web Interface <> Firmware AVR

Este documento detalha o protocolo de comunicação e a arquitetura de integração entre a interface HTML5 (frontend), o firmware Motor Core (ATmega2560) e o módulo Commander (ATmega328P).

---

## 🏗️ Arquitetura Desacoplada (Core-Commander)

O projeto utiliza uma **arquitetura distribuída** para garantir que o processamento pesado de interface (LCD e Teclado) não interfira na geração precisa de pulsos dos motores.

Existem dois componentes de hardware principais:

| Componente | Hardware | Função | Firmware |
|:---|:---|:---|:---|
| **Motor Core** | Arduino Mega 2560 | Geração de pulsos, Timers 16-bit, Fila SRAM e EEPROM | `stepcontrol-v2.ino` |
| **Commander** | ATmega328P (Uno/Nano) | Interface física (LCD 16x2 + Teclado 4x4) | `stepcommander-v2.ino` |

### Comunicação Física (H8P Protocol)

O Commander se comunica com o Motor Core através de uma **ligação serial física** (UART):

1.  **Canal Principal (USB)**: A interface Web (Chrome/Edge) se conecta ao Mega 2560 via USB (Serial 0).
2.  **Canal de Interface (Serial Link)**: O Commander se conecta ao Mega 2560 através da porta RX0/TX0.
3.  **Protocolo Unificado**: Ambos os canais utilizam o protocolo **H8P** (Hex 8-bit Protocol). O Motor Core recebe strings e devolve códigos de status.

### Fluxo de Mensagens e Roteamento

As respostas do Motor Core são despachadas via Serial:
- **Broadcast**: Mensagens de status (`B0`, `B5`, `E2`) são enviadas para todos os ouvintes.
- **Commander**: O ATmega328P escuta a serial, identifica o código (ex: `B0`) e exibe a mensagem traduzida no LCD ("Executando...").
- **Web Dashboard**: O JS interpreta os códigos e atualiza a telemetria ao vivo (`D0`, `C1`).

---

## 🤝 Handshake e Inicialização

Ao ser energizado ou resetado, o Motor Core emite um sinal de prontidão:

1.  **Motor Core → Serial**: Envia `A0`.
2.  **Web**: Exibe "Sistema Inicializado. Motor Core pronto."
3.  **Commander**: Detecta `A0` e exibe "Sis Inicializado" no LCD.

---

## 📟 Protocolo Hexadecimal (H8P)

Para economizar SRAM e garantir velocidade, o sistema utiliza chaves de 1 byte (2 chars hexadecimais).

### 📤 Comandos de Controle (Enviados → Arduino)

#### `01` RUN
Inicia a execução simultânea das filas de motor.

**Response:**
- `B0`: Fila iniciada (Broadcast)
- `E0`: Operação rejeitada — motores em execução ativa
- `E1`: Fila vazia

#### `02` STOP (Graceful Stop)
Cancela repetições e esvazia a fila atual.

**Response:**
- `B1`: Motor PARADO e Fila limpa (Broadcast)

#### `03` RepeatAll / Loop Mode
**Parameters:** `1` ativa, `0` desativa.

#### `04` Global Pause
**Parameters:** ms (tempo de pausa entre linhas).

#### `16`/`17` Enable/Disable Motor
**Parameters:** `1` (M1), `2` (M2).

#### `18` Fast Action (EEPROM)
Executa preset do slot N.

#### `19` Write Preset
Grava linha completa na EEPROM.

#### `1A` Read Preset
Dump de dados do slot EEPROM.

#### `1B` Jog (Fast Action Repeat)
Executa preset com override de repetições.

#### `1C` Save SRAM to EEPROM
Salva slot da SRAM no slot da EEPROM.

### 📥 Parâmetros de Motor (Data Injection)

| Chave | Parâmetro | Unidade | Descrição |
|:---|:---|:---|:---|
| `10` | **Steps** | Qtd Passos | Quantidade de passos |
| `11` | **Interval**| µs (Min: 50) | Velocidade (intervalo entre passos) |
| `12` | **Direction**| `0`/`1` | Sentido |
| `13` | **Repeat** | Ciclos | Repetições |
| `14` | **Pause** | ms | Pausa pós-linha |
| `15` | **Target** | `1`/`2` | Motor Alvo |

---

## 📡 Respostas do Arduino (Status Codes)

| Código | Descrição |
|:---|:---|
| `A0` | Sistema Inicializado |
| `B0` | Executando / Retomando |
| `B1` | Motor Parado (Emergência) |
| `B5` | Fila Executada (Standby) |
| `B7/B8` | Motor Habilitado/Desabilitado |
| `E0` | Erro: Já em execução |
| `E1` | Erro: Fila vazia |
| `E2` | Erro: Fila cheia |

---

*Para o esquema de ligação de hardware completo, veja [README.md](../README.md).*
