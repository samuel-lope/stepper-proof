        // Web Serial API Params
        let port;
        let reader;
        let writer;
        let keepReading = true;
        let lastCommandSent = "";
        let currentQueue = []; // Persistência temporária da sessão atual

        // UI Element Hooks
        const btnConnect = document.getElementById('btn-connect');
        const statusBadge = document.getElementById('conn-status');
        const statusDot = document.getElementById('status-dot');
        const baudRateSelect = document.getElementById('baud-rate');
        
        const controls = [
            'btn-run', 'btn-stop', 'btn-run-one',
            'btn-set-pause', 'btn-repeat-all'
        ].map(id => document.getElementById(id));

        // Dicionário do Arduino
        const hexResponses = {
            "A0": { text: "🔗 [AVR Status]: Hardware pareado. Timer1 Dual-Channel sincronizado a 16MHz.", type: "success" },
            "B0": { text: "⚙️ [Execução]: Fila mestre iniciada. Despachando pulsos independentes para M1 e M2.", type: "info" },
            "B1": { text: "🛡️ [Emergency]: Interrupções cortadas atomicamente. Ambos motores destravados e RAM limpa.", type: "warning" },
            "B4": { text: "🔁 [Force Loop]: O firmware repetirá esta fila de movimentos em laço infinito para ambos.", type: "info" },
            "B5": { text: "✅ [Finalizado]: Todos os motores concluíram a fila de instruções. Standby.", type: "success" },
            "B6": { text: "⏹️ [Loop OFF]: Modo RepeatAll desativado. A fila encerrará ao final da execução.", type: "info" },
            "E0": { text: "🚫 [Operação Negada]: Os motores já estão executando. Aguarde ou dispare um STOP.", type: "error" },
            "E1": { text: "⚠️ [Buffer Vazio]: A fila do MCU está ociosa. Injete um pacote de comandos antes de dar RUN.", type: "error" },
            "E2": { text: "🔴 [Overflow]: A SRAM atingiu o limite seguro (20 slots). Dispare um STOP (02) para limpar o buffer de memória.", type: "error" },
            "E3": { text: "🛑 [Safety Clamp Ativo]: Rejeitado! O intervalo inserido causará congelamento. Minimum = 50µs.", type: "error" }
        };

        // UI-UX PRO MAX: Sistema de Notificações Não-intrusivas (Substituição do Terminal CSS)
        function showToast(message, type = 'info') {
            const container = document.getElementById('toast-container');
            const toast = document.createElement('div');
            
            let icon = '';
            let colors = '';
            let duration = 4000;
            
            if (type === 'success') {
                colors = 'bg-slate-800 border-l-4 border-emerald-500 shadow-lg shadow-emerald-500/10';
                icon = `<svg aria-hidden="true" class="w-5 h-5 text-emerald-400" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M5 13l4 4L19 7"></path></svg>`;
            } else if (type === 'error') {
                colors = 'bg-rose-50 border-l-4 border-rose-500 shadow-lg shadow-rose-500/10 text-rose-800';
                icon = `<svg aria-hidden="true" class="w-5 h-5 text-rose-500" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M12 8v4m0 4h.01M21 12a9 9 0 11-18 0 9 9 0 0118 0z"></path></svg>`;
                duration = 8000; // Erros ganham permanência
            } else if (type === 'warning') {
                colors = 'bg-amber-50 border-l-4 border-amber-500 shadow-lg shadow-amber-500/10 text-amber-800';
                icon = `<svg aria-hidden="true" class="w-5 h-5 text-amber-500" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M12 9v2m0 4h.01m-6.938 4h13.856c1.54 0 2.502-1.667 1.732-3L13.732 4c-.77-1.333-2.694-1.333-3.464 0L3.34 16c-.77 1.333.192 3 1.732 3z"></path></svg>`;
            } else if (type === 'out') {
                colors = 'bg-slate-800 text-slate-300 shadow-lg border border-slate-700/50';
                icon = `<svg aria-hidden="true" class="w-4 h-4 text-slate-400" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M13 5l7 7-7 7M5 5l7 7-7 7"></path></svg>`;
                duration = 3000;
            } else {
                colors = 'bg-slate-800 text-slate-100 shadow-lg shadow-blue-500/10 border-l-4 border-blue-500';
                icon = `<svg aria-hidden="true" class="w-5 h-5 text-blue-400" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M13 16h-1v-4h-1m1-4h.01M21 12a9 9 0 11-18 0 9 9 0 0118 0z"></path></svg>`;
            }

            toast.className = `toast-enter pointer-events-auto rounded-xl p-4 font-mono text-sm flex items-start gap-3 backdrop-blur-md max-w-sm w-full ${colors}`;
            
            let textStyles = type === 'out' ? 'font-mono text-[13px] opacity-80' : 'font-sans font-medium text-[14px]';
            
            toast.innerHTML = `
                <div class="flex-shrink-0 mt-0.5">${icon}</div>
                <div class="flex-1 pt-0.5">
                    <p class="${textStyles} leading-tight">${message}</p>
                </div>
                <button class="flex-shrink-0 ml-4 hover:opacity-75 transition-opacity outline-none focus-visible:ring-2 focus-visible:ring-current focus-visible:outline-none" onclick="this.parentElement.remove()" aria-label="Close">
                    <svg aria-hidden="true" class="w-4 h-4 ${type==='error' || type==='warning' ? 'text-current' : 'text-slate-400'}" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M6 18L18 6M6 6l12 12"></path></svg>
                </button>
            `;

            container.appendChild(toast);

            setTimeout(() => {
                if(toast.parentElement) {
                    toast.classList.replace('toast-enter', 'toast-leave');
                    toast.addEventListener('animationend', () => toast.remove());
                }
            }, duration);
        }

        // --- MANUSEIO DA SERIAL ---
        btnConnect.addEventListener('click', async () => {
            if (port) await disconnectSerial();
            else await connectSerial();
        });

        async function connectSerial() {
            try {
                port = await navigator.serial.requestPort();
                const selectedBaudRate = parseInt(baudRateSelect.value);
                await port.open({ baudRate: selectedBaudRate });

                updateUIState(true);
                showToast(`Hardware Serial pareado (${selectedBaudRate} bps).`, "success");

                keepReading = true;
                readLoop();
            } catch (error) {
                console.error('Erro de conexão:', error);
                showToast(`Falha WebSerial: ${error.message}`, "error");
            }
        }

        async function disconnectSerial() {
            keepReading = false;
            if (reader) await reader.cancel();
            if (port) {
                await port.close();
                port = null;
            }
            updateUIState(false);
            showToast("Placa Arduino desconectada com segurança.", "warning");
        }

        function updateUIState(connected) {
            controls.forEach(btn => btn.disabled = !connected);
            baudRateSelect.disabled = connected;
            
            if (connected) {
                btnConnect.textContent = "Desconectar";
                btnConnect.classList.replace("bg-blue-600", "bg-rose-50");
                btnConnect.classList.replace("hover:bg-blue-700", "hover:bg-rose-100");
                btnConnect.classList.replace("text-white", "text-rose-600");
                
                statusBadge.textContent = "Conectado";
                statusBadge.classList.replace("text-slate-500", "text-emerald-600");
                
                statusDot.classList.replace("bg-rose-500", "bg-emerald-500");
                statusDot.classList.replace("shadow-[0_0_8px_rgba(244,63,94,0.6)]", "shadow-[0_0_8px_rgba(16,185,129,0.7)]");
                
                document.getElementById('btn-run-one').disabled = lastCommandSent === "";
            } else {
                btnConnect.textContent = "Conectar Hardware";
                btnConnect.classList.replace("bg-rose-50", "bg-blue-600");
                btnConnect.classList.replace("hover:bg-rose-100", "hover:bg-blue-700");
                btnConnect.classList.replace("text-rose-600", "text-white");
                
                statusBadge.textContent = "Offline";
                statusBadge.classList.replace("text-emerald-600", "text-slate-500");
                
                statusDot.classList.replace("bg-emerald-500", "bg-rose-500");
                statusDot.classList.replace("shadow-[0_0_8px_rgba(16,185,129,0.7)]", "shadow-[0_0_8px_rgba(244,63,94,0.6)]");
            }
        }

        function translateIncomingHex(line) {
            line = line.trim();
            if (!line) return null;

            // --- PARSER DE TELEMETRIA SILENCIOSA (Dual Motor) ---
            if (line.startsWith("C1:")) { // RAM Load Update
                document.getElementById('tel-queue').textContent = line.substring(3);
                return null;
            }
            if (line.startsWith("D0:")) { // Active Line Update — formato: D0:1XX (M1) ou D0:2XX (M2)
                const rawSlot = parseInt(line.substring(3));
                const motorNum = Math.floor(rawSlot / 100); // 1 ou 2
                const slotIdx = rawSlot % 100;
                const stId = motorNum === 2 ? 'tel-state-m2' : 'tel-state-m1';
                const st = document.getElementById(stId);
                if (st) {
                    st.textContent = `Exec L[${slotIdx}]`;
                    st.className = "text-[12px] font-mono text-cyan-400 font-semibold truncate";
                }
                return null;
            }
            if (line.startsWith("B3:")) { // Pause Update (afeta ambos visualmente)
                const pTime = line.substring(3);
                ['tel-state-m1', 'tel-state-m2'].forEach(id => {
                    const st = document.getElementById(id);
                    if (st && st.textContent.startsWith('Exec')) {
                        st.textContent = `Delay ${pTime}ms`;
                        st.className = "text-[12px] font-mono text-amber-400 animate-pulse truncate";
                    }
                });
                return null; 
            }

            // Fallback Responses Visuais (Toasts)
            if (hexResponses[line]) {
                if(line === "B1" || line === "B5" || line === "A0") {
                    ['tel-state-m1', 'tel-state-m2'].forEach(id => {
                        const st = document.getElementById(id);
                        if (st) {
                            st.textContent = "Standby";
                            st.className = "text-[12px] font-mono text-slate-400 truncate";
                        }
                    });
                    document.getElementById('tel-queue').textContent = "0";
                }
                return hexResponses[line];
            }

            if (line.startsWith("B2:")) 
                return { text: `⏳ [Delay Handler]: Modificador Global de Pausas injetado para ${line.substring(3)}ms fixos.`, type: "info" };

            if (line.startsWith("C0:")) {
                const parts = line.split(',');
                let idx = parts[0].split(':')[1];
                let details = [];
                let motorLabel = 'M1';
                for (let i = 1; i < parts.length; i++) {
                    const [key, val] = parts[i].split(':');
                    if (key === "10") details.push(`${val} Passos`);
                    if (key === "11") details.push(`${val}µs int.`);
                    if (key === "12") details.push(val==='1'?'[FWD]':'[REV]');
                    if (key === "15") motorLabel = val === '2' ? 'M2' : 'M1';
                }
                return { text: `💾 [Alocado]: Slot N° ${idx} [${motorLabel}] gravado na RAM -> (${details.join(' | ')})`, type: "success" };
            }

            return { text: `RAW_HW > ${line}`, type: "info" };
        }

        async function readLoop() {
            let lineBuffer = "";
            while (port && port.readable && keepReading) {
                const textDecoder = new TextDecoderStream();
                const readableStreamClosed = port.readable.pipeTo(textDecoder.writable);
                reader = textDecoder.readable.getReader();

                try {
                    while (true) {
                        const { value, done } = await reader.read();
                        if (done) break;
                        if (value) {
                            lineBuffer += value;
                            let newlineIndex;
                            while ((newlineIndex = lineBuffer.indexOf('\n')) >= 0) {
                                let completeLine = lineBuffer.slice(0, newlineIndex).replace('\r', '');
                                lineBuffer = lineBuffer.slice(newlineIndex + 1);

                                if (completeLine) {
                                    let translated = translateIncomingHex(completeLine);
                                    if(translated) showToast(translated.text, translated.type);
                                }
                            }
                        }
                    }
                } catch (error) {
                    console.error("Reader Halt:", error);
                } finally {
                    reader.releaseLock();
                }
            }
        }

        async function sendRawString(commandString) {
            if (!port || !port.writable) return false;
            const textEncoder = new TextEncoderStream();
            const writableStreamClosed = textEncoder.readable.pipeTo(port.writable);
            writer = textEncoder.writable.getWriter();

            await writer.write(commandString + "\n");
            await writer.close();

            // Decoradores de output para UI
            let displayCmd = commandString;
            if(commandString === "01") displayCmd = "Injetado Sinal: 01 [EXECUTAR FILA INTEIRA]";
            else if(commandString === "02") displayCmd = "Injetado Sinal: 02 [CORTE DE EMERGÊNCIA]";
            else if(commandString === "03") displayCmd = "Injetado Sinal: 03 [FECHAR CIRCUITO EM LOOP]";
            else if(commandString.startsWith("04:")) displayCmd = commandString.replace("04:", "Injetado Sinal: 04 [SUBRESCREVER PAUSA GLOBAL -> ") + "ms]";
            else if(commandString.startsWith("10:")) {
                let parts = commandString.split(",");
                displayCmd = `Enviando pacote Hex H8P… [${parts[0]}, ${parts[1]}]`;
            }
            showToast(`TX > ${displayCmd}`, "out");
            return true;
        }

        async function sendCommand(commandString) {
            if (await sendRawString(commandString)) {
                await new Promise(r => setTimeout(r, 60)); // Microdelay de tráfego
            } else {
                showToast("Conexão I/O ausente.", "error");
            }
        }

        // --- ENGENHARIA DE COMANDOS ---
        let selectedMotor = 1;

        function selectMotor(num) {
            selectedMotor = num;
            document.getElementById('inp-motor').value = num;
            
            const btn1 = document.getElementById('btn-motor-1');
            const btn2 = document.getElementById('btn-motor-2');
            
            if (num === 1) {
                btn1.className = 'motor-selector active-motor flex items-center justify-center gap-2 py-2.5 rounded-lg border-2 border-blue-500 bg-blue-50 text-blue-700 font-semibold text-sm transition-all cursor-pointer shadow-sm focus-visible:ring-2 focus-visible:ring-blue-500 outline-none';
                btn2.className = 'motor-selector flex items-center justify-center gap-2 py-2.5 rounded-lg border-2 border-slate-200 bg-white text-slate-500 font-semibold text-sm transition-all cursor-pointer shadow-sm hover:border-slate-300 hover:bg-slate-50 focus-visible:ring-2 focus-visible:ring-blue-500 outline-none';
            } else {
                btn2.className = 'motor-selector active-motor flex items-center justify-center gap-2 py-2.5 rounded-lg border-2 border-teal-500 bg-teal-50 text-teal-700 font-semibold text-sm transition-all cursor-pointer shadow-sm focus-visible:ring-2 focus-visible:ring-teal-500 outline-none';
                btn1.className = 'motor-selector flex items-center justify-center gap-2 py-2.5 rounded-lg border-2 border-slate-200 bg-white text-slate-500 font-semibold text-sm transition-all cursor-pointer shadow-sm hover:border-slate-300 hover:bg-slate-50 focus-visible:ring-2 focus-visible:ring-blue-500 outline-none';
            }
        }

        function buildCommandFromInputs() {
            const step = document.getElementById('inp-step').value;
            const vel = document.getElementById('inp-vel').value;
            const dir = document.getElementById('inp-dir').value;
            const repeat = document.getElementById('inp-repeat').value;
            const pause = document.getElementById('inp-pause').value;
            const motor = document.getElementById('inp-motor').value;

            if (!step || !vel || vel < 50) {
                showToast("Erro: O firmware rejeitará. Velocity mínimo = 50µs.", "error");
                const velInp = document.getElementById('inp-vel');
                velInp.classList.add('border-rose-500', 'ring-rose-500', 'ring-2');
                setTimeout(() => velInp.classList.remove('border-rose-500', 'ring-rose-500', 'ring-2'), 2500);
                return null;
            }

            let cmd = `10:${step},11:${vel},12:${dir},13:${repeat}`;
            if (pause && pause.trim() !== "") cmd += `,14:${pause}`;
            cmd += `,15:${motor}`;
            return cmd;
        }

        // Triggers UI
        document.getElementById('btn-add-cmd').addEventListener('click', () => {
            const cmd = buildCommandFromInputs();
            if (cmd) {
                lastCommandSent = cmd;
                currentQueue.push(cmd);
                updateQueueButtons();
                if (port) {
                    document.getElementById('btn-run-one').disabled = false;
                    sendCommand(cmd);
                } else {
                    showToast("Passo adicionado à fila local (Offline)", "success");
                }
            }
        });

        function updateQueueButtons() {
            const saveBtn = document.getElementById('btn-save-queue');
            const clearBtn = document.getElementById('btn-clear-queue');
            saveBtn.disabled = currentQueue.length === 0;
            clearBtn.disabled = currentQueue.length === 0;
        }

        document.getElementById('btn-clear-queue').addEventListener('click', () => {
            currentQueue = [];
            updateQueueButtons();
            showToast("Fila da sessão limpa localmente.", "info");
        });

        document.getElementById('btn-save-queue').addEventListener('click', () => {
            const name = prompt("Dê um nome para esta sequência:", `Sequência ${new Date().toLocaleTimeString('pt-BR')}`);
            if (name === null) return; // Cancelado

            const finalName = name.trim() === "" ? `Seq_${Date.now()}` : name;
            saveSequence(finalName, [...currentQueue]);
        });

        // --- SISTEMA DE BIBLIOTECA (LOCAL STORAGE) ---
        function saveSequence(name, commands) {
            let library = JSON.parse(localStorage.getItem('stepper_library') || '[]');
            const newSeq = {
                id: 'seq_' + Date.now(),
                name: name,
                commands: commands,
                date: new Date().toISOString()
            };
            library.unshift(newSeq);
            localStorage.setItem('stepper_library', JSON.stringify(library));
            showToast(`Sequência "${name}" salva com sucesso!`, "success");
        }

        function deleteSequence(id) {
            let library = JSON.parse(localStorage.getItem('stepper_library') || '[]');
            library = library.filter(s => s.id !== id);
            localStorage.setItem('stepper_library', JSON.stringify(library));
            renderLibrary();
            showToast("Sequência removida.", "warning");
        }

        let openAccordionId = null; // Estado UX para manter accordion aberto

        function toggleAccordion(id, event) {
            // Se clicar nos botões de ação ou inputs, não fecha/abre
            if (event && (event.target.closest('button') || event.target.tagName === 'INPUT')) return; 
            
            if (openAccordionId === id) {
                openAccordionId = null;
            } else {
                openAccordionId = id;
            }
            renderLibrary();
        }

        function renderLibrary() {
            const list = document.getElementById('library-list');
            const empty = document.getElementById('library-empty');
            const library = JSON.parse(localStorage.getItem('stepper_library') || '[]');

            if (library.length === 0) {
                list.classList.add('hidden');
                empty.classList.remove('hidden');
                return;
            }

            empty.classList.add('hidden');
            list.classList.remove('hidden');
            list.innerHTML = '';

            library.forEach(seq => {
                const itemContainer = document.createElement('div');
                itemContainer.className = "bg-white rounded-2xl border border-slate-200 hover:border-blue-200 transition-colors flex flex-col overflow-hidden";
                
                const isOpen = openAccordionId === seq.id;

                let commandsHTML = '';
                if (isOpen) {
                    commandsHTML = '<div class="bg-slate-50/70 border-t border-slate-100 p-2.5 flex flex-col gap-1.5 shadow-inner transition-all">';
                    seq.commands.forEach((cmd, idx) => {
                        commandsHTML += `
                            <div class="flex justify-between items-center bg-white px-3 py-2 rounded-xl border border-slate-200 shadow-sm group/line" id="line-container-${seq.id}-${idx}">
                                <div class="font-mono text-[13px] text-slate-600 flex-1 flex items-center pr-2" id="line-display-${seq.id}-${idx}">
                                    <span class="text-[10px] text-slate-400 w-6 inline-block select-none">[${idx}]</span>
                                    <span class="line-content tracking-wide">${cmd}</span>
                                </div>
                                <div class="hidden flex-1" id="line-edit-${seq.id}-${idx}">
                                    <div class="flex items-center w-full relative">
                                        <span class="text-[10px] text-slate-400 w-6 inline-block select-none absolute left-0 bg-slate-50 h-full flex items-center pl-1 rounded-l text-center border-r border-slate-200 z-10">[${idx}]</span>
                                        <input type="text" id="input-${seq.id}-${idx}" value="${cmd}" class="w-full text-[13px] font-mono pl-7 pr-2 py-1 border border-blue-400 rounded focus-visible:ring-2 focus-visible:ring-blue-200 outline-none shadow-inner" onkeydown="handleLineKeydown(event, '${seq.id}', ${idx})" onblur="cancelLineEdit('${seq.id}', ${idx})">
                                    </div>
                                </div>
                                <div class="flex items-center gap-1 opacity-40 md:opacity-0 group-hover/line:opacity-100 transition-opacity ml-2" id="line-actions-${seq.id}-${idx}">
                                    <button onclick="startLineEdit(event, '${seq.id}', ${idx})" class="p-1.5 text-slate-400 hover:text-blue-600 hover:bg-blue-50 rounded-lg transition-colors focus-visible:ring-2 focus-visible:ring-blue-500 outline-none" title="Editar Linha H8P">
                                        <svg aria-hidden="true" class="w-4 h-4" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M15.232 5.232l3.536 3.536m-2.036-5.036a2.5 2.5 0 113.536 3.536L6.5 21.036H3v-3.572L16.732 3.732z"></path></svg>
                                    </button>
                                    <button onclick="deleteSeqLine(event, '${seq.id}', ${idx})" class="p-1.5 text-slate-400 hover:text-rose-600 hover:bg-rose-50 rounded-lg transition-colors focus-visible:ring-2 focus-visible:ring-rose-500 outline-none" title="Remover Linha">
                                        <svg aria-hidden="true" class="w-4 h-4" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M19 7l-.867 12.142A2 2 0 0116.138 21H7.862a2 2 0 01-1.995-1.858L5 7m5 4v6m4-6v6m1-10V4a1 1 0 00-1-1h-4a1 1 0 00-1 1v3M4 7h16"></path></svg>
                                    </button>
                                </div>
                                <div class="hidden items-center gap-1 ml-2" id="line-save-${seq.id}-${idx}">
                                    <button onmousedown="saveLineEdit(event, '${seq.id}', ${idx})" class="p-1.5 text-blue-600 hover:bg-blue-50 rounded-lg transition-colors font-medium text-xs px-2 focus-visible:ring-2 focus-visible:ring-blue-500 outline-none" title="Salvar Alterações">
                                        Salvar
                                    </button>
                                </div>
                            </div>
                        `;
                    });
                    commandsHTML += '</div>';
                }

                itemContainer.innerHTML = `
                    <div class="p-4 flex justify-between items-center cursor-pointer group/item hover:bg-slate-50/50 transition-colors ${isOpen ? 'bg-slate-50/30' : ''}" onclick="toggleAccordion('${seq.id}', event)">
                        <div class="flex flex-col">
                            <span class="text-sm font-bold text-slate-800 flex items-center gap-2">
                                <svg aria-hidden="true" class="w-4 h-4 text-slate-400 transform transition-transform duration-200 ${isOpen ? 'rotate-90 text-blue-500' : ''}" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M9 5l7 7-7 7"></path></svg>
                                ${seq.name}
                            </span>
                            <span class="text-[10px] font-mono text-slate-500 mt-1 uppercase pl-6">${seq.commands.length} instrução(ões) • ${new Date(seq.date).toLocaleDateString('pt-BR')}</span>
                        </div>
                        <div class="flex items-center gap-2 pl-4">
                            <button onclick="loadTargetSequence('${seq.id}')" class="p-2.5 bg-sky-50 text-sky-600 rounded-xl hover:bg-sky-600 hover:text-white transition transform active:scale-90 shadow-sm focus-visible:ring-2 focus-visible:ring-sky-500 outline-none" title="Carregar na Placa">
                                <svg aria-hidden="true" class="w-5 h-5" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M4 16v1a3 3 0 003 3h10a3 3 0 003-3v-1m-4-8l-4-4m0 0L8 8m4-4v12"></path></svg>
                            </button>
                            <button onclick="deleteSequence('${seq.id}')" class="p-2.5 text-slate-300 hover:text-rose-500 hover:bg-rose-50 rounded-xl transition-colors focus-visible:ring-2 focus-visible:ring-rose-500 outline-none" title="Excluir Conjunto Completo">
                                <svg aria-hidden="true" class="w-5 h-5" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M19 7l-.867 12.142A2 2 0 0116.138 21H7.862a2 2 0 01-1.995-1.858L5 7m5 4v6m4-6v6m1-10V4a1 1 0 00-1-1h-4a1 1 0 00-1 1v3M4 7h16"></path></svg>
                            </button>
                        </div>
                    </div>
                    ${commandsHTML}
                `;
                list.appendChild(itemContainer);
            });
        }

        // --- SISTEMA DE EDIÇÃO INLINE ---
        function deleteSeqLine(event, seqId, idx) {
            event.stopPropagation();
            let library = JSON.parse(localStorage.getItem('stepper_library') || '[]');
            const seqIndex = library.findIndex(s => s.id === seqId);
            if (seqIndex > -1) {
                library[seqIndex].commands.splice(idx, 1);
                
                // Se o conjunto de movimentos esvaziou, revoga a sequência
                if (library[seqIndex].commands.length === 0) {
                    library.splice(seqIndex, 1);
                    openAccordionId = null;
                    showToast("Sequência auto-removida por falta de instruções.", "info");
                } else {
                    showToast(`Linha [${idx}] removida da biblioteca.`, "warning");
                }
                
                localStorage.setItem('stepper_library', JSON.stringify(library));
                renderLibrary();
            }
        }

        function startLineEdit(event, seqId, idx) {
            event.stopPropagation();
            document.getElementById(`line-display-${seqId}-${idx}`).classList.add('hidden');
            document.getElementById(`line-actions-${seqId}-${idx}`).classList.add('hidden');
            document.getElementById(`line-actions-${seqId}-${idx}`).classList.remove('flex');
            
            document.getElementById(`line-edit-${seqId}-${idx}`).classList.remove('hidden');
            document.getElementById(`line-edit-${seqId}-${idx}`).classList.add('flex');
            document.getElementById(`line-save-${seqId}-${idx}`).classList.remove('hidden');
            document.getElementById(`line-save-${seqId}-${idx}`).classList.add('flex');
            
            const input = document.getElementById(`input-${seqId}-${idx}`);
            input.focus();
            
            // Move cursor para o final (Truque JS)
            const val = input.value;
            input.value = '';
            input.value = val;
        }

        function cancelLineEdit(seqId, idx) {
            setTimeout(() => {
                const display = document.getElementById(`line-display-${seqId}-${idx}`);
                if (!display) return; // Se a DOM redesenhou antes, ignora
                
                display.classList.remove('hidden');
                document.getElementById(`line-actions-${seqId}-${idx}`).classList.remove('hidden');
                document.getElementById(`line-actions-${seqId}-${idx}`).classList.add('flex');
                
                document.getElementById(`line-edit-${seqId}-${idx}`).classList.add('hidden');
                document.getElementById(`line-edit-${seqId}-${idx}`).classList.remove('flex');
                document.getElementById(`line-save-${seqId}-${idx}`).classList.add('hidden');
                document.getElementById(`line-save-${seqId}-${idx}`).classList.remove('flex');
            }, 100); // 100ms é o suficiente para não matar o click no onmousedown do btn Save
        }

        function handleLineKeydown(event, seqId, idx) {
            if (event.key === 'Enter') {
                event.preventDefault();
                saveLineEdit(event, seqId, idx);
            } else if (event.key === 'Escape') {
                cancelLineEdit(seqId, idx);
            }
        }

        function saveLineEdit(event, seqId, idx) {
            if (event) event.stopPropagation();
            const input = document.getElementById(`input-${seqId}-${idx}`);
            const newValue = input.value.trim();
            
            if (!newValue) {
                showToast("Erro: O comando interno não pode ser vazio.", "error");
                input.focus();
                return;
            }
            
            let library = JSON.parse(localStorage.getItem('stepper_library') || '[]');
            const seqIndex = library.findIndex(s => s.id === seqId);
            if (seqIndex > -1) {
                library[seqIndex].commands[idx] = newValue;
                localStorage.setItem('stepper_library', JSON.stringify(library));
                renderLibrary();
                showToast(`Sintaxe da linha [${idx}] salva via Editor Inline.`, "success");
            }
        }

        async function loadTargetSequence(id) {
            const library = JSON.parse(localStorage.getItem('stepper_library') || '[]');
            const seq = library.find(s => s.id === id);
            if (!seq) return;

            if (!port) {
                showToast("Conecte o hardware antes de carregar a biblioteca.", "error");
                return;
            }

            toggleLibraryModal(false);

            // Limpar fila atual (MCU + local) antes de carregar a nova sequência
            await sendCommand("02"); // STOP — limpa SRAM do MCU
            currentQueue = [];
            updateQueueButtons();

            showToast(`📥 Carregando manobra: ${seq.name}...`, "info");
            
            // Lógica de envio sequencial seguro (Apenas LOAD, sem RUN)
            for (const cmd of seq.commands) {
                await sendCommand(cmd);
                currentQueue.push(cmd);
                await new Promise(r => setTimeout(r, 150)); // Delay extra para garantir estabilidade de buffer
            }
            updateQueueButtons();
            showToast("✅ Sequência carregada na memória com sucesso. Aguardando comando RUN.", "success");
        }

        // Modal Handlers
        const modal = document.getElementById('modal-library');
        function toggleLibraryModal(show) {
            if (show) {
                renderLibrary();
                modal.classList.remove('hidden');
                document.body.style.overflow = 'hidden';
            } else {
                modal.classList.add('hidden');
                document.body.style.overflow = '';
            }
        }

        document.getElementById('btn-library').addEventListener('click', () => toggleLibraryModal(true));
        document.getElementById('btn-close-modal').addEventListener('click', () => toggleLibraryModal(false));
        document.getElementById('modal-overlay').addEventListener('click', () => toggleLibraryModal(false));
        document.getElementById('btn-clear-all-library').addEventListener('click', () => {
            if(confirm("Tem certeza que deseja apagar TODA a biblioteca?")) {
                localStorage.removeItem('stepper_library');
                renderLibrary();
                showToast("Biblioteca resetada.", "error");
            }
        });

        document.getElementById('btn-run-one').addEventListener('click', async () => {
            const cmd = buildCommandFromInputs();
            if (cmd) {
                await sendCommand("02"); // STOP
                await sendCommand(cmd);  // LOAD
                await sendCommand("01"); // RUN
                lastCommandSent = cmd;
            }
        });

        document.getElementById('btn-run').addEventListener('click', async () => {
            await sendCommand(repeatOn ? "03:1" : "03:0");
            await sendCommand("01");
        });
        document.getElementById('btn-stop').addEventListener('click', () => sendCommand("02"));
        
        // Customização de UI para o botão Repeat All
        let repeatOn = false;
        document.getElementById('btn-repeat-all').addEventListener('click', () => {
             repeatOn = !repeatOn;
             const btn = document.getElementById('btn-repeat-all');
             const dot = document.getElementById('loop-dot');
             if(repeatOn) {
                 dot.classList.replace('bg-slate-300', 'bg-emerald-500');
                 btn.classList.add('border-emerald-400', 'bg-emerald-50', 'text-emerald-700');
                 btn.classList.remove('border-slate-300', 'bg-white', 'text-slate-700');
                 showToast("🔁 Loop ativado. Será enviado ao MCU junto com o RUN.", "info");
             } else {
                 dot.classList.replace('bg-emerald-500', 'bg-slate-300');
                 btn.classList.remove('border-emerald-400', 'bg-emerald-50', 'text-emerald-700');
                 btn.classList.add('border-slate-300', 'bg-white', 'text-slate-700');
                 showToast("⏹️ Loop desativado. A fila encerrará normalmente.", "info");
             }
        });

        document.getElementById('btn-set-pause').addEventListener('click', () => {
            const pGlobal = document.getElementById('inp-global-pause').value;
            if (pGlobal !== "") sendCommand(`04:${pGlobal}`);
        });
    
