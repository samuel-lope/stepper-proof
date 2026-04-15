# Guia de Integração: Web Interface <> Firmware AVR

Este documento detalha o protocolo de comunicação e a arquitetura de integração entre a interface HTML5 (frontend) e o firmware ATmega328P (backend/hardware).

---

## 🏗️ Arquitetura de Comunicação

O projeto usa a **Web Serial API** para estabelecer um túnel direto navegador–microcontrolador via USB.

- **Camada de Transporte**: Serial RS-232 over USB.
- **Configuração**: **9600 bps**, 8-N-1.
- **Protocolo**: Texto em pares `Chave:Valor` hexadecimais, separados por vírgula, terminados por `\n`.

---

## 🤝 Handshake e Inicialização

Ao ser energizado ou resetado, o Arduino emite um sinal de prontidão:

1. **Arduino → Web**: Envia `A0`.
2. **Web → Usuário**: A interface detecta o código e exibe "Sistema Inicializado. Timer1 Dual-Channel sincronizado."

---

## 📟 Protocolo Hexadecimal (H8P)

Para economizar SRAM (2KB disponíveis no ATmega328P), o sistema não processa strings longas. Usa chaves de 1 byte (2 chars hexadecimais).

### 📤 Comandos de Fluxo (Web → Arduino)

Disparam ações imediatas no sistema de estados.

| Chave | Ação | Descrição |
| :--- | :--- | :--- |
| `01` | **RUN** | Inicia a execução sequencial da fila. Ativa a flag `fila_iniciada`. |
| `02` | **STOP** | Interrompe ambos os canais do Timer1 atomicamente (`cli`/`sei`), limpa a fila e desativa os motores. |
| `03` | **REPEAT ALL** | Define `repetir_todas_linhas = true` para loop infinito da fila completa. |
| `04:X` | **PAUSE GLOBAL** | Define pausa global (ms) entre todas as transições de linha. Ex: `04:500` |

### 📥 Parâmetros de Motor (Web → Arduino)

Enviados como string única com múltiplos campos separados por vírgula.

**Exemplo Motor 1:** `10:1600,11:500,12:1,13:2,15:1`
**Exemplo Motor 2:** `10:800,11:300,12:0,15:2`

| Chave | Parâmetro | Unidade | Obrigatório | Default |
| :--- | :--- | :--- | :--- | :--- |
| `10` | Steps | Quantidade de passos | ✅ Sim | — |
| `11` | Intervalo (Vel) | Microssegundos (µs) | ✅ Sim (Min: 50) | — |
| `12` | Direção | `0` ou `1` | ❌ Não | `0` |
| `13` | Repeat | Quantidade (`0` = infinito) | ❌ Não | `1` |
| `14` | Pause After | Milissegundos (ms) | ❌ Não | `0` |
| `15` | Motor Alvo | `1` ou `2` | ❌ Não | `1` |

> [!CAUTION]
> **Safety Clamp de Frequência**: O firmware rejeita intervalos (`11`) menores que **50µs**. Valores abaixo disso causariam *starvation* de interrupções e travamento do MCU. A resposta de rejeição é `E3`.

> [!IMPORTANT]
> **Campos obrigatórios**: `10` e `11` são **sempre obrigatórios**. Se ausentes, o firmware retorna `E3` e descarta o pacote inteiro sem gravar na fila.

---

## 📡 Respostas do Arduino (Arduino → Web)

### Respostas de Status (Toasts)

| Código | Nome | Descrição |
| :--- | :--- | :--- |
| `A0` | **Init OK** | Sistema inicializado. Timer1 Dual-Channel pronto. |
| `B0` | **Run Started** | Fila iniciada. Pulsos sendo despachados para M1 e/ou M2. |
| `B1` | **Emergency Stop** | Ambos os motores parados. Fila e RAM limpas. |
| `B2:X` | **Global Pause Set** | Pausa global definida para X ms. |
| `B4` | **Repeat All** | Modo loop infinito ativado para toda a fila. |
| `B5` | **Queue Done** | Todos os motores concluíram a fila. Standby. |
| `E0` | **Already Running** | Operação rejeitada — motores em execução ativa. |
| `E1` | **Queue Empty** | RUN rejeitado — fila vazia, nada a executar. |
| `E2` | **Queue Overflow** | Limite de 20 slots atingido na SRAM. Use STOP para limpar. |
| `E3` | **Syntax Error** | Parâmetros obrigatórios (`10` ou `11`) ausentes no pacote. |

### 🛰️ Telemetria Passiva (H8P V2)

Emitidos continuamente pelo Arduino para atualizar o painel "Live Telemetry" sem gerar toasts.

| Código | Nome | Formato | Descrição |
| :--- | :--- | :--- | :--- |
| `C0:N,...` | **Slot Saved** | `C0:idx,10:steps,11:vel,12:dir,15:motor` | Confirmação de que a linha N foi gravada na RAM com seus parâmetros. |
| `C1:X` | **Queue Size** | `C1:N` | Quantidade atual de slots ocupados na SRAM (0–20). |
| `D0:X` | **Active Line** | `D0:MNN` | Linha NN do Motor M foi disparada. Ex: `D0:101` = Motor 1, slot 1. `D0:203` = Motor 2, slot 3. |
| `B3:X` | **Hardware Pause** | `B3:ms` | Pausa em andamento de X ms entre linhas. |

> [!NOTE]
> **Formato de `D0`**: O valor codifica motor e slot em um único inteiro. `floor(X / 100)` extrai o motor; `X % 100` extrai o índice do slot. A interface web já faz essa decodificação automaticamente.

---

## 🔄 Fluxo de Dados — Ciclo Completo

```mermaid
sequenceDiagram
    participant UI as Web Interface (JS)
    participant HW as Arduino (AVR)

    Note over UI,HW: 1. Construção da Fila
    UI->>HW: "10:1600,11:500,12:1,15:1\n" (Motor 1)
    HW-->>UI: "C0:0,10:1600,11:500,12:1,15:1" (Slot 0 gravado)
    HW-->>UI: "C1:1" (1 slot na RAM)

    UI->>HW: "10:800,11:300,15:2\n" (Motor 2)
    HW-->>UI: "C0:1,10:800,11:300,15:2" (Slot 1 gravado)
    HW-->>UI: "C1:2" (2 slots na RAM)

    Note over UI,HW: 2. Execução
    UI->>HW: "01\n" (RUN)
    HW-->>UI: "B0" (Fila iniciada)
    HW-->>UI: "D0:100" (M1 executando slot 0)
    HW-->>UI: "D0:201" (M2 executando slot 1)
    HW-->>UI: "B5" (Ambos concluídos)
```

---

## ⚡ Macros da Interface Web

A interface combina comandos atômicos para implementar comportamentos compostos.

### Run One Step (Movimento Isolado)

Para testar um movimento sem contaminar a fila persistente:

1. UI envia `02` — limpa qualquer resíduo na fila.
2. UI envia `10:X,11:Y,15:Z` — carrega o comando desejado.
3. UI envia `01` — dispara execução imediatamente.

---

## 🛠️ Detalhes Internos do Firmware

### Máquina de Estados (`maquinaDeEstadosMotor`)

Dois pipelines paralelos e independentes, executados a cada iteração do `loop()`:

```
Pipeline M1:
  m1_executando && !m1_em_movimento
    → em pausa? → aguardar millis()
    → senão?    → avancarFilaM1()

Pipeline M2:
  m2_executando && !m2_em_movimento
    → em pausa? → aguardar millis()
    → senão?    → avancarFilaM2()

Cleanup Global (portão fila_iniciada):
  fila_iniciada && !m1_executando && !m2_executando && qtd > 0
    → qtd = 0, fila_iniciada = false → emite B5
```

### Flag `fila_iniciada`

Gate crítico que impede a limpeza prematura da fila. O bug era: a condição de cleanup global (`!m1_executando && !m2_executando && qtd > 0`) era verdadeira a cada `loop()` após salvar um comando — antes do RUN ser enviado — destruindo a fila silenciosamente. A flag garante que a limpeza só ocorra após o motor ter realmente iniciado execução.

### ISRs dos Canais

```cpp
ISR(TIMER1_COMPA_vect) { /* Pulsa M1_PUL_PIN, recalcula OCR1A */ }
ISR(TIMER1_COMPB_vect) { /* Pulsa M2_PUL_PIN, recalcula OCR1B */ }
```

Cada ISR recalcula seu próprio `next_compare = TCNT1 + ticks_necessarios`, garantindo independência total de timing entre os dois canais.

---

*Para o esquema de ligação de hardware completo, veja [README.md](../README.md).*
