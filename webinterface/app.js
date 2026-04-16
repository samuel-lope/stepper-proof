        // Web Serial API Params
        let port;
        let reader;
        let writer;
        let keepReading = true;
        let lastCommandSent = "";
        let currentQueue = [];

        // Re-apply connection-dependent text when language changes
        document.addEventListener('langchange', () => {
            updateUIState(!!port);
        });

        // UI Element Hooks
        const btnConnect    = document.getElementById('btn-connect');
        const statusBadge   = document.getElementById('conn-status');
        const statusDot     = document.getElementById('status-dot');
        const baudRateSelect = document.getElementById('baud-rate');

        const controls = [
            'btn-run', 'btn-stop', 'btn-run-one',
            'btn-set-pause', 'btn-repeat-all'
        ].map(id => document.getElementById(id));

        // ── Notification System ───────────────────────────────────────────────

        function logAlert(message, type) {
            let alerts = JSON.parse(localStorage.getItem('stepper_alerts') || '[]');
            alerts.unshift({ id: 'alert_' + Date.now(), message, type, date: new Date().toISOString() });
            if (alerts.length > 50) alerts = alerts.slice(0, 50);
            localStorage.setItem('stepper_alerts', JSON.stringify(alerts));

            const badge = document.getElementById('alert-badge');
            const targetModal = document.getElementById('modal-alerts');
            if (badge && targetModal && targetModal.classList.contains('hidden')) {
                badge.classList.remove('hidden');
            }
        }

        function showToast(message, type = 'info') {
            logAlert(message, type);
            const container = document.getElementById('toast-container');
            const toast = document.createElement('div');

            let icon = '';
            let colors = '';
            let duration = 4000;

            if (type === 'success') {
                colors = 'bg-navy border border-white/10 border-l-4 border-l-emerald-400 shadow-xl shadow-navy/20 text-cream/90';
                icon = `<svg aria-hidden="true" class="w-5 h-5 text-emerald-400" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M5 13l4 4L19 7"></path></svg>`;
            } else if (type === 'error') {
                colors = 'bg-white border border-brand/20 border-l-4 border-l-brand shadow-xl shadow-brand/10 text-navy';
                icon = `<svg aria-hidden="true" class="w-5 h-5 text-brand" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M12 8v4m0 4h.01M21 12a9 9 0 11-18 0 9 9 0 0118 0z"></path></svg>`;
                duration = 8000;
            } else if (type === 'warning') {
                colors = 'bg-white border border-amber/20 border-l-4 border-l-amber shadow-xl shadow-amber/10 text-navy';
                icon = `<svg aria-hidden="true" class="w-5 h-5 text-amber" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M12 9v2m0 4h.01m-6.938 4h13.856c1.54 0 2.502-1.667 1.732-3L13.732 4c-.77-1.333-2.694-1.333-3.464 0L3.34 16c-.77 1.333.192 3 1.732 3z"></path></svg>`;
            } else if (type === 'out') {
                colors = 'bg-navy text-cream/70 shadow-lg border border-white/10';
                icon = `<svg aria-hidden="true" class="w-4 h-4 text-cream/50" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M13 5l7 7-7 7M5 5l7 7-7 7"></path></svg>`;
                duration = 3000;
            } else {
                colors = 'bg-navy text-cream/90 border border-white/10 border-l-4 border-l-orange shadow-xl shadow-navy/20';
                icon = `<svg aria-hidden="true" class="w-5 h-5 text-orange" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M13 16h-1v-4h-1m1-4h.01M21 12a9 9 0 11-18 0 9 9 0 0118 0z"></path></svg>`;
            }

            toast.className = `toast-enter pointer-events-auto rounded-xl p-4 font-mono text-sm flex items-start gap-3 backdrop-blur-md max-w-sm w-full ${colors}`;
            const textStyles = type === 'out' ? 'font-mono text-[13px] opacity-80' : 'font-sans font-medium text-[14px]';

            toast.innerHTML = `
                <div class="flex-shrink-0 mt-0.5">${icon}</div>
                <div class="flex-1 pt-0.5">
                    <p class="${textStyles} leading-tight">${message}</p>
                </div>
                <button class="flex-shrink-0 ml-4 hover:opacity-75 transition-opacity outline-none focus-visible:ring-2 focus-visible:ring-current focus-visible:outline-none" onclick="this.parentElement.remove()" aria-label="${t('btn.close')}">
                    <svg aria-hidden="true" class="w-4 h-4 ${type === 'error' || type === 'warning' ? 'text-current' : 'text-cream/40'}" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M6 18L18 6M6 6l12 12"></path></svg>
                </button>
            `;

            container.appendChild(toast);

            setTimeout(() => {
                if (toast.parentElement) {
                    toast.classList.replace('toast-enter', 'toast-leave');
                    toast.addEventListener('animationend', () => toast.remove());
                }
            }, duration);
        }

        // ── Serial Connection ─────────────────────────────────────────────────

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
                showToast(t('toast.connect.success', { baud: selectedBaudRate }), 'success');
                keepReading = true;
                readLoop();
            } catch (error) {
                console.error('Connection error:', error);
                showToast(t('toast.connect.error', { msg: error.message }), 'error');
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
            showToast(t('toast.disconnect'), 'warning');
        }

        function updateUIState(connected) {
            controls.forEach(btn => btn.disabled = !connected);
            baudRateSelect.disabled = connected;

            if (connected) {
                btnConnect.textContent = t('header.disconnect');
                btnConnect.className = 'text-brand bg-cream/20 hover:bg-cream/40 text-sm font-medium px-5 py-2.5 rounded-lg border border-brand/30 transition cursor-pointer transform active:scale-95 focus-visible:ring-2 focus-visible:ring-offset-1 focus-visible:ring-brand focus-visible:outline-none';

                statusBadge.textContent = t('header.connected');
                statusBadge.className   = 'text-[11px] font-bold text-emerald-400 uppercase tracking-wider transition-colors duration-300';

                statusDot.className = 'w-2.5 h-2.5 rounded-full bg-emerald-500 shadow-[0_0_8px_rgba(16,185,129,0.7)] transition-colors duration-300';

                document.getElementById('btn-run-one').disabled = lastCommandSent === '';
            } else {
                btnConnect.textContent = t('header.connect');
                btnConnect.className = 'bg-orange hover:bg-orange-dark text-white text-sm font-medium px-5 py-2.5 rounded-lg shadow-sm shadow-orange/30 transition cursor-pointer transform active:scale-95 focus-visible:ring-2 focus-visible:ring-offset-1 focus-visible:ring-orange focus-visible:outline-none';

                statusBadge.textContent = t('header.offline');
                statusBadge.className   = 'text-[11px] font-bold text-cream/60 uppercase tracking-wider transition-colors duration-300';

                statusDot.className = 'w-2.5 h-2.5 rounded-full bg-brand shadow-[0_0_8px_rgba(214,40,40,0.7)] transition-colors duration-300';
            }
        }

        // ── AVR Response Translator ───────────────────────────────────────────

        function translateIncomingHex(line) {
            line = line.trim();
            if (!line) return null;

            // Silent telemetry parsers
            if (line.startsWith('C1:')) {
                document.getElementById('tel-queue').textContent = line.substring(3);
                return null;
            }
            if (line.startsWith('D0:')) {
                const rawSlot  = parseInt(line.substring(3));
                const motorNum = Math.floor(rawSlot / 100);
                const slotIdx  = rawSlot % 100;
                const stId     = motorNum === 2 ? 'tel-state-m2' : 'tel-state-m1';
                const st       = document.getElementById(stId);
                if (st) {
                    st.textContent = `Exec L[${slotIdx}]`;
                    st.className   = 'text-[12px] font-mono text-cyan-400 font-semibold truncate';
                }
                return null;
            }
            if (line.startsWith('B3:')) {
                const pTime = line.substring(3);
                ['tel-state-m1', 'tel-state-m2'].forEach(id => {
                    const st = document.getElementById(id);
                    if (st && st.textContent.startsWith('Exec')) {
                        st.textContent = `Delay ${pTime}ms`;
                        st.className   = 'text-[12px] font-mono text-amber-400 animate-pulse truncate';
                    }
                });
                return null;
            }

            // Map of static hex codes → i18n keys + toast type
            const staticResponses = {
                'A0': { key: 'hw.A0', type: 'success' },
                'B0': { key: 'hw.B0', type: 'info'    },
                'B1': { key: 'hw.B1', type: 'warning'  },
                'B4': { key: 'hw.B4', type: 'info'    },
                'B5': { key: 'hw.B5', type: 'success' },
                'B6': { key: 'hw.B6', type: 'info'    },
                'E0': { key: 'hw.E0', type: 'error'   },
                'E1': { key: 'hw.E1', type: 'error'   },
                'E2': { key: 'hw.E2', type: 'error'   },
                'E3': { key: 'hw.E3', type: 'error'   },
            };

            if (staticResponses[line]) {
                const { key, type } = staticResponses[line];
                // Reset telemetry on terminal states
                if (line === 'B1' || line === 'B5' || line === 'A0') {
                    ['tel-state-m1', 'tel-state-m2'].forEach(id => {
                        const st = document.getElementById(id);
                        if (st) {
                            st.textContent = 'Standby';
                            st.className   = 'text-[12px] font-mono text-cream/60 truncate';
                        }
                    });
                    document.getElementById('tel-queue').textContent = '0';
                }
                return { text: t(key), type };
            }

            if (line.startsWith('B2:'))
                return { text: t('hw.B2', { ms: line.substring(3) }), type: 'info' };

            if (line.startsWith('C0:')) {
                const parts = line.split(',');
                const idx   = parts[0].split(':')[1];
                const details = [];
                let motorLabel = 'M1';
                for (let i = 1; i < parts.length; i++) {
                    const [key, val] = parts[i].split(':');
                    if (key === '10') details.push(`${val} ${t('hw.step.unit')}`);
                    if (key === '11') details.push(`${val}µs ${t('hw.int.unit')}`);
                    if (key === '12') details.push(val === '1' ? '[FWD]' : '[REV]');
                    if (key === '15') motorLabel = val === '2' ? 'M2' : 'M1';
                }
                return { text: t('hw.allocated', { idx, motor: motorLabel, details: details.join(' | ') }), type: 'success' };
            }

            return { text: `RAW_HW > ${line}`, type: 'info' };
        }

        async function readLoop() {
            let lineBuffer = '';
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
                                    const translated = translateIncomingHex(completeLine);
                                    if (translated) showToast(translated.text, translated.type);
                                }
                            }
                        }
                    }
                } catch (error) {
                    console.error('Reader Halt:', error);
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

            await writer.write(commandString + '\n');
            await writer.close();

            let displayCmd = commandString;
            if      (commandString === '01')             displayCmd = t('tx.run');
            else if (commandString === '02')             displayCmd = t('tx.stop');
            else if (commandString === '03')             displayCmd = t('tx.loop');
            else if (commandString.startsWith('04:'))   displayCmd = t('tx.pause', { ms: commandString.split(':')[1] });
            else if (commandString.startsWith('10:')) {
                const parts = commandString.split(',');
                displayCmd = t('tx.packet', { p0: parts[0], p1: parts[1] });
            }
            showToast(`TX > ${displayCmd}`, 'out');
            return true;
        }

        async function sendCommand(commandString) {
            if (await sendRawString(commandString)) {
                await new Promise(r => setTimeout(r, 60));
            } else {
                showToast(t('toast.io.absent'), 'error');
            }
        }

        // ── Command Engineering ───────────────────────────────────────────────

        let selectedMotor = 1;

        function selectMotor(num) {
            selectedMotor = num;
            document.getElementById('inp-motor').value = num;

            const btn1 = document.getElementById('btn-motor-1');
            const btn2 = document.getElementById('btn-motor-2');
            const base = 'motor-selector flex items-center justify-center gap-2 py-2.5 rounded-lg border-2 font-semibold text-sm transition-all cursor-pointer shadow-sm focus-visible:ring-2 outline-none';
            const inactive = `${base} border-cream-dark bg-white text-navy/50 hover:border-orange/40 hover:bg-cream/40 focus-visible:ring-orange`;

            if (num === 1) {
                btn1.className = `${base} active-motor border-orange bg-orange/10 text-orange-dark focus-visible:ring-orange`;
                btn2.className = inactive;
            } else {
                btn2.className = `${base} active-motor border-amber bg-amber/10 text-amber-dark focus-visible:ring-amber`;
                btn1.className = inactive;
            }
        }

        function buildCommandFromInputs() {
            const step   = document.getElementById('inp-step').value;
            const vel    = document.getElementById('inp-vel').value;
            const dir    = document.getElementById('inp-dir').value;
            const repeat = document.getElementById('inp-repeat').value;
            const pause  = document.getElementById('inp-pause').value;
            const motor  = document.getElementById('inp-motor').value;

            if (!step || !vel || vel < 50) {
                showToast(t('toast.vel.error'), 'error');
                const velInp = document.getElementById('inp-vel');
                velInp.classList.add('border-brand', 'ring-brand', 'ring-2');
                setTimeout(() => velInp.classList.remove('border-brand', 'ring-brand', 'ring-2'), 2500);
                return null;
            }

            let cmd = `10:${step},11:${vel},12:${dir},13:${repeat}`;
            if (pause && pause.trim() !== '') cmd += `,14:${pause}`;
            cmd += `,15:${motor}`;
            return cmd;
        }

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
                    showToast(t('toast.queue.added.offline'), 'success');
                }
            }
        });

        function updateQueueButtons() {
            const saveBtn  = document.getElementById('btn-save-queue');
            const clearBtn = document.getElementById('btn-clear-queue');
            saveBtn.disabled  = currentQueue.length === 0;
            clearBtn.disabled = currentQueue.length === 0;
        }

        document.getElementById('btn-clear-queue').addEventListener('click', () => {
            currentQueue = [];
            updateQueueButtons();
            showToast(t('toast.queue.cleared'), 'info');
        });

        document.getElementById('btn-save-queue').addEventListener('click', async () => {
            const defaultName = `${t('seq.default.name')} ${new Date().toLocaleTimeString(currentLang)}`;
            const name = await showPromptModal(t('prompt.seq.save.title'), defaultName);
            if (name === null) return;
            const finalName = name.trim() === '' ? `Seq_${Date.now()}` : name;
            saveSequence(finalName, [...currentQueue]);
        });

        // ── Library (LocalStorage) ────────────────────────────────────────────

        function saveSequence(name, commands) {
            let library = JSON.parse(localStorage.getItem('stepper_library') || '[]');
            library.unshift({ id: 'seq_' + Date.now(), name, commands, date: new Date().toISOString() });
            localStorage.setItem('stepper_library', JSON.stringify(library));
            showToast(t('toast.seq.saved', { name }), 'success');
        }

        async function deleteSequence(id, name) {
            if (!await showConfirmModal(t('confirm.seq.delete.title'), t('confirm.seq.delete.text', { name }))) return;
            let library = JSON.parse(localStorage.getItem('stepper_library') || '[]');
            library = library.filter(s => s.id !== id);
            localStorage.setItem('stepper_library', JSON.stringify(library));
            renderLibrary();
            showToast(t('toast.seq.deleted'), 'warning');
        }

        let openAccordionId = null;

        function toggleAccordion(id, event) {
            if (event && (event.target.closest('button') || event.target.tagName === 'INPUT')) return;
            openAccordionId = openAccordionId === id ? null : id;
            renderLibrary();
        }

        function renderLibrary() {
            const list    = document.getElementById('library-list');
            const empty   = document.getElementById('library-empty');
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
                itemContainer.className = 'bg-white rounded-2xl border border-cream-dark hover:border-orange/30 transition-colors flex flex-col overflow-hidden';

                const isOpen = openAccordionId === seq.id;

                let commandsHTML = '';
                if (isOpen) {
                    commandsHTML = '<div class="bg-cream/30 border-t border-cream-dark p-2.5 flex flex-col gap-1.5 shadow-inner">';
                    seq.commands.forEach((cmd, idx) => {
                        commandsHTML += `
                            <div class="flex justify-between items-center bg-white px-3 py-2 rounded-xl border border-cream-dark shadow-sm group/line" id="line-container-${seq.id}-${idx}">
                                <div class="font-mono text-[13px] text-navy/70 flex-1 flex items-center pr-2" id="line-display-${seq.id}-${idx}">
                                    <span class="text-[10px] text-navy/40 w-6 inline-block select-none">[${idx}]</span>
                                    <span class="line-content tracking-wide">${cmd}</span>
                                </div>
                                <div class="hidden flex-1" id="line-edit-${seq.id}-${idx}">
                                    <div class="flex items-center w-full relative">
                                        <span class="text-[10px] text-navy/40 w-6 inline-block select-none absolute left-0 bg-cream/50 h-full flex items-center pl-1 rounded-l text-center border-r border-cream-dark z-10">[${idx}]</span>
                                        <input type="text" id="input-${seq.id}-${idx}" value="${cmd}" class="w-full text-[13px] font-mono pl-7 pr-2 py-1 border border-orange rounded focus-visible:ring-2 focus-visible:ring-orange/20 outline-none shadow-inner" onkeydown="handleLineKeydown(event, '${seq.id}', ${idx})" onblur="cancelLineEdit('${seq.id}', ${idx})">
                                    </div>
                                </div>
                                <div class="flex items-center gap-1 opacity-40 md:opacity-0 group-hover/line:opacity-100 transition-opacity ml-2" id="line-actions-${seq.id}-${idx}">
                                    <button onclick="startLineEdit(event, '${seq.id}', ${idx})" class="p-1.5 text-navy/40 hover:text-orange-dark hover:bg-orange/10 rounded-lg transition-colors focus-visible:ring-2 focus-visible:ring-orange outline-none" title="${t('lib.line.edit.title')}">
                                        <svg aria-hidden="true" class="w-4 h-4" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M15.232 5.232l3.536 3.536m-2.036-5.036a2.5 2.5 0 113.536 3.536L6.5 21.036H3v-3.572L16.732 3.732z"></path></svg>
                                    </button>
                                    <button onclick="deleteSeqLine(event, '${seq.id}', ${idx})" class="p-1.5 text-navy/40 hover:text-brand hover:bg-brand/10 rounded-lg transition-colors focus-visible:ring-2 focus-visible:ring-brand outline-none" title="${t('lib.line.delete.title')}">
                                        <svg aria-hidden="true" class="w-4 h-4" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M19 7l-.867 12.142A2 2 0 0116.138 21H7.862a2 2 0 01-1.995-1.858L5 7m5 4v6m4-6v6m1-10V4a1 1 0 00-1-1h-4a1 1 0 00-1 1v3M4 7h16"></path></svg>
                                    </button>
                                </div>
                                <div class="hidden items-center gap-1 ml-2" id="line-save-${seq.id}-${idx}">
                                    <button onmousedown="saveLineEdit(event, '${seq.id}', ${idx})" class="p-1.5 text-orange-dark hover:bg-orange/10 rounded-lg transition-colors font-medium text-xs px-2 focus-visible:ring-2 focus-visible:ring-orange outline-none" title="${t('lib.line.save.btn')}">
                                        ${t('lib.line.save.btn')}
                                    </button>
                                </div>
                            </div>
                        `;
                    });
                    commandsHTML += '</div>';
                }

                itemContainer.innerHTML = `
                    <div class="p-4 flex justify-between items-center cursor-pointer group/item hover:bg-cream/30 transition-colors ${isOpen ? 'bg-cream/20' : ''}" onclick="toggleAccordion('${seq.id}', event)">
                        <div class="flex flex-col">
                            <span class="text-sm font-bold text-navy flex items-center gap-2">
                                <svg aria-hidden="true" class="w-4 h-4 text-navy/40 transform transition-transform duration-200 ${isOpen ? 'rotate-90 text-orange' : ''}" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M9 5l7 7-7 7"></path></svg>
                                ${seq.name}
                            </span>
                            <span class="text-[10px] font-mono text-navy/50 mt-1 uppercase pl-6">${t('lib.item.instructions', { n: seq.commands.length })} • ${new Date(seq.date).toLocaleDateString(currentLang)}</span>
                        </div>
                        <div class="flex items-center gap-2 pl-4">
                            <button onclick="loadTargetSequence('${seq.id}')" class="p-2.5 bg-orange/10 text-orange-dark rounded-xl hover:bg-orange hover:text-white transition transform active:scale-90 shadow-sm focus-visible:ring-2 focus-visible:ring-orange outline-none" title="${t('lib.item.load.title')}">
                                <svg aria-hidden="true" class="w-5 h-5" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M4 16v1a3 3 0 003 3h10a3 3 0 003-3v-1m-4-8l-4-4m0 0L8 8m4-4v12"></path></svg>
                            </button>
                            <button onclick="deleteSequence('${seq.id}', '${seq.name.replace(/'/g, "\\'")}')" class="p-2.5 text-navy/30 hover:text-brand hover:bg-brand/10 rounded-xl transition-colors focus-visible:ring-2 focus-visible:ring-brand outline-none" title="${t('lib.item.delete.title')}">
                                <svg aria-hidden="true" class="w-5 h-5" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M19 7l-.867 12.142A2 2 0 0116.138 21H7.862a2 2 0 01-1.995-1.858L5 7m5 4v6m4-6v6m1-10V4a1 1 0 00-1-1h-4a1 1 0 00-1 1v3M4 7h16"></path></svg>
                            </button>
                        </div>
                    </div>
                    ${commandsHTML}
                `;
                list.appendChild(itemContainer);
            });
        }

        // ── Inline Line Editor ────────────────────────────────────────────────

        function deleteSeqLine(event, seqId, idx) {
            event.stopPropagation();
            let library = JSON.parse(localStorage.getItem('stepper_library') || '[]');
            const seqIndex = library.findIndex(s => s.id === seqId);
            if (seqIndex > -1) {
                library[seqIndex].commands.splice(idx, 1);
                if (library[seqIndex].commands.length === 0) {
                    library.splice(seqIndex, 1);
                    openAccordionId = null;
                    showToast(t('toast.seq.autodelete'), 'info');
                } else {
                    showToast(t('toast.seq.line.deleted', { idx }), 'warning');
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
            const val = input.value;
            input.value = '';
            input.value = val;
        }

        function cancelLineEdit(seqId, idx) {
            setTimeout(() => {
                const display = document.getElementById(`line-display-${seqId}-${idx}`);
                if (!display) return;

                display.classList.remove('hidden');
                document.getElementById(`line-actions-${seqId}-${idx}`).classList.remove('hidden');
                document.getElementById(`line-actions-${seqId}-${idx}`).classList.add('flex');

                document.getElementById(`line-edit-${seqId}-${idx}`).classList.add('hidden');
                document.getElementById(`line-edit-${seqId}-${idx}`).classList.remove('flex');
                document.getElementById(`line-save-${seqId}-${idx}`).classList.add('hidden');
                document.getElementById(`line-save-${seqId}-${idx}`).classList.remove('flex');
            }, 100);
        }

        function handleLineKeydown(event, seqId, idx) {
            if (event.key === 'Enter')  { event.preventDefault(); saveLineEdit(event, seqId, idx); }
            else if (event.key === 'Escape') { cancelLineEdit(seqId, idx); }
        }

        function saveLineEdit(event, seqId, idx) {
            if (event) event.stopPropagation();
            const input    = document.getElementById(`input-${seqId}-${idx}`);
            const newValue = input.value.trim();

            if (!newValue) {
                showToast(t('toast.seq.line.empty'), 'error');
                input.focus();
                return;
            }

            let library = JSON.parse(localStorage.getItem('stepper_library') || '[]');
            const seqIndex = library.findIndex(s => s.id === seqId);
            if (seqIndex > -1) {
                library[seqIndex].commands[idx] = newValue;
                localStorage.setItem('stepper_library', JSON.stringify(library));
                renderLibrary();
                showToast(t('toast.seq.line.saved', { idx }), 'success');
            }
        }

        async function loadTargetSequence(id) {
            const library = JSON.parse(localStorage.getItem('stepper_library') || '[]');
            const seq = library.find(s => s.id === id);
            if (!seq) return;

            if (!port) {
                showToast(t('toast.seq.load.noconn'), 'error');
                return;
            }

            toggleLibraryModal(false);

            await sendCommand('02');
            currentQueue = [];
            updateQueueButtons();

            showToast(t('toast.seq.loading', { name: seq.name }), 'info');

            for (const cmd of seq.commands) {
                await sendCommand(cmd);
                currentQueue.push(cmd);
                await new Promise(r => setTimeout(r, 150));
            }
            updateQueueButtons();
            showToast(t('toast.seq.loaded'), 'success');
        }

        // ── Library Modal ─────────────────────────────────────────────────────

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

        document.getElementById('btn-clear-all-library').addEventListener('click', async () => {
            if (await showConfirmModal(t('confirm.lib.clearall.title'), t('confirm.lib.clearall.text'))) {
                localStorage.removeItem('stepper_library');
                renderLibrary();
                showToast(t('toast.lib.cleared'), 'error');
            }
        });

        // ── Hardware Controls ─────────────────────────────────────────────────

        document.getElementById('btn-run-one').addEventListener('click', async () => {
            const cmd = buildCommandFromInputs();
            if (cmd) {
                await sendCommand('02');
                await sendCommand(cmd);
                await sendCommand('01');
                lastCommandSent = cmd;
            }
        });

        document.getElementById('btn-run').addEventListener('click', async () => {
            await sendCommand(repeatOn ? '03:1' : '03:0');
            await sendCommand('01');
        });

        document.getElementById('btn-stop').addEventListener('click', () => sendCommand('02'));

        document.getElementById('btn-set-pause').addEventListener('click', () => {
            const pGlobal = document.getElementById('inp-global-pause').value;
            if (pGlobal !== '') sendCommand(`04:${pGlobal}`);
        });

        // ── RepeatAll Toggle ──────────────────────────────────────────────────

        let repeatOn = false;

        document.getElementById('btn-repeat-all').addEventListener('click', () => {
            repeatOn = !repeatOn;
            const btn = document.getElementById('btn-repeat-all');
            const dot = document.getElementById('loop-dot');

            if (repeatOn) {
                dot.classList.replace('bg-cream-dark', 'bg-emerald-500');
                btn.classList.add('border-amber', 'bg-amber/10', 'text-amber-dark');
                btn.classList.remove('border-cream-dark', 'bg-white', 'text-navy');
                showToast(t('toast.loop.on'), 'info');
            } else {
                dot.classList.replace('bg-emerald-500', 'bg-cream-dark');
                btn.classList.remove('border-amber', 'bg-amber/10', 'text-amber-dark');
                btn.classList.add('border-cream-dark', 'bg-white', 'text-navy');
                showToast(t('toast.loop.off'), 'info');
            }
        });

        // ── Alert History Modal ───────────────────────────────────────────────

        const alertsModal = document.getElementById('modal-alerts');

        function toggleAlertsModal(show) {
            if (!alertsModal) return;
            if (show) {
                renderAlerts();
                alertsModal.classList.remove('hidden');
                document.body.style.overflow = 'hidden';
                const badge = document.getElementById('alert-badge');
                if (badge) badge.classList.add('hidden');
            } else {
                alertsModal.classList.add('hidden');
                document.body.style.overflow = '';
            }
        }

        document.getElementById('btn-alert-history')?.addEventListener('click',       () => toggleAlertsModal(true));
        document.getElementById('btn-close-alerts')?.addEventListener('click',        () => toggleAlertsModal(false));
        document.getElementById('modal-alerts-overlay')?.addEventListener('click',    () => toggleAlertsModal(false));

        document.getElementById('btn-clear-alerts')?.addEventListener('click', () => {
            localStorage.removeItem('stepper_alerts');
            renderAlerts();
            showToast(t('toast.alerts.cleared'), 'warning');
        });

        function renderAlerts() {
            const list     = document.getElementById('alerts-list');
            const empty    = document.getElementById('alerts-empty');
            const clearBtn = document.getElementById('btn-clear-alerts');
            const alerts   = JSON.parse(localStorage.getItem('stepper_alerts') || '[]');

            if (alerts.length === 0) {
                list.classList.add('hidden');
                empty.classList.remove('hidden');
                clearBtn.disabled = true;
                return;
            }

            empty.classList.add('hidden');
            list.classList.remove('hidden');
            clearBtn.disabled = false;
            list.innerHTML = '';

            alerts.forEach(alert => {
                const item = document.createElement('div');
                let colorClass = 'bg-white border-cream-dark text-navy hover:border-orange/30';
                let iconStr    = `<svg class="w-5 h-5 text-navy/40" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M13 16h-1v-4h-1m1-4h.01M21 12a9 9 0 11-18 0 9 9 0 0118 0z"></path></svg>`;

                if (alert.type === 'error') {
                    colorClass = 'bg-white border-brand/20 text-brand hover:border-brand/40';
                    iconStr    = `<svg class="w-5 h-5 text-brand" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M12 8v4m0 4h.01M21 12a9 9 0 11-18 0 9 9 0 0118 0z"></path></svg>`;
                } else if (alert.type === 'success') {
                    colorClass = 'bg-white border-emerald-200 text-emerald-800 hover:border-emerald-300';
                    iconStr    = `<svg class="w-5 h-5 text-emerald-500" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M5 13l4 4L19 7"></path></svg>`;
                } else if (alert.type === 'warning') {
                    colorClass = 'bg-white border-amber/20 text-amber-dark hover:border-amber/40';
                    iconStr    = `<svg class="w-5 h-5 text-amber" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M12 9v2m0 4h.01m-6.938 4h13.856c1.54 0 2.502-1.667 1.732-3L13.732 4c-.77-1.333-2.694-1.333-3.464 0L3.34 16c-.77 1.333.192 3 1.732 3z"></path></svg>`;
                } else if (alert.type === 'out') {
                    colorClass = 'bg-navy border-white/10 text-cream/80 hover:border-white/20';
                    iconStr    = `<svg class="w-4 h-4 text-cream/60" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M13 5l7 7-7 7M5 5l7 7-7 7"></path></svg>`;
                }

                item.className = `p-4 rounded-2xl border flex items-start gap-4 shadow-sm transition-colors cursor-default ${colorClass}`;
                item.innerHTML = `
                    <div class="mt-0.5 shrink-0">${iconStr}</div>
                    <div class="flex-1">
                        <p class="leading-snug text-sm font-medium mb-1">${alert.message}</p>
                        <p class="text-[11px] opacity-70 font-mono tracking-wider">${new Date(alert.date).toLocaleTimeString(currentLang)}</p>
                    </div>
                `;
                list.appendChild(item);
            });
        }

        // ── Custom Modals ─────────────────────────────────────────────────────

        function showPromptModal(title, defaultValue) {
            return new Promise((resolve) => {
                const modal     = document.getElementById('modal-prompt');
                const titleEl   = document.getElementById('modal-prompt-title');
                const inputEl   = document.getElementById('modal-prompt-input');
                const btnCancel = document.getElementById('modal-prompt-cancel');
                const btnConfirm = document.getElementById('modal-prompt-confirm');

                titleEl.textContent = title;
                inputEl.value = defaultValue || '';

                modal.classList.remove('hidden');
                document.body.style.overflow = 'hidden';
                inputEl.focus();
                inputEl.select();

                const cleanup = () => {
                    modal.classList.add('hidden');
                    document.body.style.overflow = '';
                    btnCancel.removeEventListener('click', onCancel);
                    btnConfirm.removeEventListener('click', onConfirm);
                    inputEl.removeEventListener('keydown', onKey);
                };

                const onCancel  = () => { cleanup(); resolve(null); };
                const onConfirm = () => { cleanup(); resolve(inputEl.value); };
                const onKey = (e) => {
                    if (e.key === 'Enter')  onConfirm();
                    if (e.key === 'Escape') onCancel();
                };

                btnCancel.addEventListener('click', onCancel);
                btnConfirm.addEventListener('click', onConfirm);
                inputEl.addEventListener('keydown', onKey);
            });
        }

        function showConfirmModal(title, text) {
            return new Promise((resolve) => {
                const modal     = document.getElementById('modal-confirm');
                const titleEl   = document.getElementById('modal-confirm-title');
                const textEl    = document.getElementById('modal-confirm-text');
                const btnCancel = document.getElementById('modal-confirm-cancel');
                const btnConfirm = document.getElementById('modal-confirm-confirm');

                titleEl.textContent = title;
                textEl.textContent  = text;

                modal.classList.remove('hidden');
                document.body.style.overflow = 'hidden';

                const cleanup = () => {
                    modal.classList.add('hidden');
                    document.body.style.overflow = '';
                    btnCancel.removeEventListener('click', onCancel);
                    btnConfirm.removeEventListener('click', onConfirm);
                    document.removeEventListener('keydown', onKey);
                };

                const onCancel  = () => { cleanup(); resolve(false); };
                const onConfirm = () => { cleanup(); resolve(true);  };
                const onKey = (e) => { if (e.key === 'Escape') onCancel(); };

                btnCancel.addEventListener('click', onCancel);
                btnConfirm.addEventListener('click', onConfirm);
                document.addEventListener('keydown', onKey);
            });
        }
