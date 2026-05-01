# StepCommander v3.0

Interface física de controle integrada (Monolítica) operando no **ATmega2560**. Esta versão consolida o teclado matricial e o display LCD no mesmo MCU que controla os motores.

## Arquitetura Monolítica

Diferente das versões anteriores, o Commander v3.0 não requer uma segunda placa Arduino. 
- **Comunicação Interna**: Utiliza uma ponte lógica via chamadas de função (`interpretarComando`), eliminando a latência da SoftwareSerial.
- **Não-bloqueante**: Máquina de estados baseada em `millis` garante que o teclado e o display não interfiram no tempo de pulso dos motores.
- **SRAM Robusta**: Aproveita os 8KB de SRAM do Mega 2560 para buffers de comando estáveis.

---

## Hardware Setup

### Configuração de Pinos (ATmega2560)
- **Teclado 4x4:** Conectado ao **PORTK** (A8 a A15).
  - Linhas: A8, A9, A10, A11
  - Colunas: A12, A13, A14, A15
- **LCD 16x2 I2C:** Endereço `0x27` (SDA=D20, SCL=D21).

---

## Features e Atalhos

### Atalhos do Teclado

| Combo | Ação | Descrição |
|:---|:---|:---|
| `*` | `:` | Separador chave:valor |
| `#` | `,` | Separador de parâmetros |
| `*` + `#` | **ENTER** | Envia o buffer para o parser interno. Buffer vazio reenvia o último comando. |
| `*` + `C` | **Clear** | Limpa o buffer de entrada. |
| `*` + `D` | **Backspace** | Apaga o último caractere. |
| `*` + `A` | `-` | Insere sinal de menos (para valores negativos). |
| `*` + `B` | **Toggle Fast Act** | Liga/desliga o modo Fast Action (Execução via Teclas 0-9). |
| `#` + `A` + `[0-9]` | **Save to EEPROM** | Salva o conteúdo do buffer no slot EEPROM selecionado. |
| `*` + `000` | **Menu Fila SRAM** | Abre menu interativo para gerenciar a fila de execução. |

### Fila de Comandos SRAM

O sistema permite gerenciar a fila de comandos diretamente pelo teclado através do menu `*+000`.

**Ações do Menu:**
| Opção | Resultado |
|:---|:---|
| **Executar Fila** | Inicia a execução sequencial dos comandos na SRAM (`01`). |
| **Limpar SRAM** | Zera todos os slots da fila local. |

### Modo Fast Action
Quando ativado (`*` + `B`), as teclas numéricas `0-9` executam diretamente o conteúdo salvo nos slots EEPROM, enviando-os para o núcleo de execução instantaneamente. O LCD exibe `[F]Cmd:` para indicar que o modo está ativo.

### Feedback no LCD

O Commander traduz os códigos H8P do núcleo para mensagens legíveis:

| Código | Mensagem LCD |
|:---|:---|
| `A0` | `Sis Inicializado` |
| `B0` | `Executando...` |
| `B1` | `Motor Parado` |
| `B5` | `Fila Executada` |
| `B7` / `B8` | `Motor ON` / `Motor OFF` |
| `C0:X` | `Slot X (Fila)` |
| `B9:X` | `Preset X Salvo` |
| `E0-E4` | Mensagens de Erro |

> **Performance:** Telemetrias de alta frequência (`D0`, `C1`) são enviadas apenas via USB Serial para não sobrecarregar o barramento I2C do LCD durante movimentos rápidos.

---

## API Reference
O sistema utiliza o protocolo **H8P**. Consulte o [Guia de Integração](./INTEGRATION.md) para detalhes completos da sintaxe e parâmetros.
