with open('stepcontrol-v2/stepcontrol-v2.ino', 'r') as f:
    content = f.read()

new_doc = """ * =================================================================================
 * MAPEAMENTO DE FUNÇÕES DO COMMANDER (TECLADO / UI)
 * =================================================================================
 * [IMPLEMENTADO] * + #          -> Enter (Enviar Comando da Linha)
 * [IMPLEMENTADO] * + C          -> Apagar Tudo (Clear Buffer)
 * [IMPLEMENTADO] * + D          -> Apagar Último (Backspace)
 * [IMPLEMENTADO] * + A          -> Inserir Sinal de Menos (-)
 * [IMPLEMENTADO] * + B          -> Alternar Modo Fast Act (Execução Direta de Macros)
 * [IMPLEMENTADO] * + A + C + N  -> Gravar Comando Atual na EEPROM (Slot N) - Opção 1
 * [IMPLEMENTADO] A + N          -> Gravar Comando Atual na EEPROM (Slot N) - Opção 2
 * [IMPLEMENTADO] * + 000        -> Abrir Menu Interativo (SRAM Queue)
 * [IMPLEMENTADO] Teclas no Menu -> A/B (Navegar), # (Confirmar), * (Cancelar)
 * [IMPLEMENTADO] Num Fast Act   -> Teclas 0-9 executam slots EEPROM imediatamente
 * [IMPLEMENTADO] UI Multitarefa -> Scroll Adaptativo de textos e Timeout de 8s para Alertas
 * =================================================================================
 */"""

content = content.replace(" * =================================================================================\n */", new_doc)

with open('stepcontrol-v2/stepcontrol-v2.ino', 'w') as f:
    f.write(content)

print("Documentation added.")
