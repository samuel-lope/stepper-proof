# ⚙️ Stepper Proof: Controle Dual TB6600

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
Execute o servidor local apontando para a pasta `public/` (ex: extensão *Live Server* do VS Code). Acesse `http://localhost:5500` no Google Chrome ou Microsoft Edge (necessário suporte nativo a Web Serial API).

## Features

- **Dual Motor Independente:** Controle simultâneo via Timer1 Dual-Channel Freerunning.
- **Alta Precisão:** Alternância de pinos de até 62,5ns, operando em block atômico (evitando *jitter*).
- **Protocolo H8P Otimizado:** Uso mínimo de SRAM com array estrito em comunicação 100% de 8-bits hexadecimal.
- **UI Premium (Tailwind):** Telemetria dual ao vivo, modo escuro em painéis e "Command Builder" minimalista de altura fixa (600px).
- **Segurança de Hardware:** Bloco atômico para interrupção de emergência (`STOP`) e *Safety Clamp* de 50µs.
- **EEPROM Fast Action:** Execução de 5 comandos pré-gravados (`18`, `19`, `1A`).
- **Customização Total:** Painel de *Settings* com *Dictionary Override* e *Visibility Control*.
- **Internacionalização (i18n):** Suporte nativo a `EN-US` e `PT-BR`.

## Configuration

| Variável | Descrição | Localização | Padrão |
|:---|:---|:---|:---|
| **Baud Rate** | Velocidade da Porta Serial | `stepcontrol.ino` | `9600` |
| **Pinos M1** | DIR:D2, PUL:D3, EN:D6 | `stepcontrol.ino` | Fixos |
| **Pinos M2** | DIR:D4, PUL:D5, EN:D7 | `stepcontrol.ino` | Fixos |
| **Safe Clamp** | Limite menor de pulso | `stepcontrol.ino` | `50 µs` |

## Documentation

- [Referência de API (Protocolo H8P)](./docs/INTEGRATION.md)
- [Arquitetura & Fluxo](./docs/INTEGRATION.md#architectura-de-comunicação)
- [Arquivo de Indexação para IA (llms)](./llms.txt)
- [Changelog](./CHANGELOG.md)

## Contributing

Contribuições são bem-vindas. Siga os princípios:
- **AVR:** Zero *String* de dados, use interrupções atômicas.
- **Front-end:** Mantenha a paleta original (`navy`, `cream`, `orange`) e estrutura Tailwind.

1. Fork → 2. Feature Branch → 3. Commit → 4. Push → 5. Pull Request.

## License

MIT