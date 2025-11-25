// --- LÓGICA DE LOGIN ---
const loginForm = document.getElementById("loginForm");

if (loginForm) {
    const errorDiv = document.getElementById("error");

    loginForm.addEventListener("submit", async function(e) {
        e.preventDefault();
        errorDiv.textContent = "Verificando...";
        
        const formData = new FormData(loginForm);
        const user = formData.get("user");
        const pass = formData.get("pass");

        try {
            // Enviamos los datos como x-www-form-urlencoded (igual que tu original)
            const res = await fetch("/login", {
                method: "POST",
                headers: { "Content-Type": "application/x-www-form-urlencoded" },
                body: "user=" + encodeURIComponent(user) + "&pass=" + encodeURIComponent(pass)
            });

            const text = await res.text();

            if (res.ok) {
                // Si el ESP32 responde 200 OK, recargamos para ir a la terminal
                window.location.href = "/";
            } else {
                // Si responde 401, mostramos el error
                errorDiv.textContent = text || "Credenciales incorrectas.";
            }
        } catch (err) {
            console.error(err);
            errorDiv.textContent = "Error de conexión con el ESP32.";
        }
    });
}

// --- LÓGICA DE LA TERMINAL ---
const term = document.getElementById("term");
const input = document.getElementById("inputLine");

if (term && input) {
    let lastActivity = Date.now();
    const SESSION_LIMIT = 3 * 60 * 1000; // 3 minutos

    // Historial y autocompletado
    const history = [];
    let historyIndex = -1;
    const commands = [
        "led y on", "led y off", "led b on", "led b off",
        "led all on", "led all off", "status", "help", "clear"
    ];

    // --- RELOJ EN TIEMPO REAL ---
    function updateClock() {
        const now = new Date();
        const timeString = now.toLocaleTimeString('es-ES', { hour12: false });
        const clockElement = document.getElementById('clock');
        if (clockElement) {
            clockElement.innerText = timeString;
        }
    }
    setInterval(updateClock, 1000);
    updateClock(); // Primera llamada inmediata

    function showWelcome() {
        term.innerHTML = "";
        const welcomeMsg = "Sistema iniciado correctamente.<br>Escribe 'help' para ver la lista de comandos.<br>";
        const separator = "<div class='separator-line'></div>";
        term.innerHTML = welcomeMsg + separator;
    }

    function appendLine(text) {
        term.innerHTML += text.replace(/</g, "&lt;").replace(/>/g, "&gt;") + "<br>";
        term.scrollTop = term.scrollHeight;
    }

    // Autocompletar con TAB
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
                    throw new Error("Sesión expirada. Redirigiendo..."); 
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

    // Auto logout por inactividad
    setInterval(() => {
        if (Date.now() - lastActivity > SESSION_LIMIT) {
            appendLine("[INFO] Sesión expirada. Cerrando...");
            setTimeout(() => { doLogout(); }, 800);
        }
    }, 2000);

    showWelcome();
}

// Función de logout
function doLogout() {
    fetch("/logout").then(() => { 
        window.location.href = "/"; 
    }).catch(() => {
        window.location.href = "/";
    });
}