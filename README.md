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
- **UI Premium (Tailwind):** Telemetria dual ao vivo, modo escuro em painéis e "Command Builder" minimalista com altura fixa de 600px que previne a necessidade de scroll vertical.
- **Segurança de Hardware (Atomic & Safe):** Bloco atômico para interrupção de emergência (`STOP`). *Safety Clamp* de 50µs previne travamento do MCU.
- **Driver Enable/Disable:** Controle dinâmico `16:X` / `17:X` em tempo real.
- **Fast Action Presets (EEPROM):** Execução instantânea de 5 comandos pré-gravados via `18:X`, com suporte a gravação dinâmica (`19`) e leitura (`1A`).
- **Internacionalização (i18n):** Strings dinâmicas em `EN-US` e `PT-BR`.
- **Configuração & Customização Total:** Painel de *Settings* que permite substituir qualquer termo do dicionário (*Dictionary Override*) e gerenciar a visibilidade de alertas (*Visibility Control*), com salvamento instantâneo via LocalStorage.

## Configuration

| Configuração | Descrição | Onde Localizar | Padrão |
| :--- | :--- | :--- | :--- |
| **Baud Rate** | Velocidade da Serial | `stepcontrol.ino` e `<select>` do `index.html` | `9600` |
| **Pinos de Controle M1** | DIR, PUL e EN para o Motor 1 | Macros em `stepcontrol.ino` | D2, D3, D6 |
| **Pinos de Controle M2** | DIR, PUL e EN para o Motor 2 | Macros em `stepcontrol.ino` | D4, D5, D7 |
| **Safe Clamp** | Limite menor de pulso | Loop principal interno em `.ino`| `50 µs` |

## Documentation

- [Referência de API (Protocolo H8P)](./docs/INTEGRATION.md)
- [Arquivo de Indexação para IA (llms)](./llms.txt)
- [Registro de Alterações](./CHANGELOG.md)

## Contributing

Contribuições para o **Stepper Proof** são bem-vindas. Mantenha os seguintes princípios ao codificar:
- **AVR:** Minimize o uso de SRAM (nenhuma *String* de dados). Use blocos atômicos `cli`/`sei` em buffers compartilhados.
- **Front-end:** Siga as regras estruturais de Tailwind configuradas, sempre preservando paletas originais (`navy`, `cream`, `orange`) e as dimensões sem rolagem vertical desnecessária.

1. Faça um Fork do repositório
2. Crie uma branch de feature (`git checkout -b feature/MinhaFeature`)
3. Faça o commit de suas mudanças (`git commit -m 'Feature explicativa'`)
4. Faça o push para a branch (`git push origin feature/MinhaFeature`)
5. Abra um Pull Request

## License

MIT