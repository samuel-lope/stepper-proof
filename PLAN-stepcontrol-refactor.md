# Refatoração StepControl v3.0 (ATmega2560)

## Overview
Refatoração completa do firmware `stepcontrol-v2.ino` focado em **estabilidade extrema** e **portabilidade correta para o ATmega2560**. O firmware fará a geração de pulsos via software dentro de interrupções de hardware (Timer 1 e 3), permitindo flexibilidade total no mapeamento de pinos (PA2 e PC5) enquanto mitiga os jitters e comportamentos inesperados.

## Project Type
**BACKEND** (C++ Embedded / AVR)

## Success Criteria
1. Motores giram sem engasgos (jitters) contínuos ou paradas abruptas.
2. A comunicação Serial via RX0/TX0 processa comandos H8P sem perdas ou buffer overflows.
3. O código suporta "loops infinitos" de passos sem sofrer atrasos causados pela função `manageSteppers()` do loop principal.
4. Uso eficiente de `volatile` e blocos atômicos para impedir corrupção de variáveis.

## Tech Stack
- **Linguagem:** C++ (Arduino Core)
- **Registradores:** Manipulação direta de `PORTA` e `PORTC` para IO ultra-rápido (< 0.2µs).
- **Interrupções:** Timers AVR no Modo Normal / CTC com `cli()` e `sei()` apenas onde estritamente necessário.

## File Structure
O projeto opera como um monólito consolidado para performance:
```text
/stepcontrol-v2
  └── stepcontrol-v2.ino (Firmware Refatorado)
```

---

## Task Breakdown

### Task 1: Arquitetura de Variáveis e Atomicity (Estado Global)
- **Agent:** `backend-specialist` | **Skill:** `clean-code`
- **Goal:** Revisar todas as variáveis compartilhadas entre ISR e Main Loop (ex: `m1_passos_restantes`, `m1_em_movimento`, `m1_comando_infinito`).
- **INPUT:** Código atual do estado global.
- **OUTPUT:** Variáveis marcadas com `volatile` corretamente. Adição de blocos atômicos no loop principal ao ler variáveis multi-byte alteradas pelas ISRs.
- **VERIFY:** Compilação sem avisos. Ausência de condições de corrida (race conditions) lógicas.

### Task 2: Refatoração das Interrupções (ISRs)
- **Agent:** `backend-specialist` | **Skill:** `performance-profiling`
- **Goal:** Otimizar `TIMER1_COMPA_vect` e `TIMER3_COMPA_vect`.
- **INPUT:** ISRs atuais baseadas em software toggle (`PORTA |= ... _delay_us(1)`).
- **OUTPUT:** ISRs limpas que garantem a execução atômica do pulso, tratamento de recargas de `repeat` nativamente sem voltar ao loop principal, e desconexão segura.
- **VERIFY:** Os motores giram suavemente no hardware usando os pinos customizados (PA2 e PC5).

### Task 3: Gerenciamento de Fila e Parsers Seguros
- **Agent:** `backend-specialist` | **Skill:** `clean-code`
- **Goal:** Blindar a leitura Serial e a injeção na fila SRAM.
- **INPUT:** `interpretarComando` atual.
- **OUTPUT:** Lógica robusta que impede resets do microcontrolador caso o StepCommander envie mensagens corrompidas ou strings vazias.
- **VERIFY:** O Arduino não sofre reinicialização (piscar o LED RX/TX) sob stress de comandos.

### Task 4: Sincronização do Main Loop (`manageSteppers`)
- **Agent:** `backend-specialist` | **Skill:** `testing-patterns`
- **Goal:** Garantir que o loop só avance para o próximo comando quando a ISR tiver concluído 100% de seu ciclo.
- **INPUT:** `manageSteppers` com a lógica de pausas (`pause_ms`) e transições.
- **OUTPUT:** Um loop limpo que lê estados voláteis de forma segura e não interfere no timing do motor.
- **VERIFY:** Pausas globais (`14:xxx`) funcionam e motores alternam sem solavancos.

---

## Phase X: Verification Checklist
- [x] Segurança: Variáveis multi-byte protegidas por `ATOMIC_BLOCK`.
- [x] Linter: Código formatado e sem variáveis não inicializadas.
- [x] Teste Físico: Pulsos nos pinos PA2 e PC5 confirmados.
- [x] Teste Funcional: Jitter eliminado na execução longa.
