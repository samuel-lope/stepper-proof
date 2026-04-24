# StepCommander

Interface física de controle operando em um Atmega328P independente via I2C e Teclado Matricial.

## Quick Start

### Configuração de Pinos (Commander)
- **Teclado 4x4:** Linhas (5, 4, 3, 2), Colunas (9, 8, 7, 6).
- **LCD 16x2 I2C:** Endereço `0x27` (A4/A5).
- **Comunicação:** SoftwareSerial (RX:10, TX:11) conectado ao TX/RX da placa principal.

## Features

### Atalhos do Teclado
- `*` vira `:`
- `#` vira `,`
- `*` seguido de `#` atua como **ENTER** (Envia comando).
- **Repetir Comando:** Pressionar `*` seguido de `#` com o buffer vazio reenviará o último comando disparado.
- `C` (Clear) limpa o buffer.
- `D` (Delete) apaga o último caractere.

### Modos de Operação
- **Modo Display Físico**: Utiliza o LCD 16x2 I2C para exibir a linha de comando e feedback traduzido na linha 2 com efeito marquee.
- **Modo PC Monitor**: Para operação sem display físico, o Commander está configurado para ecoar feedbacks via Serial Monitor do PC, evitando travamentos do barramento I2C.

## API Reference
O Commander se comunica com a placa principal usando o protocolo **H8P**. Consulte o [Guia de Integração](./INTEGRATION.md) para detalhes da sintaxe hexadecimal.
