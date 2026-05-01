# ⚙️ Stepper Proof (ATmega328P)

Controle simultâneo e independente de alta precisão para dois motores de passo TB6600 via Web Serial API e interface física externa baseada no protocolo H8P.

## Quick Start

### 1. Hardware Setup
O sistema utiliza um Arduino Uno/Nano/Pro Mini (ATmega328P) para controle de motores e interface:

- **Motor Controller (ATmega328P):**
  - **Motor 1:** DIR(D2), PUL(D3) — [PORTD]
  - **Motor 2:** DIR(D4), PUL(D5) — [PORTD]
  - **Serial Link:** Conectado ao PC via USB.

*(Nota: Os drivers usam lógica de Enable invertida, LOW para ativar).*

### 2. Firmware (AVR)
- **Motor Control:** Upload de `stepcontrol/stepcontrol.ino` para o ATmega328P.

### 3. Interface Web
Execute o servidor local apontando para a pasta `public/`. Acesse `http://localhost:5500` no Google Chrome ou Edge.

## Estrutura do Projeto

| Diretório | Descrição |
|:---|:---|
| `stepcontrol/` | **Motor Control:** Firmware dedicado para ATmega328P |
| `stepcommander/` | **Commander:** Firmware de Interface (Opcional) |
| `docs/` | Documentação Técnica e Protocolo H8P |
| `public/` | Dashboard Web (React/Tailwind) |

## Features

- **Controle Preciso:** Gerenciamento de pulsos otimizado para motores de passo.
- **Fila SRAM:** Processamento sequencial de comandos.
- **EEPROM Presets:** Gravação de presets diretamente no microcontrolador.
- **UI Premium (Web):** Telemetria em tempo real e controle total via Web Serial.

## Configuration

| Parâmetro | Descrição | Default |
|:---|:---|:---|
| Baud Rate | Velocidade de comunicação serial | `9600 bps` |
| Safe Clamp | Limite mínimo de intervalo de pulso | `50 µs` |
| MAX_FILA | Capacidade da fila SRAM | `20 slots` |

## Documentation

- [Guia de Integração & API H8P](./docs/INTEGRATION.md)
- [StepCommander Setup](./docs/STEPCOMMANDER.md)
- [Changelog](./CHANGELOG.md)

## License

MIT