const term = document.getElementById("term");
const input = document.getElementById("inputLine");
let lastActivity = Date.now();
const SESSION_LIMIT = 3 * 60 * 1000;
const history = [];
let historyIndex = -1;

const commands = [
    "led r on",
    "led r off",
    "led b on",
    "led b off",
    "led all on",
    "led all off",
    "status",
    "help",
    "clear"
];

function showWelcome() {
    term.innerHTML = "";
    const title = "<span class='title'>ESP32 RETRO TERMINAL</span><br>";
    const welcomeMsg = "Bienvenido. Escribe 'help' para ver la lista de comandos.<br>";
    const separator = "<hr class='separator-line'>";
    term.innerHTML = title + welcomeMsg + separator;
}

function appendLine(text) {
    term.innerHTML += text.replace(/</g, "&lt;").replace(/>/g, "&gt;") + "<br>";
    term.scrollTop = term.scrollHeight;
}

function tryAutocomplete(current) {
    if (!current) return null;
    const matches = commands.filter(c => c.startsWith(current));
    if (matches.length === 0) return null;
    return matches[0];
}

function sendCommand(cmd) {
    appendLine("> " + cmd);
    fetch("/cmd?c=" + encodeURIComponent(cmd))
        .then(resp => {
            if (!resp.ok) {
                throw new Error("Sesión expirada. Redirigiendo al login...");
            }
            return resp.text();
        })
        .then(text => {
            appendLine(text);
            lastActivity = Date.now();
        })
        .catch(e => {
            appendLine("[ERROR] " + e.message);
            setTimeout(() => { doLogout(); }, 1200);
        });
}

input.addEventListener("keydown", function(e) {
    if (e.key === "Enter") {
        const raw = input.value.trim();
        if (raw.length === 0) return;
        const cmd = raw.toLowerCase();
        if (cmd === "clear") {
            showWelcome();
        } else {
            sendCommand(cmd);
            history.unshift(cmd);
            historyIndex = -1;
        }
        input.value = "";
        lastActivity = Date.now();
        e.preventDefault();
    } else if (e.key === "ArrowUp") {
        if (history.length === 0) return;
        if (historyIndex + 1 < history.length) historyIndex++;
        input.value = history[historyIndex];
        e.preventDefault();
    } else if (e.key === "ArrowDown") {
        if (historyIndex > 0) {
            historyIndex--;
            input.value = history[historyIndex];
        } else {
            historyIndex = -1;
            input.value = "";
        }
        e.preventDefault();
    } else if (e.key === "Tab") {
        e.preventDefault();
        const current = input.value.trim().toLowerCase();
        const auto = tryAutocomplete(current);
        if (auto) {
            input.value = auto + " ";
        }
    }
});

setInterval(() => {
    if (Date.now() - lastActivity > SESSION_LIMIT) {
        appendLine("[INFO] Sesión expirada por inactividad. Cerrando...");
        setTimeout(() => { doLogout(); }, 800);
    }
}, 2000);

showWelcome();

function doLogout() {
    fetch("/logout").then(() => { window.location.href = "/"; });
}

