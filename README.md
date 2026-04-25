# ⚙️ Stepper Proof

Controle simultâneo e independente de alta precisão para dois motores de passo TB6600 via Web Serial API e interface física dedicada.

## Quick Start

### 1. Hardware Setup
Conecte os motores ao MCU (Arduino Uno/Nano):
- **Motor 1:** DIR(D2), PUL(D3), ENA(D6)
- **Motor 2:** DIR(D4), PUL(D5), ENA(D7)
- **SoftwareSerial (Commander):** RX(D10), TX(D11)

*(Nota: Os drivers usam lógica de Enable invertida, LOW para ativar).*

### 2. Firmware (AVR)
Abra `stepcontrol/stepcontrol.ino`, compile e faça o upload. A porta serial operará a **9600 bps**.

Para controle físico opcional, carregue o firmware do StepCommander V2 em `stepcommander-v2/` em um segundo Arduino.

### 3. Interface Web
Execute o servidor local apontando para a pasta `public/`. Acesse `http://localhost:5500` no Google Chrome ou Edge (necessário suporte nativo a Web Serial API).

## Estrutura do Projeto

| Diretório | Descrição |
|:---|:---|
| `stepcontrol/` | Firmware da Placa Principal (Dual Motor Engine) |
| `stepcommander/` | Firmware V1 do Interface de Teclado (Simples/Legacy) |
| `stepcommander-v2/` | Firmware V2 do Interface de Teclado (Não-bloqueante/Otimizado) |
| `public/` | Dashboard Web (React/Tailwind) |
| `docs/` | Documentação Técnica e Guias de Integração |

## Arquitetura de Comunicação

O firmware principal (`stepcontrol.ino`) opera com **duas interfaces de entrada simultâneas**:

```
┌──────────────┐  USB Serial    ┌─────────────────────┐  SoftwareSerial  ┌──────────────────┐
│  Web Browser  │ ──────────── │   stepcontrol.ino    │ ──────────────── │ stepcommander-v2 │
│  (Chrome/Edge)│  (RX0/TX1)   │ (Placa Principal)    │   (D10/D11)      │ (Teclado + LCD)  │
└──────────────┘               └─────────────────────┘                   └──────────────────┘
```

### Roteamento Inteligente de Respostas

O firmware identifica automaticamente a **origem de cada comando** e roteia as respostas:

| Tipo de Resposta | USB Serial (Web) | SoftwareSerial (Commander) |
|:---|:---:|:---:|
| **Respostas diretas** (C0, B9, BA, BB, BC) | Dados completos (`C0:0,10:1600,11:250,...`) | Formato otimizado (`C0:0`) |
| **Eventos globais** (B0, B1, B4, B5, B6, B7, B8) | ✅ Broadcast | ✅ Broadcast |
| **Telemetria de alta frequência** (D0, C1) | ✅ Somente USB | ❌ Bloqueado |
| **Erros** (E0–E4) | ✅ Somente quem enviou | ✅ Somente quem enviou |

> **Por quê?** `SoftwareSerial` desabilita interrupts globais durante a transmissão de cada byte (~1ms/byte a 9600 baud). Enviar telemetria de alta frequência (D0 dispara a cada ciclo de passos) via SoftwareSerial causaria stuttering nos pulsos do Timer1, prejudicando a precisão dos motores.

## Features

- **Dual Motor Independente:** Controle simultâneo via Timer1 Dual-Channel Freerunning (COMPA/COMPB).
- **Alta Precisão:** Alternância de pinos de até 62,5ns, operando em bloco atômico (evitando *jitter*).
- **Protocolo H8P:** Comunicação 100% hexadecimal otimizada para SRAM de 2KB (20 slots máx.).
- **Dual Interface:** Controle via Web Serial API (USB) ou teclado matricial físico (SoftwareSerial).
- **EEPROM Fast Action:** 10 presets pré-gravados com execução instantânea (`18`, `19`, `1A`, `1B`, `1C`).
- **UI Premium (Tailwind):** Telemetria dual ao vivo, modo escuro em painéis e Command Builder minimalista.
- **Segurança de Hardware:** Bloco atômico para interrupção de emergência (`STOP`) e *Safety Clamp* de 50µs.
- **Internacionalização (i18n):** Suporte nativo a `EN-US` e `PT-BR`.
- **StepCommander V2:** Módulo periférico não-bloqueante com LCD 16x2, teclado matricial 4x4 e modos Fast Action. Ver [Docs](./docs/STEPCOMMANDER.md).

## Configuration

| Parâmetro | Descrição | Default |
|:---|:---|:---|
| Baud Rate | Velocidade de comunicação serial | `9600 bps` |
| Pinos M1 | DIR, PUL, ENA do Motor 1 | D2, D3, D6 |
| Pinos M2 | DIR, PUL, ENA do Motor 2 | D4, D5, D7 |
| Pinos CMD Serial | RX, TX do SoftwareSerial (Commander) | D10, D11 |
| Safe Clamp | Limite mínimo de intervalo de pulso | `50 µs` |
| MAX_FILA | Capacidade da fila SRAM | `20 slots` |
| MAX_PRESETS | Slots de preset na EEPROM | `10 slots` |

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