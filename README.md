# ⚙️ Stepper Proof v3.0 (Decoupled API Architecture)

Controle simultâneo e independente de alta precisão para dois motores de passo TB6600 via Web Serial API e interface física externa baseada no protocolo H8P.

## Quick Start

### 1. Hardware Setup
O sistema utiliza uma arquitetura de dois núcleos físicos para garantir performance máxima e zero jitter nos motores:

- **Motor Core (Arduino Mega 2560):**
  - **Motor 1:** DIR(D22), PUL(D24), ENA(D26) — [PORTA]
  - **Motor 2:** DIR(D30), PUL(D32), ENA(D34) — [PORTC]
  - **Serial Link:** RX0/TX0 conectados ao Commander.

- **Commander (Arduino Uno/Nano/Pro Mini - ATmega328P):**
  - **Teclado 4x4:** Linhas(D5-D2), Colunas(D9-D6).
  - **LCD 16x2 I2C:** SDA(A4), SCL(A5) — [Endereço 0x27].
  - **Serial Link:** RX/TX conectados ao Motor Core.

*(Nota: Os drivers usam lógica de Enable invertida, LOW para ativar).*

### 2. Firmware (AVR)
- **Motor Core:** Upload de `stepcontrol-v2/stepcontrol-v2.ino` para o Mega 2560.
- **Commander:** Upload de `stepcommander-v2/stepcommander-v2.ino` para o ATmega328P.

### 3. Interface Web
Execute o servidor local apontando para a pasta `public/`. Acesse `http://localhost:5500` no Google Chrome ou Edge.

## Estrutura do Projeto

| Diretório | Descrição |
|:---|:---|
| `stepcontrol-v2/` | **Motor Core:** Firmware dedicado (Timers + Fila SRAM + EEPROM) |
| `stepcommander-v2/` | **Commander:** Firmware de Interface (LCD + Keypad + UI Logic) |
| `docs/` | Documentação Técnica e Protocolo H8P |
| `public/` | Dashboard Web (React/Tailwind) |

## Arquitetura Desacoplada

A versão 3.0 utiliza uma arquitetura distribuída para isolar as rotinas de timing crítico dos motores (Mega 2560) das rotinas de interface lenta (I2C/LCD no 328P).

```
┌──────────────┐      USB       ┌────────────────────────┐      Serial      ┌────────────────────────┐
│  Web Browser  │ <───────────> │   ATmega2560 (Core)    │ <──────────────> │  ATmega328P (Commander)│
│  (Dashboard)  │               │ (Motors + SRAM Queue)  │  (H8P Protocol)  │   (LCD + Keypad)       │
└──────────────┘               └────────────────────────┘                  └────────────────────────┘
```

### Protocolo H8P (API Desacoplada)

O Motor Core (Mega 2560) expõe uma API Serial baseada em códigos hexadecimais:
- **Comandos**: Recebe strings H8P (ex: `10:1600,11:500`) e gerencia a execução.
- **Status (B-Codes)**: Emite códigos de estado curtos (ex: `B0`, `B5`) que o Commander traduz para o usuário.
- **Telemetria (D-Codes)**: Envia dados de execução bruta para o Dashboard Web via USB.

## Features

- **Arquitetura Distribuída:** Zero stutter nos motores ao interagir com a interface física.
- **Dual Motor 16-bit:** Controle via Timer1 e Timer3 do Mega 2560 para máxima precisão.
- **Fila SRAM de 20 slots:** Processamento autônomo e sequencial de comandos.
- **EEPROM Fast Action:** 10 slots de presets globais persistentes no Core.
- **Atalhos H8P:** Suporte a repetições customizadas (`1B`), salvamento dinâmico (`1C`) e modo Fast Act.
- **UI Premium (Web):** Telemetria em tempo real e controle total via Web Serial.

## Configuration

| Parâmetro | Descrição | Default |
|:---|:---|:---|
| Baud Rate | Velocidade de comunicação serial | `9600 bps` |
| Safe Clamp | Limite mínimo de intervalo de pulso | `50 µs` |
| MAX_FILA | Capacidade da fila SRAM (Motor Core) | `20 slots` |
| MAX_PRESETS | Slots de preset na EEPROM | `10 slots` |

## Documentation

- [Guia de Integração & API H8P](./docs/INTEGRATION.md)
- [StepCommander Setup](./docs/STEPCOMMANDER.md)
- [AI Context (llms.txt)](./llms.txt)
- [Changelog](./CHANGELOG.md)

## License

MIT