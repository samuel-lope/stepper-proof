import re

with open('stepcontrol-v2/stepcontrol-v2.ino', 'r') as f:
    content = f.read()

# Replace String concats in interpretarComando
content = content.replace('setUIMessage("B2: Pausa " + String(valor) + "ms");', 'char msg[20]; snprintf_P(msg, sizeof(msg), PSTR("B2: Pausa %ldms"), valor); setUIMessage(msg);')
content = content.replace('setUIMessage("B7: M" + String(valor) + " Habilitado");', 'char msg[25]; snprintf_P(msg, sizeof(msg), PSTR("B7: M%ld Habilitado"), valor); setUIMessage(msg);')
content = content.replace('setUIMessage("B8: M" + String(valor) + " Desabilt.");', 'char msg[25]; snprintf_P(msg, sizeof(msg), PSTR("B8: M%ld Desabilt."), valor); setUIMessage(msg);')
content = content.replace('setUIMessage("BA: L" + String(valor) + " S:" + String(p.step) + " V:" + String(p.vel));', 'char msg[25]; snprintf_P(msg, sizeof(msg), PSTR("BA: L%ld S:%lu V:%lu"), valor, p.step, p.vel); setUIMessage(msg);')
content = content.replace('setUIMessage("C0: Slot " + String(idx_eeprom) + " Salvo");', 'char msg[25]; snprintf_P(msg, sizeof(msg), PSTR("C0: Slot %d Salvo"), idx_eeprom); setUIMessage(msg);')
content = content.replace('setUIMessage("B9: Preset " + String(valor) + " Gravado");', 'char msg[25]; snprintf_P(msg, sizeof(msg), PSTR("B9: Preset %ld Gravado"), valor); setUIMessage(msg);')
content = content.replace('setUIMessage("C1: Slot " + String(qtd_comandos_na_fila));', 'char msg[20]; snprintf_P(msg, sizeof(msg), PSTR("C1: Slot %d"), qtd_comandos_na_fila); setUIMessage(msg);')

# Replace update in carregarProximoComando String
content = content.replace('Serial.println("D0: M1 L" + String(m1_indice_atual));', 'Serial.print("D0: M1 L"); Serial.println(m1_indice_atual);')
content = content.replace('Serial.println("D0: M2 L" + String(m2_indice_atual));', 'Serial.print("D0: M2 L"); Serial.println(m2_indice_atual);')

# Replace fast actions String
content = content.replace('setUIMessage("BB: FastAct " + String(idx));', 'char msg[20]; snprintf_P(msg, sizeof(msg), PSTR("BB: FastAct %d"), idx); setUIMessage(msg);')
content = content.replace('setUIMessage("BC: FastActRep " + String(idx));', 'char msg[25]; snprintf_P(msg, sizeof(msg), PSTR("BC: FastActRep %d"), idx); setUIMessage(msg);')

# Loop update
old_loop = """void loop()
{
    if (keyPressFlag)
    {
        _delay_ms(20);
        char key = scanKeypadFast();
        if (key)
        {
            processKeyInput(key);
            while ((PINK >> 4) != 0x0F)
                ;
            _delay_ms(20);
        }
        keyPressFlag = false;
    }

    handleScroll();
    manageSteppers();
}"""

new_loop = """void diagnosticoEEPROM()
{
    Serial.println(F("--- EEPROM Slots ---"));
    for (uint8_t s = 0; s < MAX_PRESETS; s++)
    {
        ComandoMotor cmd = lerPresetEEPROM(s);
        Serial.print(F("Slot "));
        Serial.print(s);
        Serial.print(F(": "));
        if (cmd.step == 0 && cmd.vel == 0)
            Serial.println(F("[Vazio]"));
        else
        {
            Serial.print(F("S:"));
            Serial.print(cmd.step);
            Serial.print(F(" V:"));
            Serial.print(cmd.vel);
            Serial.print(F(" D:"));
            Serial.print(cmd.dir);
            Serial.print(F(" R:"));
            Serial.print(cmd.repeat);
            Serial.print(F(" P:"));
            Serial.print(cmd.pause_ms);
            Serial.print(F(" M:"));
            Serial.println(cmd.motor_id);
        }
    }
    Serial.println(F("--------------------"));
}

void loop()
{
    if (keyPressFlag)
    {
        _delay_ms(20);
        char key = scanKeypadFast();
        if (key)
        {
            processKeyInput(key);
            while ((PINK >> 4) != 0x0F)
                ;
            _delay_ms(20);
        }
        keyPressFlag = false;
    }

    handleDisplayTasks();
    manageSteppers();
}"""

content = content.replace(old_loop, new_loop)

# Remove old handleScroll
start_handle_scroll = content.find("void handleScroll()")
end_handle_scroll = content.find("void processKeyInput(char key)")
if start_handle_scroll != -1 and end_handle_scroll != -1:
    content = content[:start_handle_scroll] + content[end_handle_scroll:]

with open('stepcontrol-v2/stepcontrol-v2.ino', 'w') as f:
    f.write(content)

print("Parser and Loop updated.")
