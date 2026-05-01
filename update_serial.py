import re

with open('stepcontrol-v2/stepcontrol-v2.ino', 'r') as f:
    lines = f.readlines()

new_lines = []
in_diag = False

for line in lines:
    if "DIAG:" in line or "DIAG-RUN:" in line:
        new_lines.append(line)
        continue

    # Skip replacing in EEPROM diagnostic function
    if "void diagnosticoEEPROM" in line:
        in_diag = True
    if in_diag and line.strip() == "}":
        in_diag = False
        
    if in_diag:
        new_lines.append(line)
        continue

    # Replace Serial.print -> broadcast
    # Replace Serial.println -> broadcastln
    
    # regex to handle base/format argument like Serial.print(val, HEX)
    line = re.sub(r'Serial\.print\((.*?),\s*(.*?)\);', r'broadcast2(\1, \2);', line)
    line = re.sub(r'Serial\.println\((.*?),\s*(.*?)\);', r'broadcastln2(\1, \2);', line)
    
    line = line.replace('Serial.print(', 'broadcast(')
    line = line.replace('Serial.println(', 'broadcastln(')

    new_lines.append(line)

# Add macro definitions at the top
macros = """
// ==========================================
// BROADCAST MACROS (Dual Serial)
// ==========================================
#define broadcast(x) { Serial.print(x); Serial1.print(x); }
#define broadcastln(x) { Serial.println(x); Serial1.println(x); }
#define broadcast2(x, y) { Serial.print(x, y); Serial1.print(x, y); }
#define broadcastln2(x, y) { Serial.println(x, y); Serial1.println(x, y); }

"""

for i, line in enumerate(new_lines):
    if "DEFINIÇÕES DE PINOS DOS MOTORES" in line:
        new_lines.insert(i, macros)
        break

with open('stepcontrol-v2/stepcontrol-v2.ino', 'w') as f:
    f.writelines(new_lines)
