/**
 * i18n.js — StepperControl Pro Internationalization
 * Supports: en-US (default), pt-BR
 * Usage: t('key') | t('key', { var: value }) | setLanguage('pt-BR')
 */

const translations = {
    'en-US': {
        // ── App ──────────────────────────────────────────────────────────
        'app.title': 'StepperControl Pro',

        // ── Header ───────────────────────────────────────────────────────
        'header.library': 'Library',
        'header.offline': 'Offline',
        'header.connected': 'Connected',
        'header.connect': 'Connect Hardware',
        'header.disconnect': 'Disconnect',
        'lang.label': 'Language',

        // ── Command Builder ───────────────────────────────────────────────
        'cmd.card.title': 'New Movement Command',
        'cmd.card.subtitle': 'Configure technical parameters to inject into the microcontroller queue.',
        'cmd.motor.label': 'Target Motor',
        'cmd.motor.1': 'Motor 1',
        'cmd.motor.2': 'Motor 2',
        'cmd.step.label': 'Step Count',
        'cmd.vel.label': 'Pulse Interval (µs)',
        'cmd.vel.hint': 'MIN 50',
        'cmd.dir.label': 'Physical Direction',
        'cmd.dir.fwd': '1 (Direction A — Forward)',
        'cmd.dir.rev': '0 (Direction B — Reverse)',
        'cmd.repeat.label': 'Repetition Cycles',
        'cmd.pause.label': 'Post-Execution Pause',
        'cmd.pause.placeholder': 'e.g. 500 (Optional)',
        'cmd.btn.clear': 'Clear Current Queue',
        'cmd.btn.save': 'Save Sequence',
        'cmd.btn.add': 'Push to Array',
        'cmd.btn.preset': 'Fast Action',

        // ── Global Config Cards ───────────────────────────────────────────
        'fastaction.title': 'Fast Action (Key 18)',
        'jog.title': 'Jog Scrubber (Key 1B)',
        'global.pause.title': 'Global Pause (Key 04)',
        'global.pause.desc': 'Fixed delay automatically injected between all lines in the motor queue.',
        'global.pause.placeholder': 'e.g. 1000',
        'global.pause.btn': 'Apply',
        'global.loop.title': 'Loop Master (Key 03)',
        'global.loop.desc': 'Overrides the queue to restart infinitely from zero after completing.',
        'global.loop.btn': 'Activate Queue RepeatAll',

        // ── Telemetry & Hardware Panel ────────────────────────────────────
        'hw.panel.title': 'Hardware Panel',
        'hw.step.btn': 'Fire Single Step',
        'hw.enable.title': 'Enable/Disable motor driver',
        'tel.driver.on': 'Driver ON',
        'tel.driver.off': 'Driver OFF',

        // ── Alert History Floating Button ─────────────────────────────────
        'alerts.btn.title': 'Alert History',
        'alerts.btn.aria': 'View recent alert history',

        // ── Library Modal ─────────────────────────────────────────────────
        'lib.modal.title': 'Sequence Library',
        'lib.modal.subtitle': 'Manage and execute locally saved command sets.',
        'lib.empty.title': 'No saved sequences',
        'lib.empty.desc': 'Build movements and click the save button to start your technical library.',
        'lib.footer.info': 'Data persisted via LocalStorage.',
        'lib.btn.clearall': 'Clear All',

        // ── Library List Items (dynamic) ──────────────────────────────────
        'lib.item.instructions': '{n} instruction(s)',
        'lib.item.load.title': 'Load to Board',
        'lib.item.delete.title': 'Delete Complete Set',
        'lib.line.edit.title': 'Edit H8P Line',
        'lib.line.delete.title': 'Remove Line',
        'lib.line.save.btn': 'Save',

        // ── Alerts Modal ──────────────────────────────────────────────────
        'alertsmodal.title': 'Recent Alerts',
        'alertsmodal.subtitle': 'Local Session',
        'alertsmodal.empty.title': 'No occurrences',
        'alertsmodal.empty.desc': 'System alerts will appear here.',
        'alertsmodal.btn.clear': 'Empty History',

        // ── Generic Modals ────────────────────────────────────────────────
        'modal.btn.cancel': 'Cancel',
        'modal.btn.save': 'Save',
        'modal.btn.confirm': 'Confirm',

        // ── Confirm Modal Calls ───────────────────────────────────────────
        'confirm.seq.delete.title': 'Delete Sequence',
        'confirm.seq.delete.text': 'Are you sure you want to remove "{name}"?',
        'confirm.lib.clearall.title': 'Clear Library',
        'confirm.lib.clearall.text': 'Are you sure you want to delete the ENTIRE library? This action cannot be undone.',

        // ── Prompt Modal Calls ────────────────────────────────────────────
        'prompt.seq.save.title': 'Save Sequence',
        'prompt.eeprom.title': 'Save EEPROM Preset (Slot 0-4):',
        'seq.default.name': 'Sequence',

        // ── Button / Shared ───────────────────────────────────────────────
        'btn.close': 'Close',

        // ── Toast Messages ────────────────────────────────────────────────
        'toast.connect.success': 'Hardware Serial paired ({baud} bps).',
        'toast.disconnect': 'Arduino board safely disconnected.',
        'toast.connect.error': 'WebSerial Error: {msg}',
        'toast.io.absent': 'I/O connection unavailable.',
        'toast.queue.added.offline': 'Step added to local queue (Offline)',
        'toast.queue.cleared': 'Session queue cleared locally.',
        'toast.seq.saved': 'Sequence "{name}" saved successfully!',
        'toast.seq.deleted': 'Sequence removed.',
        'toast.seq.line.deleted': 'Line [{idx}] removed from library.',
        'toast.seq.autodelete': 'Sequence auto-removed: no remaining instructions.',
        'toast.seq.line.saved': 'Line [{idx}] syntax saved via Inline Editor.',
        'toast.seq.line.empty': 'Error: Command cannot be empty.',
        'toast.seq.load.noconn': 'Connect the hardware before loading a library.',
        'toast.seq.loading': '📥 Loading maneuver: {name}...',
        'toast.seq.loaded': '✅ Sequence loaded into memory. Awaiting RUN command.',
        'toast.loop.on': '🔁 Loop activated. Will be sent to MCU along with RUN.',
        'toast.loop.off': '⏹️ Loop deactivated. Queue will end normally.',
        'toast.alerts.cleared': 'Alert log cleared.',
        'toast.lib.cleared': 'Library reset.',
        'toast.vel.error': 'Error: Firmware will reject. Minimum velocity = 50µs.',
        'toast.motor.enabled': '⚡ Motor {motor} driver ENABLED (EN → LOW).',
        'toast.motor.disabled': '🔌 Motor {motor} driver DISABLED (EN → HIGH).',
        'toast.eeprom.invalid': 'Invalid slot. Choose between 0 and 4.',
        'toast.dict.saved': 'Dictionary Overrides Saved.',
        'toast.vis.saved': 'Alert Visibility Preferences Saved.',

        // ── Settings Modal System ─────────────────────────────────────────
        'settings.tab.dict': 'Dictionary',
        'settings.tab.vis': 'Visibility',
        'settings.tab.motor': 'Motor Config',
        'settings.dict.title': 'Dictionary Override',
        'settings.dict.desc': 'Modify these values to redefine hardware responses and internal logs. Leave empty to use system defaults.',
        'settings.dict.save': 'Save Overrides',
        'settings.vis.title': 'Alert Visibility',
        'settings.vis.desc': 'Uncheck an alert type to silence it continuously across the local session. Settings apply immediately.',
        'settings.motor.title': 'Hardware Definition',
        'settings.motor.desc': 'Define Pulses Per Revolution. Changing this directly adjusts the limits in the Jog Scrubber.',

        // ── Dictionary Categories ─────────────────────────────────────────
        'dict.cat.ui': 'Interface & Labels',
        'dict.cat.modals': 'Modals & Dialogs',
        'dict.cat.alerts': 'Toasts & System Alerts',
        'dict.cat.hw': 'Hardware Responses (RX)',
        'dict.cat.tx': 'Serial Injections (TX)',
        'dict.cat.other': 'Other Strings',


        // ── TX Decorators (sendRawString) ─────────────────────────────────
        'tx.run': 'Injected Signal: 01 [EXECUTE FULL QUEUE]',
        'tx.stop': 'Injected Signal: 02 [EMERGENCY CUT]',
        'tx.loop': 'Injected Signal: 03 [CLOSE CIRCUIT IN LOOP]',
        'tx.pause': 'Injected Signal: 04 [OVERRIDE GLOBAL PAUSE → {ms}ms]',
        'tx.packet': 'Sending H8P Hex packet… [{p0}, {p1}]',
        'tx.enable': 'Injected Signal: 16 [ENABLE MOTOR {motor}]',
        'tx.disable': 'Injected Signal: 17 [DISABLE MOTOR {motor}]',

        // ── Hardware Responses (AVR → UI) ─────────────────────────────────
        'hw.A0': '🔗 [AVR Status]: Hardware paired. Timer1 Dual-Channel synchronized at 16MHz.',
        'hw.B0': '⚙️ [Execution]: Master queue started. Dispatching independent pulses to M1 and M2.',
        'hw.B1': '🛡️ [Emergency]: Interrupts atomically cut. Both motors unlocked and RAM cleared.',
        'hw.B4': '🔁 [Force Loop]: Firmware will repeat this movement queue in an infinite loop for both.',
        'hw.B5': '✅ [Finished]: All motors completed the instruction queue. Standby.',
        'hw.B6': '⏹️ [Loop OFF]: RepeatAll mode deactivated. Queue will end after execution.',
        'hw.E0': '🚫 [Operation Denied]: Motors are already executing. Wait or fire a STOP.',
        'hw.E1': '⚠️ [Buffer Empty]: MCU queue is idle. Inject a command packet before RUN.',
        'hw.E2': '🔴 [Overflow]: SRAM reached safe limit (20 slots). Fire a STOP (02) to clear memory buffer.',
        'hw.E3': '🛑 [Safety Clamp Active]: Rejected! The interval entered would cause freezing. Minimum = 50µs.',
        'hw.B2': '⏳ [Delay Handler]: Global Pause Modifier injected for {ms}ms fixed.',
        'hw.B7': '⚡ [Driver ON]: Motor {motor} driver enabled. Holding torque active.',
        'hw.B8': '🔌 [Driver OFF]: Motor {motor} driver disabled. Free-spinning.',
        'hw.allocated': '💾 [Allocated]: Slot #{idx} [{motor}] written to RAM → ({details})',
        'hw.step.unit': 'Steps',
        'hw.int.unit': 'int.',
    },

    'pt-BR': {
        // ── App ──────────────────────────────────────────────────────────
        'app.title': 'StepperControl Pro',

        // ── Header ───────────────────────────────────────────────────────
        'header.library': 'Biblioteca',
        'header.offline': 'Offline',
        'header.connected': 'Conectado',
        'header.connect': 'Conectar Hardware',
        'header.disconnect': 'Desconectar',
        'lang.label': 'Idioma',

        // ── Command Builder ───────────────────────────────────────────────
        'cmd.card.title': 'Novo Comando de Movimento',
        'cmd.card.subtitle': 'Configure os parâmetros técnicos para inserir na fila do microcontrolador.',
        'cmd.motor.label': 'Motor Alvo',
        'cmd.motor.1': 'Motor 1',
        'cmd.motor.2': 'Motor 2',
        'cmd.step.label': 'Quantidade de Passos',
        'cmd.vel.label': 'Intervalo do Pulso (µs)',
        'cmd.vel.hint': 'MÍN 50',
        'cmd.dir.label': 'Direção Física',
        'cmd.dir.fwd': '1 (Sentido A — Avanço)',
        'cmd.dir.rev': '0 (Sentido B — Recuo)',
        'cmd.repeat.label': 'Ciclos de Repetição',
        'cmd.pause.label': 'Pausa Pós-Execução',
        'cmd.pause.placeholder': 'Ex: 500 (Opcional)',
        'cmd.btn.clear': 'Limpar Fila Atual',
        'cmd.btn.save': 'Salvar Sequência',
        'cmd.btn.add': 'Adicionar à Fila',
        'cmd.btn.preset': 'Nova Ação',

        // ── Global Config Cards ───────────────────────────────────────────
        'fastaction.title': 'Ações Rápidas (Key 18)',
        'jog.title': 'Modo Jog (Key 1B)',
        'global.pause.title': 'Pausa Global (Key 04)',
        'global.pause.desc': 'Delay fixo injetado automaticamente entre todas as linhas da fila do motor.',
        'global.pause.placeholder': 'Ex: 1000',
        'global.pause.btn': 'Aplicar',
        'global.loop.title': 'Mestre de Loop (Key 03)',
        'global.loop.desc': 'Sobrescreve a fila para reiniciar do zero infinitamente após finalizar.',
        'global.loop.btn': 'Ativar RepeatAll Fila',

        // ── Telemetry & Hardware Panel ────────────────────────────────────
        'hw.panel.title': 'Painel Hardware',
        'hw.step.btn': 'Passo Único',
        'hw.enable.title': 'Habilitar/Desabilitar driver do motor',
        'tel.driver.on': 'Driver ON',
        'tel.driver.off': 'Driver OFF',

        // ── Alert History Floating Button ─────────────────────────────────
        'alerts.btn.title': 'Histórico de Alertas',
        'alerts.btn.aria': 'Ver histórico de alertas recentes',

        // ── Library Modal ─────────────────────────────────────────────────
        'lib.modal.title': 'Biblioteca de Sequências',
        'lib.modal.subtitle': 'Gerencie e execute conjuntos de comandos salvos localmente.',
        'lib.empty.title': 'Nenhuma sequência salva',
        'lib.empty.desc': 'Desenvolva movimentos e clique no botão de salvar para iniciar sua biblioteca técnica.',
        'lib.footer.info': 'Dados persistidos via LocalStorage.',
        'lib.btn.clearall': 'Limpar Tudo',

        // ── Library List Items (dynamic) ──────────────────────────────────
        'lib.item.instructions': '{n} instrução(ões)',
        'lib.item.load.title': 'Carregar na Placa',
        'lib.item.delete.title': 'Excluir Conjunto Completo',
        'lib.line.edit.title': 'Editar Linha H8P',
        'lib.line.delete.title': 'Remover Linha',
        'lib.line.save.btn': 'Salvar',

        // ── Alerts Modal ──────────────────────────────────────────────────
        'alertsmodal.title': 'Alertas Recentes',
        'alertsmodal.subtitle': 'Sessão Local',
        'alertsmodal.empty.title': 'Sem ocorrências',
        'alertsmodal.empty.desc': 'Os alertas do sistema aparecerão aqui.',
        'alertsmodal.btn.clear': 'Esvaziar Histórico',

        // ── Generic Modals ────────────────────────────────────────────────
        'modal.btn.cancel': 'Cancelar',
        'modal.btn.save': 'Salvar',
        'modal.btn.confirm': 'Confirmar',

        // ── Confirm Modal Calls ───────────────────────────────────────────
        'confirm.seq.delete.title': 'Excluir Sequência',
        'confirm.seq.delete.text': 'Tem certeza que deseja remover "{name}"?',
        'confirm.lib.clearall.title': 'Limpar Biblioteca',
        'confirm.lib.clearall.text': 'Tem certeza que deseja apagar TODA a biblioteca? Esta ação não pode ser desfeita.',

        // ── Prompt Modal Calls ────────────────────────────────────────────
        'prompt.seq.save.title': 'Salvar Sequência',
        'prompt.eeprom.title': 'Gravar Preset EEPROM (Slot 0-4):',
        'seq.default.name': 'Sequência',

        // ── Button / Shared ───────────────────────────────────────────────
        'btn.close': 'Fechar',

        // ── Toast Messages ────────────────────────────────────────────────
        'toast.connect.success': 'Hardware Serial pareado ({baud} bps).',
        'toast.disconnect': 'Placa Arduino desconectada com segurança.',
        'toast.connect.error': 'Falha WebSerial: {msg}',
        'toast.io.absent': 'Conexão I/O ausente.',
        'toast.queue.added.offline': 'Passo adicionado à fila local (Offline)',
        'toast.queue.cleared': 'Fila da sessão limpa localmente.',
        'toast.seq.saved': 'Sequência "{name}" salva com sucesso!',
        'toast.seq.deleted': 'Sequência removida.',
        'toast.seq.line.deleted': 'Linha [{idx}] removida da biblioteca.',
        'toast.seq.autodelete': 'Sequência auto-removida por falta de instruções.',
        'toast.seq.line.saved': 'Sintaxe da linha [{idx}] salva via Editor Inline.',
        'toast.seq.line.empty': 'Erro: O comando interno não pode ser vazio.',
        'toast.seq.load.noconn': 'Conecte o hardware antes de carregar a biblioteca.',
        'toast.seq.loading': '📥 Carregando manobra: {name}...',
        'toast.seq.loaded': '✅ Sequência carregada na memória com sucesso. Aguardando comando RUN.',
        'toast.loop.on': '🔁 Loop ativado. Será enviado ao MCU junto com o RUN.',
        'toast.loop.off': '⏹️ Loop desativado. A fila encerrará normalmente.',
        'toast.alerts.cleared': 'Log de alertas esvaziado.',
        'toast.lib.cleared': 'Biblioteca resetada.',
        'toast.vel.error': 'Erro: O firmware rejeitará. Velocity mínimo = 50µs.',
        'toast.motor.enabled': '⚡ Driver do Motor {motor} HABILITADO (EN → LOW).',
        'toast.motor.disabled': '🔌 Driver do Motor {motor} DESABILITADO (EN → HIGH).',
        'toast.eeprom.invalid': 'Slot inválido. Escolha entre 0 e 4.',
        'toast.dict.saved': 'Substituições do Dicionário Salvas.',
        'toast.vis.saved': 'Preferências de Visibilidade Salvas.',

        // ── Settings Modal System ─────────────────────────────────────────
        'settings.tab.dict': 'Dicionário',
        'settings.tab.vis': 'Visibilidade',
        'settings.tab.motor': 'Cfg Motor',
        'settings.dict.title': 'Substituição de Dicionário',
        'settings.dict.desc': 'Modifique esses valores para redefinir as respostas de hardware e logs internos. Deixe vazio para usar os padrões do sistema.',
        'settings.dict.save': 'Salvar Alteraçoes',
        'settings.vis.title': 'Visibilidade de Alertas',
        'settings.vis.desc': 'Desmarque um tipo de alerta para silenciá-lo continuamente durante a sessão local. As configurações são aplicadas imediatamente.',
        'settings.motor.title': 'Definição de Hardware',
        'settings.motor.desc': 'Defina os Pulsos Por Revolução. Alterar isso ajusta diretamente os limites no Jog Scrubber.',

        // ── Dictionary Categories ─────────────────────────────────────────
        'dict.cat.ui': 'Interface e Rótulos',
        'dict.cat.modals': 'Modais e Diálogos',
        'dict.cat.alerts': 'Toasts e Alertas',
        'dict.cat.hw': 'Respostas de Hardware (RX)',
        'dict.cat.tx': 'Informações Seriais (TX)',
        'dict.cat.other': 'Outras Strings',


        // ── TX Decorators (sendRawString) ─────────────────────────────────
        'tx.run': 'Injetado Sinal: 01 [EXECUTAR FILA INTEIRA]',
        'tx.stop': 'Injetado Sinal: 02 [CORTE DE EMERGÊNCIA]',
        'tx.loop': 'Injetado Sinal: 03 [FECHAR CIRCUITO EM LOOP]',
        'tx.pause': 'Injetado Sinal: 04 [SUBRESCREVER PAUSA GLOBAL → {ms}ms]',
        'tx.packet': 'Enviando pacote Hex H8P… [{p0}, {p1}]',
        'tx.enable': 'Injetado Sinal: 16 [HABILITAR MOTOR {motor}]',
        'tx.disable': 'Injetado Sinal: 17 [DESABILITAR MOTOR {motor}]',

        // ── Hardware Responses (AVR → UI) ─────────────────────────────────
        'hw.A0': '🔗 [AVR Status]: Hardware pareado. Timer1 Dual-Channel sincronizado a 16MHz.',
        'hw.B0': '⚙️ [Execução]: Fila mestre iniciada. Despachando pulsos independentes para M1 e M2.',
        'hw.B1': '🛡️ [Emergency]: Interrupções cortadas atomicamente. Ambos motores destravados e RAM limpa.',
        'hw.B4': '🔁 [Force Loop]: O firmware repetirá esta fila de movimentos em laço infinito para ambos.',
        'hw.B5': '✅ [Finalizado]: Todos os motores concluíram a fila de instruções. Standby.',
        'hw.B6': '⏹️ [Loop OFF]: Modo RepeatAll desativado. A fila encerrará ao final da execução.',
        'hw.E0': '🚫 [Operação Negada]: Os motores já estão executando. Aguarde ou dispare um STOP.',
        'hw.E1': '⚠️ [Buffer Vazio]: A fila do MCU está ociosa. Injete um pacote de comandos antes de dar RUN.',
        'hw.E2': '🔴 [Overflow]: A SRAM atingiu o limite seguro (20 slots). Dispare um STOP (02) para limpar o buffer de memória.',
        'hw.E3': '🛑 [Safety Clamp Ativo]: Rejeitado! O intervalo inserido causará congelamento. Minimum = 50µs.',
        'hw.B2': '⏳ [Delay Handler]: Modificador Global de Pausas injetado para {ms}ms fixos.',
        'hw.B7': '⚡ [Driver ON]: Driver do Motor {motor} habilitado. Torque de retenção ativo.',
        'hw.B8': '🔌 [Driver OFF]: Driver do Motor {motor} desabilitado. Eixo livre.',
        'hw.allocated': '💾 [Alocado]: Slot N° {idx} [{motor}] gravado na RAM → ({details})',
        'hw.step.unit': 'Passos',
        'hw.int.unit': 'int.',
    }
};

// ── Engine ────────────────────────────────────────────────────────────────────

let currentLang = localStorage.getItem('stepper_lang') || 'en-US';
let customTranslations = JSON.parse(localStorage.getItem('stepper_custom_i18n') || '{}');

/**
 * Translate a key with optional variable interpolation.
 * @param {string} key
 * @param {Record<string,string|number>} [vars]
 * @returns {string}
 */
function t(key, vars = {}) {
    const defaultDict = translations['en-US'];
    const currentDict = translations[currentLang] || defaultDict;
    
    let str = key;
    if (customTranslations[key]) {
        str = customTranslations[key];
    } else if (Object.prototype.hasOwnProperty.call(currentDict, key)) {
        str = currentDict[key];
    } else if (Object.prototype.hasOwnProperty.call(defaultDict, key)) {
        str = defaultDict[key];
    }

    for (const [k, v] of Object.entries(vars)) {
        str = str.replaceAll(`{${k}}`, v);
    }
    return str;
}

/** Apply all static data-i18n* attributes in the DOM. */
/** 
 * Scans the DOM for [data-i18n*] attributes and injects translated text.
 * Handles textContent, placeholder, title, and aria-label.
 */
function applyTranslations() {
    document.querySelectorAll('[data-i18n]').forEach(el => {
        el.textContent = t(el.getAttribute('data-i18n'));
    });
    document.querySelectorAll('[data-i18n-placeholder]').forEach(el => {
        el.placeholder = t(el.getAttribute('data-i18n-placeholder'));
    });
    document.querySelectorAll('[data-i18n-title]').forEach(el => {
        el.title = t(el.getAttribute('data-i18n-title'));
    });
    document.querySelectorAll('[data-i18n-aria]').forEach(el => {
        el.setAttribute('aria-label', t(el.getAttribute('data-i18n-aria')));
    });
    document.documentElement.lang = currentLang;
    document.title = t('app.title');
}

/** Highlight the active language button in the header toggle. */
/** 
 * Updates the visual state of language toggle buttons in the UI. 
 */
function updateLangButtons() {
    ['en-US', 'pt-BR'].forEach(lang => {
        const btn = document.querySelector(`[data-lang-btn="${lang}"]`);
        if (!btn) return;
        if (lang === currentLang) {
            btn.classList.add('bg-amber', 'text-navy', 'font-bold');
            btn.classList.remove('text-cream/50');
        } else {
            btn.classList.remove('bg-amber', 'text-navy', 'font-bold');
            btn.classList.add('text-cream/50');
        }
    });
}

/**
 * Switch the active language.
 * @param {'en-US'|'pt-BR'} lang
 */
function setLanguage(lang) {
    if (!translations[lang]) return;
    currentLang = lang;
    localStorage.setItem('stepper_lang', lang);
    applyTranslations();
    updateLangButtons();
    // Re-render open dynamic panels so their strings update immediately
    if (typeof renderLibrary === 'function') renderLibrary();
    if (typeof renderAlerts === 'function') renderAlerts();
    // Notify app.js to refresh connection-dependent UI text
    document.dispatchEvent(new CustomEvent('langchange'));
}

// ── Bootstrap (runs immediately — scripts are at bottom of <body>) ────────────
applyTranslations();
updateLangButtons();
