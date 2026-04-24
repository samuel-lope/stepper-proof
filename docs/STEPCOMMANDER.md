# StepCommander

Interface física de controle operando em um Atmega328P independente via I2C e Teclado Matricial.

## Versões do Firmware

O projeto disponibiliza duas implementações distintas para o Commander, atendendo a diferentes necessidades de complexidade e performance.

### [V1] Original (`stepcommander/`)
Versão inicial focada em simplicidade e facilidade de leitura.
- **Arquitetura:** Sequencial (Polling).
- **Comunicação:** Utiliza `readStringUntil` (bloqueante).
- **Gerenciamento de Memória:** Uso intensivo de objetos `String`.
- **Ideal para:** Testes rápidos e aprendizado da lógica básica do protocolo H8P.

### [V2] Otimizada (`stepcommander-v2/`)
Versão refatorada para alta performance e estabilidade em ambientes de produção.
- **Arquitetura:** Multitarefa Cooperativa (Máquina de Estados baseada em `millis`).
- **Comunicação:** RX Não-bloqueante (caractere por caractere via buffer estático).
- **Gerenciamento de Memória:** Otimização severa de SRAM usando a macro `F()` (PROGMEM) e bitfields para flags.
- **Ideal para:** Operação contínua onde a responsividade do teclado é crítica durante a recepção de dados.

---

## Hardware Setup (Comum a ambas as versões)

### Configuração de Pinos
- **Teclado 4x4:** Linhas (5, 4, 3, 2), Colunas (9, 8, 7, 6).
- **LCD 16x2 I2C:** Endereço `0x27` (A4/A5).
- **Comunicação:** SoftwareSerial (RX:10, TX:11) conectado ao TX/RX da placa principal.
- **Opcional:** TM1638plus nos pinos A0 (STB), A1 (CLK), A2 (DIO).

---

## Features e Atalhos

### Atalhos do Teclado
- `*` vira `:`
- `#` vira `,`
- `*` seguido de `#` atua como **ENTER** (Envia comando).
- **Repetir Comando:** Pressionar `*` seguido de `#` com o buffer vazio reenviará o último comando disparado.
- `*` seguido de `C` limpa o buffer (Clear).
- `*` seguido de `D` apaga o último caractere (Backspace).
- `*` seguido de `A` insere sinal de menos `-`.

### Modos de Feedback
- **Display LCD**: Exibe a linha de comando (L1) e feedback traduzido do protocolo H8P (L2) com efeito marquee automático para mensagens longas.
- **Monitor Serial**: Ambas as versões ecoam logs de debug via porta USB nativa (9600 bps) para facilitar o diagnóstico sem necessidade do LCD.

## API Reference
O Commander se comunica com a placa principal usando o protocolo **H8P**. Consulte o [Guia de Integração](./INTEGRATION.md) para detalhes da sintaxe hexadecimal.
