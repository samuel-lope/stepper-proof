# Guia de Integração: Web Interface <> Firmware AVR

Este documento detalha o protocolo de comunicação entre a interface HTML5 (frontend) e o firmware Motor Control (ATmega328P).

---

## 🏗️ Arquitetura do Sistema

O projeto utiliza um Arduino Uno/Nano/Pro Mini (ATmega328P) como núcleo de processamento.

| Componente | Hardware | Função | Firmware |
|:---|:---|:---|:---|
| **Motor Control** | ATmega328P | Geração de pulsos, Fila SRAM e EEPROM | `stepcontrol.ino` |

### Comunicação Serial (H8P Protocol)

O Motor Control se comunica via Serial (USB):

1.  **Protocolo Unificado**: Utiliza o protocolo **H8P** (Hex 8-bit Protocol). O firmware recebe strings e devolve códigos de status.
2.  **Web Dashboard**: O JS interpreta os códigos e atualiza a telemetria ao vivo (`D0`, `C1`).

---

## 📟 Protocolo Hexadecimal (H8P)

O protocolo **H8P (Hex 8-bit Protocol)** utiliza chaves hexadecimais para minimizar o tráfego de dados e o uso de SRAM.

### 📤 Comandos de Controle (PC → Arduino)

| Código | Comando | Parâmetro | Descrição |
|:---|:---|:---|:---|
| `01` | **RUN** | (nenhum) | Inicia a execução da fila SRAM. |
| `02` | **STOP** | (nenhum) | Parada imediata, limpa fila e cancela repetições. |
| `10` | **Steps** | Qtd Passos | Quantidade de pulsos a enviar. |
| `11` | **Interval**| µs | Velocidade (delay entre pulsos). |
| `12` | **Direction**| `0`/`1` | Sentido de rotação. |

---

## 📡 Respostas do Arduino (Status Codes)

| Código | Tipo | Descrição |
|:---|:---|:---|
| `A0` | System | Inicialização concluída |
| `B0` | Status | Fila em execução |
| `B1` | Status | Motores parados/limpeza |
| `B5` | Finish | Fila concluída com sucesso |
| `E0` | Erro | Motores já em movimento |
| `E1` | Erro | Fila vazia |

---

*Para o esquema de ligação de hardware completo, veja [README.md](../README.md).*
t Invalido" |

---

*Para o esquema de ligação de hardware completo, veja [README.md](../README.md).*
