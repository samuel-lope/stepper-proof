# Guia de Integração: Web Interface <> Firmware AVR

Este documento detalha o protocolo de comunicação e a arquitetura de integração entre a interface HTML5 (frontend) e o firmware ATmega328P (backend/hardware).

---

## 🏗️ Arquitetura de Comunicação

O projeto usa a **Web Serial API** para estabelecer um túnel direto navegador–microcontrolador via USB. Alternativamente, a comunicação pode ser originada localmente pelo módulo periférico **StepCommander**.

- **Camada de Transporte (Web)**: Serial RS-232 over USB (Hardware Serial, RX/TX nos pinos 0 e 1 do Arduino).
- **Camada de Transporte (StepCommander)**: Conexão via `SoftwareSerial` originada nos pinos 10 e 11 do Commander para o Hardware Serial do controlador principal.
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

### 📤 Protocolo de Saída (Web → Arduino)

Comandos injetados via Serial para controle de fluxo e estado.

#### `01` - RUN
Inicia a execução simultânea das filas de motor (`m1_executando = true`, `m2_executando = true`).
- **Resposta**: `B0` (Sucesso) ou `E1` (Fila Vazia).

#### `02` - STOP (Emergência)
Interrompe instantaneamente todos os pulsos via Timer1 de forma atômica (`cli`). Limpa a SRAM e EEPROM buffers.
- **Resposta**: `B1` (Sucesso).

#### `03:X` - Loop Mode
Define se a fila deve recomeçar do zero após atingir o final.
- `03:1`: Ativa Loop Infinito.
- `03:0`: Desativa Loop Infinito.
- **Resposta**: `B4` (Ativo) ou `B6` (Inativo).

#### `04:X` - Global Pause
Define um atraso em milissegundos injetado entre comandos da fila.
- **Parâmetro**: `X` = milissegundos.
- **Resposta**: `B2:X`.

#### `16:X` / `17:X` - Driver Control
Habilita ou desabilita fisicamente o estágio de potência do driver TB6600 (EN Pin).
- `16:X`: Habilita Motor X (EN → LOW).
- `17:X`: Desabilita Motor X (EN → HIGH).
- **Resposta**: `B7:X` ou `B8:X`.

#### `18:X` - Fast Action (EEPROM)
Executa o preset armazenado no slot `X` (0-9) da EEPROM.
- **Resposta**: `BB:X,...` (Sucesso) ou `E4` (Slot Inválido).

#### `19:X,...` - Write Preset
Grava uma linha completa de comando no slot `X` da EEPROM.
- **Sintaxe**: `19:X,10:step,11:vel,12:dir,13:repeat,14:pause,15:motor`
- **Resposta**: `B9:X,...`.

#### `1A:X` - Read Preset
Solicita o dump de dados do slot `X` da EEPROM.
- **Resposta**: `BA:X,...`.

#### `1B:X:Y` - Scrubber / Jog
Executa preset `X` com `Y` repetições forçadas. Se `Y` for negativo, inverte a direção original.
- **Resposta**: `BC:X,...`.

#### `1C:X:Y` - Save SRAM to EEPROM
Salva a linha de comando armazenada no slot SRAM `Y` (0-19) diretamente no slot EEPROM `X` (0-9).
- **Resposta**: `B9:X,...` (Confirmação com a linha copiada) ou `E1` (SRAM vazia/inválida).

### 📥 Parâmetros de Motor (Data Injection)

Enviados como string de campos hexadecimais separados por vírgula.

| Chave | Parâmetro | Unidade | Obrigatório |
|:---|:---|:---|:---|
| `10` | **Steps** | Qtd Passos | ✅ Sim |
| `11` | **Interval** | Microssegundos (µs) | ✅ Sim (Min: 50) |
| `12` | **Direction**| `0` (REV) / `1` (FWD) | ❌ Não |
| `13` | **Repeat** | Ciclos (`0` = ∞) | ❌ Não |
| `14` | **Pause** | Millissegundos (ms) | ❌ Não |
| `15` | **Target** | `1` (M1) / `2` (M2) | ❌ Não |

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
| `B4` | **Repeat ON** | Loop infinito ativado (`03:1` recebido). |
| `B5` | **Queue Done** | Todos os motores concluíram a fila. Standby. |
| `B6` | **Repeat OFF** | Loop infinito desativado (`03:0` recebido). |
| `E0` | **Already Running** | Operação rejeitada — motores em execução ativa. |
| `E1` | **Queue Empty** | RUN rejeitado — fila vazia / ou slot SRAM inválido ao copiar para EEPROM. |
| `E2` | **Queue Overflow** | Limite de 20 slots atingido na SRAM. Use STOP para limpar. |
| `E3` | **Syntax Error** | Parâmetros obrigatórios (`10` ou `11`) ausentes no pacote. |
| `B7:X` | **Motor Enabled** | Driver do Motor X habilitado (EN → LOW). Torque de retenção ativo. |
| `B8:X` | **Motor Disabled** | Driver do Motor X desabilitado (EN → HIGH). Eixo livre. |
| `B9:X,...` | **Preset Saved** | Confirmação da gravação na EEPROM. Retorna a linha gravada. |
| `BA:X,...` | **Preset Data** | Resposta do comando de leitura `1A`. Retorna string do respectivo slot. |
| `BB:X,...` | **Preset Executed** | Resposta do `18`. Fast Action executado no slot X. |
| `BC:X,...` | **Rep Override** | Resposta do `1B`. Fast Action executado no slot X com repetições substituídas. |
| `E4` | **Invalid Slot** | Rejeição: O slot EEPROM requisitado (X) é maior que o configurado (Max: 9). |

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

### Execute All (RUN com controle de Loop)

Quando o usuário clica em **EXECUTE ALL**, a interface injeta o estado atual do botão Mestre de Loop antes do `01`:

1. UI envia `03:1` (se toggle ON) ou `03:0` (se toggle OFF).
2. UI envia `01` — inicia a execução com o modo de loop já definido no MCU.

Isso garante que o MCU sempre receba o estado correto de loop imediatamente antes da execução, independente de comandos anteriores.

### Carregar Sequência da Biblioteca

Ao carregar uma sequência salva do `localStorage`, a interface executa limpeza automática antes de injetar:

1. UI envia `02` — limpa SRAM do MCU (STOP atômico).
2. `currentQueue = []` — reseta a fila local JS.
3. UI envia cada comando da sequência com delay de 150ms entre eles.

Isso previne duplicidade ou acúmulo de linhas residuais na SRAM.

### Enable/Disable Motor (Controle do Driver)

Os toggle switches integrados ao seletor de motor disparam comandos de controle do driver:

- **Checkbox marcado** → UI envia `16:X` (Enable Motor X: EN → LOW).
- **Checkbox desmarcado** → UI envia `17:X` (Disable Motor X: EN → HIGH).

O firmware confirma com `B7:X` ou `B8:X`, e a telemetria atualiza o estado para "Driver ON" ou "Driver OFF".

> [!NOTE]
> O TB6600 usa lógica de **enable ativo-baixo**: `LOW` habilita o driver (torque de retenção ativo), `HIGH` desabilita (eixo livre).

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
