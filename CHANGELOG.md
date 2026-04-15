# Changelog

All notable changes to this project will be documented in this file.

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
