# ⚙️ Stepper Proof v3.0

Controle simultâneo e independente de alta precisão para dois motores de passo TB6600 via Web Serial API e interface física integrada (Monolítica).

## Quick Start

### 1. Hardware Setup
Conecte os motores e periféricos ao MCU (**Arduino Mega 2560**):
- **Motor 1:** DIR(D22), PUL(D24), ENA(D26) — [PORTA]
- **Motor 2:** DIR(D30), PUL(D32), ENA(D34) — [PORTC]
- **Teclado 4x4:** Linhas(A8-A11), Colunas(A12-A15) — [PORTK]
- **LCD 16x2 I2C:** SDA(D20), SCL(D21) — [Endereço 0x27]

*(Nota: Os drivers usam lógica de Enable invertida, LOW para ativar).*

### 2. Firmware (AVR)
Abra `stepcontrol-v2/stepcontrol-v2.ino`, compile e faça o upload para sua placa Mega 2560. A porta serial opera a **9600 bps**.

### 3. Interface Web
Execute o servidor local apontando para a pasta `public/`. Acesse `http://localhost:5500` no Google Chrome ou Edge (necessário suporte nativo a Web Serial API).

## Estrutura do Projeto

| Diretório | Descrição |
|:---|:---|
| `stepcontrol-v2/` | Firmware Monolítico Principal (Motor Core + UI Commander) |
| `docs/` | Documentação Técnica e Guias de Integração |
| `public/` | Dashboard Web (React/Tailwind) |
| `stepcommander/` | [Legacy] Firmware V1 para interface externa |

## Arquitetura Monolítica

A versão 3.0 consolida o controle de motores e a interface física em um único ATmega2560, eliminando latências de comunicação serial física e liberando pinos de I/O.

```
┌──────────────┐  USB Serial    ┌─────────────────────────────────────────┐
│  Web Browser  │ ──────────── │           ATmega2560 (Monolítico)       │
│  (Chrome/Edge)│  (RX0/TX1)   │ ┌──────────────┐      ┌────────────────┐ │
└──────────────┘               │ │ Módulo Core  │ <──> │ Módulo UI (LCD)│ │
                               │ └──────────────┘      └────────────────┘ │
                               └──────────────────────────────────────────┘
```

### Desacoplamento Lógico (Virtual Serial)

O sistema mantém um **desacoplamento estrito** entre a Interface Local e o Núcleo de Execução:
- **Ponte de Comando**: O teclado envia comandos para a função `interpretarComando(char*)`, simulando um cliente serial interno.
- **Roteamento de Respostas**: O firmware identifica se o comando veio da USB ou da Interface Local para formatar a resposta ideal (Hex bruta vs. Texto amigável).

| Tipo de Resposta | USB Serial (Web) | Interface Local (LCD) |
|:---|:---:|:---:|
| **Status Global** | ✅ Broadcast Hex | ✅ Texto Traduzido |
| **Telemetria (D0, C1)**| ✅ Somente USB | ❌ Omitido (Performance) |
| **Erros (E0–E4)** | ✅ Hex | ✅ Alerta LCD |

## Features

- **Arquitetura Monolítica:** Controle e Interface em um único chip (ATmega2560).
- **Dual Motor Independente:** Controle via Timer1 Dual-Channel Freerunning.
- **Alta Precisão:** Alternância de pinos atômica, operando em blocos `cli/sei`.
- **Protocolo H8P:** Comunicação hexadecimal otimizada (20 slots SRAM máx.).
- **Atalhos Inteligentes:** `#+A+N` (Save EEPROM), `*+B` (Fast Act), `*+A` (Sinal `-`).
- **UI Premium (Tailwind):** Telemetria dual ao vivo e modo escuro.
- **Segurança:** *Safety Clamp* de 50µs e interrupção de emergência instantânea.
- **EEPROM Fast Action:** 10 slots de execução instantânea e salvamento dinâmico.
- **Commander V3.0:** Interface não-bloqueante integrada com menu interativo. Ver [Docs](./docs/STEPCOMMANDER.md).

## Configuration

| Parâmetro | Descrição | Default |
|:---|:---|:---|
| Baud Rate | Velocidade de comunicação serial | `9600 bps` |
| Pinos M1 | DIR, PUL, ENA do Motor 1 | D2, D3, D6 |
| Pinos M2 | DIR, PUL, ENA do Motor 2 | D4, D5, D7 |
| Pinos CMD Serial | RX, TX do SoftwareSerial (Commander) | A0, A1 |
| Safe Clamp | Limite mínimo de intervalo de pulso | `50 µs` |
| MAX_FILA | Capacidade da fila SRAM (placa principal) | `20 slots` |
| MAX_PRESETS | Slots de preset na EEPROM | `10 slots` |
| MAX_INPUT_LEN | Tamanho máximo de comando (Commander) | `64 chars` |
| QUEUE_MAX_SLOTS | Fila SRAM local do Commander | `5 slots` |

## Documentation

- [Guia de Integração & API H8P](./docs/INTEGRATION.md)
- [StepCommander Setup](./docs/STEPCOMMANDER.md)
- [AI Context (llms.txt)](./llms.txt)
- [Changelog](./CHANGELOG.md)

## Contributing

1. Fork → 2. Feature Branch → 3. Commit → 4. Push → 5. Pull Request.

- **AVR:** Zero *String* de dados, use interrupções atômicas.
- **Front-end:** Mantenha a paleta original (`navy`, `cream`, `orange`) e estrutura Tailwind.

## License

MIT