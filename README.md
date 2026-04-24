# ⚙️ Stepper Proof

Controle simultâneo e independente de alta precisão para dois motores de passo TB6600 via Web Serial API.

## Quick Start

### 1. Hardware Setup
Conecte os motores ao MCU (Arduino Uno/Nano):
- **Motor 1:** DIR(D2), PUL(D3), ENA(D6)
- **Motor 2:** DIR(D4), PUL(D5), ENA(D7)

*(Nota: Os drivers usam lógica de Enable invertida, LOW para ativar).*

### 2. Firmware (AVR)
Abra `stepcontrol/stepcontrol.ino`, compile e faça o upload. A porta serial operará a **9600 bps**.

### 3. Interface Web
Execute o servidor local apontando para a pasta `public/`. Acesse `http://localhost:5500` no Google Chrome ou Edge (necessário suporte nativo a Web Serial API).

## Features

- **Dual Motor Independente:** Controle simultâneo via Timer1 Dual-Channel Freerunning.
- **Alta Precisão:** Alternância de pinos de até 62,5ns, operando em block atômico (evitando *jitter*).
- **Protocolo H8P Otimizado:** Uso mínimo de SRAM com array estrito em comunicação 100% de 8-bits hexadecimal.
- **UI Premium (Tailwind):** Telemetria dual ao vivo, modo escuro em painéis e "Command Builder" minimalista de altura fixa (600px).
- **Segurança de Hardware:** Bloco atômico para interrupção de emergência (`STOP`) e *Safety Clamp* de 50µs.
- **EEPROM Fast Action:** Execução de 5 comandos pré-gravados (`18`, `19`, `1A`).
- **Customização Total:** Painel de *Settings* com *Dictionary Override* e *Visibility Control*.
- **Internacionalização (i18n):** Suporte nativo a `EN-US` e `PT-BR`.
- **StepCommander (Interface Física):** Novo módulo periférico (Arduino secundário) com Teclado Matricial 4x4 e LCD 16x2 para controle sem PC via protocolo H8P.

## Configuration

| Variable | Description | Default |
|----------|-------------|---------|
| PORTA SERIAL | Baud Rate da comunicação | `9600` |
| Pinos M1 | Pinos do Driver Motor 1 | D2, D3, D6 |
| Pinos M2 | Pinos do Driver Motor 2 | D4, D5, D7 |
| Safe Clamp | Limite mínimo de pulso (`stepcontrol.ino`) | `50 µs` |

## Documentation

- [API Reference & Architecture](./docs/INTEGRATION.md)
- [StepCommander Setup](./docs/STEPCOMMANDER.md)
- [AI Context (llms.txt)](./llms.txt)
- [Changelog](./CHANGELOG.md)

## Contributing

1. Fork → 2. Feature Branch → 3. Commit → 4. Push → 5. Pull Request.

- **AVR:** Zero *String* de dados, use interrupções atômicas.
- **Front-end:** Mantenha a paleta original (`navy`, `cream`, `orange`) e estrutura Tailwind.

## License

MIT