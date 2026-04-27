# StepCommander

Interface física de controle operando em um Atmega328P independente via SoftwareSerial, Teclado Matricial 4x4 e Display LCD 16x2 I2C.

## Versões do Firmware

O projeto disponibiliza duas implementações distintas para o Commander, atendendo a diferentes necessidades de complexidade e performance.

### [V1] Original (`stepcommander/`)
Versão inicial focada em simplicidade e facilidade de leitura.
- **Arquitetura:** Sequencial (Polling).
- **Comunicação:** Utiliza `readStringUntil` (bloqueante).
- **Gerenciamento de Memória:** Uso intensivo de objetos `String`.
- **Ideal para:** Testes rápidos e aprendizado da lógica básica do protocolo H8P.

### [V2] Otimizada (`stepcommander-v2/`) — v2.2
Versão refatorada para alta performance e estabilidade em ambientes de produção.
- **Arquitetura:** Multitarefa Cooperativa (Máquina de Estados baseada em `millis`).
- **Comunicação:** RX Não-bloqueante (caractere por caractere via buffer estático `char[]`).
- **Gerenciamento de Memória:** Otimização severa de SRAM usando `F()` (PROGMEM), `PSTR()` e bitfields para flags.
- **Persistência Local:** 10 slots de macro EEPROM (64 chars cada) para armazenamento de comandos frequentes.
- **Fila SRAM:** 5 slots de comandos de motor enfileirados localmente, persistentes até clear manual.
- **TM1638 Opcional:** Suporte ao módulo 7 segmentos desabilitado por padrão para economizar memória. Descomente as referências no código para ativar.
- **Ideal para:** Operação contínua onde a responsividade do teclado é crítica durante a recepção de dados.

---

## Hardware Setup (Comum a ambas as versões)

### Configuração de Pinos
- **Teclado 4x4:** Linhas (5, 4, 3, 2), Colunas (9, 8, 7, 6).
- **LCD 16x2 I2C:** Endereço `0x27` (SDA=A4, SCL=A5).
- **Comunicação:** SoftwareSerial (RX:10, TX:11) → Conecta ao A0/A1 da placa principal.
- **Opcional:** TM1638plus nos pinos A0 (STB), A1 (CLK), A2 (DIO) — desabilitado por padrão no v2.2.

### Conexão com a Placa Principal

```
 Commander (V2)         Placa Principal (stepcontrol)
 ┌──────────┐           ┌──────────────────┐
 │ TX (D11) │ ────────→ │ RX (A0)          │
 │ RX (D10) │ ←──────── │ TX (A1)          │
 │ GND      │ ────────→ │ GND              │
 └──────────┘           └──────────────────┘
```

> **Importante:** Os pinos TX/RX são cruzados (TX do Commander → RX do Controlador e vice-versa).

---

## Features e Atalhos

### Atalhos do Teclado

| Combo | Ação | Descrição |
|:---|:---|:---|
| `*` | `:` | Separador chave:valor |
| `#` | `,` | Separador de parâmetros |
| `*` + `#` | **ENTER** | Enfileira comandos de motor na SRAM ou envia direto (passthrough). Buffer vazio reenvia o último comando. |
| `*` + `C` | **Clear** | Limpa o buffer de entrada. |
| `*` + `D` | **Backspace** | Apaga o último caractere. |
| `*` + `A` | `-` | Insere sinal de menos (para `1B` com inversão). |
| `*` + `B` | **Toggle Fast Act** | Liga/desliga o modo Fast Action. |
| `*` + `A` + `C` + `[0-9]` | **Save to EEPROM** | Salva o conteúdo do buffer no slot EEPROM local. |
| `*` + `0` + `0` + `0` | **Menu Fila SRAM** | Abre menu interativo para enviar fila à MCU ou limpar a SRAM. |

### Limite de Caracteres

O buffer de entrada suporta até **64 caracteres**. Ao atingir o limite, o LCD exibe `"LIMITE!"` em vez de travar o display.

### Fila de Comandos SRAM (v2.2)

Comandos que contêm parâmetros de motor (`10:` ou `11:`) são automaticamente enfileirados na SRAM local ao pressionar ENTER, em vez de serem enviados imediatamente.

**Comportamento:**
1. **Enfileirar**: Comando com `10:` ou `11:` → vai para a fila SRAM (até 5 slots).
2. **Executar**: Digitar `01` + ENTER → re-envia toda a fila para a placa principal + executa.
3. **Passthrough**: Comandos sem parâmetros de motor (`02`, `03`, `16:1`, etc.) → enviados direto.
4. **Persistência**: A fila permanece na SRAM até o clear manual via menu.

**Indicador LCD:**
```
[3]Cmd:10:1600     ← Mostra 3 itens na fila
Pronto.
```

**Menu SRAM** (`*` + `0` + `0` + `0`):
```
>Enviar MCU  [3]    ← Navegue com A/B, confirme com #
 Limpar SRAM        ← Cancele com *
```

| Ação do Menu | Resultado |
|:---|:---|
| **Enviar MCU** | Transmite toda a fila para a placa principal + envia `01` (run) |
| **Limpar SRAM** | Zera o contador da fila (`queueCount = 0`) |

### Modo Fast Action
Quando ativado (`*` + `B`), as teclas numéricas `0-9` executam diretamente o conteúdo salvo nos slots EEPROM locais do Commander, enviando-os para a placa principal sem necessidade de digitação manual.

### Feedback no LCD

O Commander traduz automaticamente os códigos H8P recebidos da placa principal para mensagens legíveis no display:

| Código Recebido | Mensagem LCD |
|:---|:---|
| `A0` | `Sis Inicializado` |
| `B0` | `Iniciando Fila` |
| `B1` | `Motor Parado` |
| `B4` / `B6` | `Repeat ON` / `Repeat OFF` |
| `B5` | `Fila Executada` |
| `B7` / `B8` | `Motor Habilitado` / `Motor Desabiltd.` |
| `B9` / `BA` | `Preset EEPROM` |
| `BB` / `BC` | `FastAct. Exec` |
| `C0:X` | `Fila: Linha X` |
| `E0` | `Err: Em Execucao` |
| `E1` | `Err: Fila Vazia` |
| `E2` | `Err: Fila Cheia` |
| `E3` | `Erro de Sintaxe` |
| `E4` | `Preset Invalido` |

> **Nota:** As mensagens `D0` (telemetria de linha ativa) e `C1` (contagem de fila) **não são recebidas** pelo Commander — são exclusivas da USB Serial para evitar interferência nos pulsos do Timer1.

### Modos de Feedback
- **Display LCD**: Linha 1 exibe o comando em construção (`Cmd:`, `[N]Cmd:` com fila, ou `[F]Cmd:` no modo Fast Act). Linha 2 exibe o feedback H8P traduzido com efeito marquee automático para mensagens longas.
- **Monitor Serial USB**: Logs de debug são ecoados via porta USB nativa (9600 bps) para diagnóstico sem necessidade do LCD.

---

## API Reference
O Commander se comunica com a placa principal usando o protocolo **H8P**. Consulte o [Guia de Integração](./INTEGRATION.md) para detalhes completos da sintaxe hexadecimal e tabelas de roteamento de respostas.
