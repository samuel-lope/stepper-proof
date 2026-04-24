# PLAN-avr-refactor

## Overview
O objetivo desta tarefa é reescrever o código `stepcommander-v2\stepcommander-v2.ino` para utilizar uma abordagem de mais baixo nível, mais próxima da arquitetura do microcontrolador AVR (Atmega 328P). O foco é garantir um código bem comentado, altamente estruturado e otimizado, mantendo a funcionalidade de hardware e comunicação H8P.

## ✅ Phase 0: Socratic Gate (Concluído)
As seguintes decisões arquiteturais foram firmadas para o projeto:

- **Q1 (Bibliotecas):** Manter as bibliotecas existentes (`LiquidCrystal_I2C`, `Keypad`, `SoftwareSerial`, `TM1638plus`).
- **Q2 (Estabilidade/ISRs):** Focar na estabilidade. Utilizaremos uma **Máquina de Estados baseada em millis()** (Cooperative Multitasking) para eliminar `delay()`, já que ISRs customizadas poderiam entrar em conflito com o `SoftwareSerial` (que usa PCINT) e o `Wire` (I2C). O "baixo nível" será aplicado na otimização de memória (flags bit a bit) e estrutura de código.
- **Q3 (Hardware):** Manter o `SoftwareSerial` nos pinos 10 e 11, respeitando a montagem física atual.

## Project Type
FIRMWARE / BACKEND (AVR C++)

## Success Criteria
- O código compila sem erros para o target Atmega 328P.
- A lógica utiliza registradores, máscaras de bits (`_BV()`) e interrupções onde houver benefício de performance.
- O código é modularizado e ricamente comentado (foco didático e estrutural).
- As bibliotecas mantidas são utilizadas sem bloqueios (`delay()`).

## Tech Stack
- C++ / AVR-GCC
- Timers e Interrupções do Atmega 328P
- Registradores de Entrada/Saída (`DDRx`, `PORTx`, `PINx`)

## File Structure
- `stepcommander-v2/stepcommander-v2.ino` (Refatorado em Abordagem Orientada a Eventos/Máquina de Estados)
- *(Possibilidade de extração de drivers `.h`/`.cpp` dependendo da resposta da Q1)*

## Task Breakdown

### Task 1: Definição Arquitetural
- **Agent:** `backend-specialist`
- **INPUT:** Suas respostas às perguntas acima.
- **OUTPUT:** Decisão sobre manutenção de bibliotecas e estratégia de uso de Timers.
- **VERIFY:** Aprovação antes do código.

### Task 2: Setup de Registradores e Hardware
- **Agent:** `backend-specialist`
- **INPUT:** Especificações do AVR Atmega 328P.
- **OUTPUT:** Bloco `setup()` reescrito manipulando diretamente registradores para GPIO e inicialização de Timers.
- **VERIFY:** O código compila no AVR-GCC sem erros de sintaxe.

### Task 3: Implementação ISRs e Loop Não-Bloqueante
- **Agent:** `backend-specialist`
- **INPUT:** Lógica de entrada/saída atual.
- **OUTPUT:** Máquina de estados onde o `loop()` lida apenas com flags marcadas pelas rotinas de interrupção (ISRs).
- **VERIFY:** LCD, Teclado e TM1638 funcionam concorrentemente.

### Task 4: Otimização do Protocolo H8P
- **Agent:** `backend-specialist`
- **INPUT:** Estrutura das mensagens H8P.
- **OUTPUT:** Parser do H8P integrado ao novo modelo não-bloqueante/USART.
- **VERIFY:** Comandos enviados pelo Keypad são processados sem perda de pacotes.

## Phase X: Verification
- [ ] Compilação: Verificar espaço na Flash e RAM após otimizações.
- [ ] Lint: O código segue padrões C++ limpos com nomes descritivos.
- [ ] Validação no Hardware: (Ação do Usuário) Flash na placa e teste prático de comunicação e displays.
