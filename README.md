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
- **StepCommander (Interface Física):** Novo módulo periférico (Arduino secundário) com Teclado Matricial 4x4 e LCD 16x2 para controle sem PC via protocolo H8P.

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
- [StepCommander (Código da Interface)](./stepcommander/stepcommander.ino)
- [Changelog](./CHANGELOG.md)

## Protocolo de Comunicação (Índice Hexadecimal)

### 📤 Comandos e Parâmetros (Enviados para o Arduino)

| Hex | Comando | Descrição | Exemplo |
| :--- | :--- | :--- | :--- |
| `01` | **run** | Inicia a execução da fila | `01` |
| `02` | **stop** | Parada de emergência e limpa fila | `02` |
| `03` | **repeatAll** | Loop infinito da fila (1: ativa, 0: desativa) | `03:1` |
| `04` | **pause** | Pausa global em ms | `04:1000` |
| `10` | **step** | Quantidade de passos (Obrigatório) | `10:800` |
| `11` | **vel** | Intervalo em microssegundos (Obrigatório) | `11:300` |
| `12` | **dir** | Direção: 0 ou 1 (Opcional) | `12:1` |
| `13` | **repeat** | Repetições da linha (0 = infinito) (Opcional) | `13:5` |
| `14` | **pause** | Pausa em ms após a linha (Opcional) | `14:500` |
| `15` | **motor** | Motor Alvo: 1 ou 2 (Opcional, default: 1) | `15:2` |
| `16` | **enableMotor** | Habilita driver do motor (EN LOW) | `16:1` |
| `17` | **disableMotor** | Desabilita driver do motor (EN HIGH) | `17:1` |
| `18` | **fastAction** | Executa preset EEPROM (0 a 4) | `18:0` |
| `19` | **writePreset** | Grava preset na EEPROM | `19:2,10:800...` |
| `1A` | **readPreset** | Lê preset da EEPROM | `1A:2` |
| `1B` | **fastActionRep** | Executa preset EEPROM com loop customizado | `1B:0:-4` |

### 📥 Alertas e Respostas (Recebidos do Arduino)

| Hex | Significado |
| :--- | :--- |
| `A0` | Sistema Inicializado com Sucesso |
| `B0` | Iniciando execução da fila |
| `B1` | Motor PARADO e Fila limpa |
| `B2` | Pausa global definida |
| `B3` | [TELEMETRIA] Pausa em andamento (ms) |
| `B4` | Modo repeatAll ATIVADO |
| `B5` | Fila executada com sucesso |
| `B6` | Modo repeatAll DESATIVADO |
| `B7` | Motor habilitado (EN = LOW) |
| `B8` | Motor desabilitado (EN = HIGH) |
| `B9` | Preset EEPROM gravado (Eco de linha) |
| `BA` | Preset EEPROM lido (Dump de parâmetros) |
| `BB` | fastAction executado com sucesso |
| `BC` | fastActionRep executado com sucesso |
| `C0` | Linha salva com sucesso (Retorna Slot e parâmetros) |
| `C1` | [TELEMETRIA] Contagem de slots na SRAM |
| `D0` | [TELEMETRIA] Linha ativada (MNN: Motor e Slot) |
| `E0` | Erro: Motor já em execução |
| `E1` | Erro: Fila dSRAM vazia |
| `E2` | Erro: Fila SRAM cheia |
| `E3` | Erro de sintaxe (Parâmetros obrigatórios ausentes) |
| `E4` | Erro: Índice de preset inválido (fora de 0-4) |

## Hardware Adicional: StepCommander

Interface física de controle operando em um Atmega328P independente.

### Configuração de Pinos (Commander)
- **Teclado 4x4:** Linhas (9, 8, 7, 6), Colunas (5, 4, 3, 2).
- **LCD 16x2 I2C:** Endereço `0x27` (A4/A5).
- **Comunicação:** SoftwareSerial (RX:10, TX:11) conectado ao TX/RX da placa principal.

### Atalhos do Teclado
- `*` vira `:`
- `#` vira `,`
- `*` seguido de `#` atua como **ENTER** (Envia comando).
- `C` (Clear) limpa o buffer.
- `D` (Delete) apaga o último caractere.

## Contributing

Contribuições são bem-vindas. Siga os princípios:
- **AVR:** Zero *String* de dados, use interrupções atômicas.
- **Front-end:** Mantenha a paleta original (`navy`, `cream`, `orange`) e estrutura Tailwind.

1. Fork → 2. Feature Branch → 3. Commit → 4. Push → 5. Pull Request.

## License

MIT