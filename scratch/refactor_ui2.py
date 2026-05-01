import re

with open('stepcontrol-v2/stepcontrol-v2.ino', 'r') as f:
    content = f.read()

start_marker = "// ==========================================\n// TECLADO E INTERFACE (UI)\n// ==========================================\n"
end_marker = "// ==========================================\n// PARSER (Interpretador H8P Completo)\n// ==========================================\n"

new_ui_block = """// ==========================================
// TECLADO E INTERFACE (UI)
// ==========================================
void setUIMessage(const char *msg)
{
    strncpy(currentMsg, msg, MSG_MAX_LEN);
    currentMsg[MSG_MAX_LEN] = '\\0';
    systemFlags.isMessageLong = (strlen(currentMsg) > 16) ? 1 : 0;
    systemFlags.msgIsStatus = 1; // Temporária
    scrollIndexBottom = 0;
    lastMsgTime = millis();
    systemFlags.lcdNeedsUpdate = 1;
    Serial.println(msg);
}

char scanKeypadFast()
{
    char key = 0;
    PCICR &= ~(1 << PCIE2);
    for (uint8_t r = 0; r < 4; r++)
    {
        PORTK = ~(1 << r) | 0xF0;
        _delay_us(10);
        uint8_t cols = PINK >> 4;
        if (cols != 0x0F)
        {
            for (uint8_t c = 0; c < 4; c++)
            {
                if (!(cols & (1 << c)))
                {
                    key = matrixKeys[3 - r][3 - c];
                    break;
                }
            }
        }
        if (key)
            break;
    }
    PORTK = 0xF0;
    PCIFR |= (1 << PCIF2);
    PCICR |= (1 << PCIE2);
    return key;
}

void scrollTextToBuffer(char *out, const char *text, uint8_t textLen, uint16_t idx)
{
    const uint8_t GAP = 4;
    uint8_t totalLen = textLen + GAP;
    uint16_t start = idx % totalLen;

    for (uint8_t i = 0; i < 16; i++)
    {
        uint16_t pos = (start + i) % totalLen;
        out[i] = (pos < textLen) ? text[pos] : ' ';
    }
    out[16] = '\\0';
}

void updateLCD()
{
    if (systemFlags.menuActive)
    {
        char dispTop[17];
        char dispBot[17];
        if (systemFlags.menuSelection == 0)
        {
            snprintf_P(dispTop, 17, PSTR(">Executar Fila[%d]"), qtd_comandos_na_fila);
            strcpy_P(dispBot, PSTR(" Limpar Fila    "));
        }
        else
        {
            snprintf_P(dispTop, 17, PSTR(" Executar Fila[%d]"), qtd_comandos_na_fila);
            strcpy_P(dispBot, PSTR(">Limpar Fila    "));
        }
        lcd.setCursor(0, 0);
        lcd.print(dispTop);
        lcd.setCursor(0, 1);
        lcd.print(dispBot);
        return;
    }

    char dispTop[17];
    char dispBot[17];

    char prefix[11];
    uint8_t prefixLen;

    if (systemFlags.isFastActMode)
    {
        strcpy_P(prefix, PSTR("[F]Cmd:"));
        prefixLen = 7;
    }
    else if (qtd_comandos_na_fila > 0)
    {
        snprintf_P(prefix, sizeof(prefix), PSTR("[%d]Cmd:"), qtd_comandos_na_fila);
        prefixLen = strlen(prefix);
    }
    else
    {
        strcpy_P(prefix, PSTR("Cmd:"));
        prefixLen = 4;
    }

    uint8_t maxInLen = 16 - prefixLen;
    strncpy(dispTop, prefix, 16);
    dispTop[prefixLen] = '\\0';

    if (inputLen <= maxInLen)
    {
        strncat(dispTop, inputBuffer, maxInLen);
        uint8_t total = strlen(dispTop);
        while (total < 16)
            dispTop[total++] = ' ';
        dispTop[16] = '\\0';
    }
    else
    {
        strncat(dispTop, inputBuffer + (inputLen - maxInLen), maxInLen);
        dispTop[16] = '\\0';
    }

    uint8_t msgLen = strlen(currentMsg);
    if (msgLen <= 16)
    {
        strncpy(dispBot, currentMsg, 16);
        uint8_t total = msgLen;
        while (total < 16)
            dispBot[total++] = ' ';
        dispBot[16] = '\\0';
    }
    else
    {
        scrollTextToBuffer(dispBot, currentMsg, msgLen, scrollIndexBottom);
    }

    lcd.setCursor(0, 0);
    lcd.print(dispTop);
    lcd.setCursor(0, 1);
    lcd.print(dispBot);
}

void handleDisplayTasks()
{
    unsigned long now = millis();

    if (systemFlags.msgIsStatus && (now - lastMsgTime >= MSG_TIMEOUT_MS))
    {
        if (systemFlags.isFastActMode)
            strcpy_P(currentMsg, PSTR("[FAST ACT]"));
        else
            strcpy_P(currentMsg, PSTR("Pronto."));
            
        systemFlags.isMessageLong = 0;
        systemFlags.msgIsStatus = 0;
        scrollIndexBottom = 0;
        systemFlags.lcdNeedsUpdate = 1;
    }

    if (systemFlags.isMessageLong)
    {
        uint8_t msgLen = strlen(currentMsg);
        uint16_t interval = (uint16_t)max(200, SCROLL_INTERVAL_BASE - (int)(msgLen * 10));
        if (now - lastScrollTime >= interval)
        {
            lastScrollTime = now;
            scrollIndexBottom++;
            systemFlags.lcdNeedsUpdate = 1;
        }
    }

    if (systemFlags.lcdNeedsUpdate)
    {
        updateLCD();
        systemFlags.lcdNeedsUpdate = 0;
    }
}

void enterMenu()
{
    systemFlags.menuActive = 1;
    systemFlags.menuSelection = 0;
    systemFlags.lcdNeedsUpdate = 1;
}

void processMenuKey(char key)
{
    if (key == 'A' || key == 'B')
    {
        systemFlags.menuSelection = !systemFlags.menuSelection;
    }
    else if (key == '#')
    {
        systemFlags.menuActive = 0;
        if (systemFlags.menuSelection == 0)
        {
            char cmdBuf[10];
            strcpy(cmdBuf, "01");
            interpretarComando(cmdBuf);
        }
        else
        {
            qtd_comandos_na_fila = 0;
            setUIMessage("SRAM Limpa!");
        }
    }
    else if (key == '*')
    {
        systemFlags.menuActive = 0;
        setUIMessage("Menu Fechado");
    }
    systemFlags.lcdNeedsUpdate = 1;
}

ComandoMotor parseLineToMotorCommand(char *linha)
{
    // Parsea uma linha no formato H8P e retorna o struct correspondente.
    char temp[MAX_INPUT_LEN + 1];
    strncpy(temp, linha, MAX_INPUT_LEN);
    temp[MAX_INPUT_LEN] = '\\0';

    char *d = temp;
    do { while (*d == ' ') d++; } while ((*linha++ = *d++));
    linha = linha - (d - linha); // reset pointer, wait actually temp was modified? No, wait, temp is modified.
    
    // Safer parse:
    strncpy(temp, linha, MAX_INPUT_LEN);
    temp[MAX_INPUT_LEN] = '\\0';
    
    ComandoMotor cmd = {0, 0, 0, 1, 0, 1};
    char *ponteiro_virgula;
    char *par = strtok_r(temp, ",", &ponteiro_virgula);

    while (par != NULL)
    {
        char *ponteiro_dois_pontos;
        char *chave_str = strtok_r(par, ":", &ponteiro_dois_pontos);
        char *valor_str = strtok_r(NULL, ":", &ponteiro_dois_pontos);

        if (chave_str != NULL && valor_str != NULL)
        {
            uint8_t chave = (uint8_t)strtol(chave_str, NULL, 16);
            int32_t valor = atol(valor_str);

            switch (chave)
            {
            case 0x10: cmd.step = (uint32_t)valor; break;
            case 0x11: cmd.vel = (uint32_t)valor; break;
            case 0x12: cmd.dir = (uint8_t)valor; break;
            case 0x13: cmd.repeat = (uint16_t)valor; break;
            case 0x14: cmd.pause_ms = (uint32_t)valor; break;
            case 0x15: cmd.motor_id = (uint8_t)valor; break;
            }
        }
        par = strtok_r(NULL, ",", &ponteiro_virgula);
    }
    return cmd;
}

void processKeyInput(char key)
{
    if (systemFlags.menuActive)
    {
        processMenuKey(key);
        return;
    }

    if (systemFlags.isFastActMode && isDigit(key))
    {
        executarFastAction(key - '0');
        return;
    }

    if (saveComboState == 1)
    {
        if (key == 'C')
        {
            saveComboState = 2;
            systemFlags.lcdNeedsUpdate = 1;
            return;
        }
        else
        {
            saveComboState = 0;
            if (inputLen < MAX_INPUT_LEN)
            {
                inputBuffer[inputLen++] = '-';
                inputBuffer[inputLen] = '\\0';
            }
            else setUIMessage("LIMITE!");
        }
    }
    else if (saveComboState == 2)
    {
        if (isDigit(key))
        {
            saveComboState = 0;
            if (inputLen > 0)
            {
                ComandoMotor cmdToSave = parseLineToMotorCommand(inputBuffer);
                gravarPresetEEPROM(key - '0', cmdToSave);
                char msg[20];
                snprintf_P(msg, sizeof(msg), PSTR("Slot %d Salvo!"), key - '0');
                setUIMessage(msg);
                inputLen = 0;
                inputBuffer[0] = '\\0';
            }
            else setUIMessage("Buffer Vazio!");
            systemFlags.lcdNeedsUpdate = 1;
            return;
        }
        else saveComboState = 0;
    }

    bool hasStar = (inputLen > 0 && inputBuffer[inputLen - 1] == ':');

    if (hasStar)
    {
        if (key == '#')
        {
            inputBuffer[--inputLen] = '\\0';
            if (inputLen == 0 && lastCommand[0] != '\\0')
            {
                strncpy(inputBuffer, lastCommand, MAX_INPUT_LEN);
                inputLen = strlen(inputBuffer);
            }
            else if (inputLen > 0)
            {
                strncpy(lastCommand, inputBuffer, MAX_INPUT_LEN);
                lastCommand[MAX_INPUT_LEN] = '\\0';
            }

            if (inputLen > 0)
            {
                char cmdBuf[MAX_INPUT_LEN + 1];
                strncpy(cmdBuf, inputBuffer, MAX_INPUT_LEN);
                cmdBuf[MAX_INPUT_LEN] = '\\0';
                interpretarComando(cmdBuf);
            }
            inputLen = 0;
            inputBuffer[0] = '\\0';
        }
        else if (key == 'B')
        {
            inputBuffer[--inputLen] = '\\0';
            systemFlags.isFastActMode = !systemFlags.isFastActMode;
            setUIMessage(systemFlags.isFastActMode ? "FAST ACT ON" : "FAST ACT OFF");
        }
        else if (key == 'C')
        {
            inputLen = 0;
            inputBuffer[0] = '\\0';
        }
        else if (key == 'D')
        {
            if (inputLen >= 2)
                inputBuffer[inputLen -= 2] = '\\0';
            else
            {
                inputLen = 0;
                inputBuffer[0] = '\\0';
            }
        }
        else if (key == 'A')
        {
            inputBuffer[--inputLen] = '\\0';
            saveComboState = 1;
        }
        else if (key == '*')
        {
            if (inputLen < MAX_INPUT_LEN)
            {
                inputBuffer[inputLen++] = ':';
                inputBuffer[inputLen] = '\\0';
            }
            else setUIMessage("LIMITE!");
        }
        else if (isDigit(key))
        {
            if (inputLen < MAX_INPUT_LEN)
            {
                inputBuffer[inputLen++] = key;
                inputBuffer[inputLen] = '\\0';
            }
            else setUIMessage("LIMITE!");
        }
        else
        {
            if (inputLen < MAX_INPUT_LEN)
            {
                inputBuffer[inputLen++] = key;
                inputBuffer[inputLen] = '\\0';
            }
            else setUIMessage("LIMITE!");
        }
    }
    else
    {
        char toAppend = 0;
        if (key == '#') toAppend = ',';
        else if (key == '*') toAppend = ':';
        else toAppend = key;

        if (toAppend)
        {
            if (inputLen < MAX_INPUT_LEN)
            {
                inputBuffer[inputLen++] = toAppend;
                inputBuffer[inputLen] = '\\0';
            }
            else setUIMessage("LIMITE!");
        }
    }

    if (inputLen == 4 && inputBuffer[0] == ':' && inputBuffer[1] == '0' &&
        inputBuffer[2] == '0' && inputBuffer[3] == '0')
    {
        inputLen = 0;
        inputBuffer[0] = '\\0';
        enterMenu();
    }

    systemFlags.lcdNeedsUpdate = 1;
}

"""

start_idx = content.find(start_marker)
end_idx = content.find(end_marker)

if start_idx != -1 and end_idx != -1:
    new_content = content[:start_idx] + new_ui_block + content[end_idx:]
    with open('stepcontrol-v2/stepcontrol-v2.ino', 'w') as f:
        f.write(new_content)
    print("Replaced UI block successfully.")
else:
    print("Markers not found.")

