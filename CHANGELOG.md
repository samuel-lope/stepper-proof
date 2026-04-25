# Changelog

All notable changes to this project will be documented in this file.

## [1.7.0] - 2026-04-25
### Added
- **SoftwareSerial no Firmware Principal**: `stepcontrol.ino` agora escuta comandos em duas portas simultâneas — USB Serial (Web) e SoftwareSerial D10/D11 (Commander).
- **Roteamento Inteligente de Respostas**: O firmware detecta automaticamente a origem de cada comando (`SRC_USB` ou `SRC_COMMANDER`) e roteia respostas de forma diferenciada:
  - **USB**: Dados completos (`C0:0,10:1600,11:250,...`).
  - **Commander**: Formato otimizado (`C0:0`), compatível com o display LCD 16x2.
- **Broadcast de Eventos Globais**: Mudanças de estado (`B0`, `B1`, `B4`, `B5`, `B6`, `B7`, `B8`) são enviadas para ambas as interfaces simultaneamente.
- **Funções de Roteamento**: `enviarRespostaHex()`, `enviarRespostaParam()`, `enviarRespostaComando()`, `broadcastHex()`, `broadcastParam()` substituem todos os `Serial.print` diretos.

### Fixed
- **Motor Stuttering com SoftwareSerial**: Telemetria de alta frequência (`D0`, `C1`) agora é exclusiva da USB Serial. `SoftwareSerial.write()` desabilita interrupts globais durante TX (~1ms/byte a 9600 baud), o que causava bloqueio do `TIMER1_COMPA/B_vect` e stuttering visível nos pulsos dos motores durante operação contínua com `repeat=0`.
- **Mensagem LCD incorreta para C0**: O Commander exibia "Salvo: Slot X" ao receber `C0` (enfileiramento na SRAM), sugerindo falsamente uma gravação em EEPROM. Corrigido para "Fila: Linha X".

### Changed
- **Documentação Completa**: README, INTEGRATION.md, STEPCOMMANDER.md e llms.txt reescritos com diagramas de arquitetura dual-interface, tabelas de roteamento de respostas, e referência completa de feedback LCD.

## [1.6.0] - 2026-04-22
### Added
- **StepCommander**: Nova interface de hardware periférica rodando em Atmega328P.
- **Controle Físico**: Suporte a Teclado Matricial 4x4 com mapeamento customizado (`*` -> `:`, `#` -> `,`, `*`+`#` -> Enter).
- **Feedback Visual**: Integração com LCD 16x2 via I2C (`0x27`) com tradução de códigos H8P para mensagens amigáveis.
- **Comunicação Inter-Placas**: Implementação de link Serial via `SoftwareSerial` (10/11) para comando e telemetria remota.

## [Unreleased]
### Added
- **Comando `1C` (Save to EEPROM)**: Novo comando H8P para salvar diretamente uma linha da fila (SRAM) para a EEPROM. Sintaxe: `1C:eeprom_slot:sram_slot`.
- **Expansão de Memória**: O número máximo de slots na EEPROM (Presets) foi aumentado de 5 para 10 (slots 0 a 9).
- **Comando de Repetição (Commander)**: Pressionar `*` + `#` no teclado matricial com o buffer vazio reenvia automaticamente o último comando disparado para a placa principal.
- Documentação revisada seguindo os novos templates 2025.
- Comentários JSDoc abrangentes nas funções principais do frontend (`app.js` e `i18n.js`).
- Refatoração da Referência de API em `INTEGRATION.md` para formato de endpoints técnicos.

### Changed
- **Debug Serial no Commander**: A rotina de processamento `processIncomingMessage()` foi reescrita para exibir traduções via terminal do PC (ex: `-> Sis Inicializado`), prevenindo travamentos completos do barramento I2C caso o display LCD físico seja desconectado para testes de bancada.

### Fixed
- **Inversão de Matriz (Keypad)**: Corrigida transposição matemática de hardware invertendo os pinos de linhas (`5, 4, 3, 2`) e colunas (`9, 8, 7, 6`) do StepCommander, alinhando a lógica da biblioteca `Keypad.h` com o chicote físico real do componente genérico.

### Fixed
- Sincronização do limite máximo do *Jog Slider* no primeiro pareamento serial (M1/M2).
- Atualização dinâmica dos limites de *Jog* ao salvar ou ler configurações de preset.


## [1.5.1] - 2026-04-20

### Changed
- **Mapeamento de Hardware**: GPIO de Enable do Motor 1 alterado de **D8** para **D6**, otimizando a disposição física no Port D do ATmega328P.

### Fixed
- **Sanitização de Comandos**: Adicionada validação de ID de motor para os comandos `16` (Enable) e `17` (Disable), prevenindo execuções em IDs inexistentes.

### Added
### Added
- **Comando `1B` (Fast Action Override)**: Executa um preset da EEPROM substituindo sua quantidade de repetições originais. Sintaxe: `1B:slot:repeticoes` (ex: `1B:0:4`). Se o valor de repetições for negativo (ex: `1B:0:-40`), o sistema inverte automaticamente a direção original gravada na EEPROM enquanto executa a versão absoluta do número de repetições.
- **Code Documentation**: Inclusão de cabeçalhos profissionais JSDoc/Doxygen-style nas funções principais do firmware para melhor manutenibilidade.


## [1.5.0] - 2026-04-17

### Added
- **Modal de Configurações**: Novo painel central para personalização da experiência de usuário.
- **Dictionary Overrides**: Sistema que permite ao usuário substituir qualquer string ou termo técnico do firmware/interface em tempo real, com persistência automática.
- **Alert Visibility Control**: Configuração granular de visibilidade para cada tipo de alerta (toasts). Permite silenciar notificações específicas sem perder logs no histórico.
- **Auto-Save Persistence**: Implementação de ouvintes de evento (`input`/`change`) que salvam instantaneamente as preferências no `localStorage` (`stepper_custom_i18n` e `stepper_hidden_alerts`).

### Fixed
- **Mapeamento de Toasts**: Vinculação obrigatória de `messageKey` em todas as chamadas `showToast` da interface UI, garantindo que o filtro de visibilidade seja respeitado em 100% das mensagens.
- **Erro de Sintaxe Serial**: Removida tag redundante que causava erro de execução no parser de conexões.

---

### Added
- **Internacionalização (i18n)**: Engine de tradução dependency-free (`i18n.js`) com suporte a EN-US e PT-BR. Todas as strings da interface são carregadas dinamicamente via `data-i18n` attributes e função `t('key', params)`. Idioma salvo em `localStorage`.
- **Toggle de Idioma**: Switch EN/PT no header da interface com troca instantânea de todos os textos, incluindo tooltips e ARIA labels.
- **Comando `16:X` (Enable Motor)**: Novo comando do firmware que ativa o driver TB6600 do motor selecionado (EN → LOW). Sintaxe: `16:1` para M1, `16:2` para M2.
- **Comando `17:X` (Disable Motor)**: Novo comando que desativa o driver TB6600 (EN → HIGH, eixo livre). Sintaxe: `17:1` para M1, `17:2` para M2.
- **Comando `18:X` (fastAction)**: Novo comando que executa instantaneamente um dos 5 presets gravados na EEPROM (slots 0 a 4). Limpa a fila atual e injeta o preset para execução imediata.
- **EEPROM Presets (`19`/`1A`)**: Comandos para gerenciar a memória não volátil. `19:X` grava a linha atual no slot X da EEPROM; `1A:X` lê e retorna os parâmetros do slot X via Serial.
- **Inicialização de Presets**: Sistema de "Magic Byte" que detecta o primeiro boot e carrega automaticamente 5 presets de fábrica variados para M1 e M2 na EEPROM.
- **Toggle de Enable/Disable na UI**: Checkboxes integrados ao seletor de motor "Target Motor". Checkbox marcado = motor habilitado (16:X), desmarcado = desabilitado (17:X).
- **Feedback Visual na Telemetria**: O painel Live Telemetry agora exibe "Driver ON" (verde) ou "Driver OFF" (vermelho) em tempo real quando o estado do driver muda.

### Changed
- **Seletor de Motor refatorado**: Botões "Motor 1" / "Motor 2" agora incluem toggle switches laterais para controle do driver, eliminando a seção separada "Driver EN".
- **Evento `langchange`**: Custom event disparado ao trocar de idioma, permitindo que elementos dinâmicos (status de conexão, botão conectar) atualizem seus textos sem perder o estado atual.
- **Alinhamento e Layout**: Refatoração do layout da `main` no arquivo `index.html`. O grid primário foi isolado da altura de tela esticada para que a coluna da direita alinhe-se e distribua-se verticalmente baseada na proporção do painel principal ("New Movement Command", agora com altura fixa absoluta de 600px).

### Fixed
- **Alinhamento de Baud Rate**: Corrigido o baud rate padrão no componente `<select>` da interface web de 115200 para **9600 bps**, alinhando-o com a especificação do firmware e evitando falhas de conexão inicial para novos usuários.
- **Bug de estado ao trocar idioma**: Corrigido o problema onde o indicador de conexão e botão "Conectar" eram resetados para "OFFLINE" ao trocar de idioma, mesmo com hardware conectado. Elementos dinâmicos agora usam `langchange` event listener ao invés de `data-i18n`.
- **Bug de botões retraídos**: Corrigido o colapso dos botões "Motor 1"/"Motor 2" ao clicar na seleção. A classe `flex-1` estava ausente na string `base` da função `selectMotor()`.

---

## [1.3.1] - 2026-04-15

### Added
- **Código `B6` (Repeat OFF)**: Nova resposta do firmware ao receber `03:0`, confirmando que o loop infinito foi desativado.

### Changed
- **Comando `03` agora é booleano key:value**: `03:1` ativa e `03:0` desativa o loop infinito. Incompável com o formato antigo `03` sem valor (removido).
- **Botão Mestre de Loop é toggle visual**: O botão "Ativar RepeatAll Fila" não envia mais o comando ao clicar. O estado é transmitido (`03:1` ou `03:0`) automaticamente junto com o **EXECUTE ALL**.
- **Limpeza automática ao carregar sequência**: `loadTargetSequence` agora envia `02` (STOP) e reseta `currentQueue` antes de injetar uma sequência da biblioteca, prevenindo duplicidade de comandos na SRAM.

---

## [1.3.0] - 2026-04-15

### Added
- **Dual Motor Support**: Suporte simultâneo e independente a dois motores de passo usando dois drivers TB6600. Motor 1 (Dir=D2, Pul=D3, En=D8) e Motor 2 (Dir=D4, Pul=D5, En=D7).
- **Timer1 Dual-Channel Freerunning**: Refatoração completa do Timer1 para operar em modo Freerunning com dois canais independentes — `ISR(TIMER1_COMPA_vect)` para M1 e `ISR(TIMER1_COMPB_vect)` para M2. Cada canal calcula seu próprio `next_compare` sem interferência mútua.
- **Parâmetro `15` (Motor Alvo)**: Novo campo no protocolo H8P para selecionar o motor destino por comando. Ex: `10:1600,11:500,15:2`. Default retrocompatível: Motor 1.
- **`struct ComandoMotor` com `motor_id`**: Campo `uint8_t motor_id` adicionado à estrutura de fila para roteamento correto de cada slot ao pipeline correto na máquina de estados.
- **Interface Web — Seletor de Motor**: Command Builder atualizado com seletor visual de 2 botões (Motor 1 azul / Motor 2 teal) com estado ativo destacado.
- **Interface Web — Telemetria Dual**: Painel Live Telemetry expandido de 2 para 3 colunas (SRAM / Motor 1 / Motor 2), cada motor com estado independente.
- **Parser D0 Dual**: A interface web agora decodifica o formato `D0:MNN` (ex: `D0:101` = M1, slot 1; `D0:203` = M2, slot 3) e atualiza apenas o painel do motor correto.

### Fixed
- **Bug Crítico — Fila destruída antes do RUN**: A condição de cleanup global `!m1_executando && !m2_executando && qtd_comandos_na_fila > 0` era satisfeita a cada iteração de `loop()` após salvar um comando, destruindo a fila silenciosamente antes que o RUN fosse enviado. Corrigido com a flag `bool fila_iniciada` que só autoriza o cleanup após o comando `01` ter sido processado com sucesso.

### Changed
- **Baud Rate**: Alterado de 115200 para **9600 bps** para maior compatibilidade com hardware de teste.
- **Mensagens H8P atualizadas**: Toasts e `hexResponses` da interface web atualizados para refletir operações duais (ex: "Ambos motores destravados e RAM limpa").
- **Confirmação C0 com Motor**: A resposta `C0` agora inclui o campo `15:X` para identificar a qual motor o slot foi alocado. Log: `Slot N° X [MX] gravado na RAM`.

---

## [1.2.0] - 2026-04-14

### Added
- **Live Telemetry Interface**: Novo componente UI que exibe status de hardware em tempo real (uso de SRAM, linha ativa, contagens regressivas de delay) sem toasts para atualizações constantes.
- **H8P V2 Telemetry Extension**: Novos códigos Hex (`C1`, `D0`, `B3`) no firmware para stream de estado para o frontend.
- **UI/UX Overhaul**: Migração para Tailwind CSS com layout técnico-minimalista. Terminal estático substituído por sistema dinâmico de Toast notifications.
- **Micro-interactions**: Escala ativa nos botões e transições suaves para melhor experiência tátil.

### Changed
- **Parser Intelligence**: A interface web decodifica pacotes hexadecimais em dados legíveis (ex: código `C0` convertido em Slot ID e detalhes de passos).

---

## [1.1.0] - 2026-04-13

### Added
- **Safety Clamp**: Limite mínimo de 50µs para intervalos de pulso — previne *starvation* do MCU.
- **Atomic Operations**: Mudanças de estado críticas (STOP) envolvidas em blocos `cli()`/`sei()`.
- **Developer Environment**: Configurações `.vscode` (`arduino.json`, `c_cpp_properties.json`) e `.clangd` para IntelliSense no macOS.
- **AI-Friendly Docs**: `llms.txt` e estrutura de documentação para compreensão por agentes LLM.

### Fixed
- **Timer Race Conditions**: Manipulações de `TCCR1B` e limpeza de `TIFR1` tornadas atômicas.
- **IntelliSense Errors**: `#include <Arduino.h>` e forward declarations adicionados explicitamente.
- **Compiler Warnings**: Warnings de assignment resolvidos na lógica de parsing serial.

---

## [1.0.0] - 2026-04-12

### Added
- Lançamento inicial do sistema Stepper Control.
- Controle de alta precisão via Timer1 de 16 bits.
- Dashboard Web Serial API para gerenciamento de fila.
- Protocolo de comunicação hexadecimal H8P.
