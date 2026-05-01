# StepCommander v3.0 (External UI Module)

Interface física de controle operando em um **ATmega328P** (Arduino Uno/Nano/Nano Every). Esta placa atua como o "cérebro visual" do sistema, processando a entrada do usuário e exibindo feedback em tempo real, enquanto o Motor Core (Mega 2560) lida com o controle de potência.

## Arquitetura Desacoplada

O Commander v3.0 isola as operações de interface do loop de controle de motores:
- **Comunicação Física**: Utiliza uma conexão Serial (RX/TX) a 9600 bps com o Motor Core.
- **Não-bloqueante**: O processamento do teclado e as atualizações do LCD I2C não afetam o timing crítico dos motores.
- **Tradução de Status**: O Commander recebe códigos de status hexadecimais (B-Codes) do Motor Core e os traduz em mensagens amigáveis no display.

---

## Hardware Setup

### Configuração de Pinos (ATmega328P)
- **Teclado 4x4:** 
  - Linhas: D5, D4, D3, D2
  - Colunas: D9, D8, D7, D6
- **LCD 16x2 I2C:** Endereço `0x27` (SDA=A4, SCL=A5).
- **Serial Link:** TX/RX conectados aos pinos RX0/TX0 do Mega 2560.

---

## Features e Atalhos

### Atalhos do Teclado

| Combo | Ação | Descrição |
|:---|:---|:---|
| `*` | `:` | Separador chave:valor |
| `#` | `,` | Separador de parâmetros |
| `*` + `#` | **ENTER** | Envia o buffer para o Motor Core. Buffer vazio reenvia o último comando. |
| `*` + `C` | **Clear** | Limpa o buffer de entrada. |
| `*` + `D` | **Backspace** | Apaga o último caractere. |
| `*` + `A` | `-` | Insere sinal de menos. |
| `*` + `B` | **Toggle Fast Act** | Liga/desliga o modo Fast Action (Execução direta via 0-9). |
| `*` + `A` + `C` + `[0-9]` | **Save to EEPROM** | Salva o conteúdo do buffer no slot EEPROM do Core. |
| `*` + `000` | **Menu SRAM** | Menu interativo para Enviar/Limpar a fila do Core. |

### Feedback no LCD

O Commander traduz os códigos H8P recebidos do Core:

| Código | Mensagem LCD |
|:---|:---|
| `A0` | `Sis Inicializado` |
| `B0` | `Executando...` |
| `B1` | `Motor Parado` |
| `B5` | `Fila Executada` |
| `E0-E4` | Mensagens de Erro específicas |

---

## API Reference
O sistema utiliza o protocolo **H8P**. Consulte o [Guia de Integração](./INTEGRATION.md) para detalhes completos da sintaxe e parâmetros.
