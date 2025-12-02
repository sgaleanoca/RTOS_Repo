// Mensaje de inicio - DEBE aparecer en consola
console.log("=== SCRIPT.JS CARGADO ===");
console.log("Estado del DOM:", document.readyState);

// --- FORZAR ORIENTACIÓN VERTICAL EN MÓVIL ---
function lockOrientation() {
    // Intentar bloquear la orientación a vertical
    if (screen.orientation && screen.orientation.lock) {
        screen.orientation.lock('portrait').catch(function(err) {
            console.log('No se pudo bloquear la orientación:', err);
        });
    } else if (screen.lockOrientation) {
        screen.lockOrientation('portrait');
    } else if (screen.mozLockOrientation) {
        screen.mozLockOrientation('portrait');
    } else if (screen.msLockOrientation) {
        screen.msLockOrientation('portrait');
    }
}

// Detectar si es móvil
function isMobile() {
    return /Android|webOS|iPhone|iPad|iPod|BlackBerry|IEMobile|Opera Mini/i.test(navigator.userAgent) || 
           (window.innerWidth <= 768);
}

// Aplicar bloqueo de orientación si es móvil
if (isMobile()) {
    // Intentar bloquear al cargar
    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', lockOrientation);
    } else {
        lockOrientation();
    }
    
    // Reintentar si cambia la orientación
    window.addEventListener('orientationchange', function() {
        setTimeout(lockOrientation, 100);
    });
    
    // También escuchar cambios de resize
    let resizeTimer;
    window.addEventListener('resize', function() {
        clearTimeout(resizeTimer);
        resizeTimer = setTimeout(lockOrientation, 100);
    });
}

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
                // Si el ESP32 responde 200 OK, redirigimos al dashboard
                window.location.href = "/dashboard";
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

// --- RELOJ EN TIEMPO REAL ---
function updateClock() {
    const clockElement = document.getElementById('clock');
    if (clockElement) {
        const now = new Date();
        const timeString = now.toLocaleTimeString('es-ES', { hour12: false });
        clockElement.innerText = timeString;
    }
}
setInterval(updateClock, 1000);
updateClock(); // Primera llamada inmediata
console.log("[CLOCK] Reloj inicializado");

// --- TEMPERATURA EN TIEMPO REAL ---
let isUpdatingTemperature = false; // Evitar peticiones superpuestas

function updateTemperature() {
    const tempElement = document.getElementById('tempDisplay');
    if (!tempElement) {
        return;
    }
    
    // Evitar peticiones superpuestas
    if (isUpdatingTemperature) {
        return;
    }
    
    isUpdatingTemperature = true;
    
    fetch("/temperature")
        .then(resp => {
            if (!resp.ok) {
                if (resp.status === 401) {
                    return Promise.reject(new Error("Unauthorized"));
                }
                throw new Error(`HTTP ${resp.status}`);
            }
            return resp.text().then(text => {
                try {
                    return JSON.parse(text);
                } catch (e) {
                    console.error("[TEMP] Error parseando JSON:", text);
                    throw e;
                }
            });
        })
        .then(data => {
            if (data && typeof data.temperature === 'number' && isFinite(data.temperature)) {
                const temp = data.temperature;
                if (temp > -900 && temp < 200) {
                    tempElement.innerText = `Temp: ${temp.toFixed(1)}°C`;
                } else {
                    tempElement.innerText = "Temp: --°C";
                }
            } else {
                tempElement.innerText = "Temp: --°C";
            }
        })
        .catch(err => {
            // No hacer nada si es error de autenticación (el usuario no está logueado)
            if (err.message !== "Unauthorized") {
                const tempEl = document.getElementById('tempDisplay');
                if (tempEl) {
                    tempEl.innerText = "Temp: --°C";
                }
            }
        })
        .finally(() => {
            isUpdatingTemperature = false;
        });
}
setInterval(updateTemperature, 1000);
updateTemperature(); // Primera llamada inmediata
console.log("[TEMP] Temperatura inicializada - actualización cada 1 segundo");

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

// Función para ir al dashboard
function goToDashboard() {
    window.location.href = "/dashboard";
}

// Función para suspender (bloquear) la sesión
function suspendSession() {
    doLogout();
}

// --- FUNCIONALIDAD PARA LA PÁGINA SLIDER ---

// Actualizar temperatura en el slider
function updateSliderTemperature() {
    const tempElement = document.getElementById('sliderTemperature');
    if (!tempElement) {
        return;
    }
    
    fetch("/temperature")
        .then(resp => {
            if (!resp.ok) {
                if (resp.status === 401) {
                    return Promise.reject(new Error("Unauthorized"));
                }
                throw new Error(`HTTP ${resp.status}`);
            }
            return resp.text().then(text => {
                try {
                    return JSON.parse(text);
                } catch (e) {
                    console.error("[SLIDER TEMP] Error parseando JSON:", text);
                    throw e;
                }
            });
        })
        .then(data => {
            if (data && typeof data.temperature === 'number' && isFinite(data.temperature)) {
                const temp = data.temperature;
                if (temp > -900 && temp < 200) {
                    tempElement.textContent = `${temp.toFixed(1)}°C`;
                } else {
                    tempElement.textContent = "--°C";
                }
            } else {
                tempElement.textContent = "--°C";
            }
        })
        .catch(err => {
            if (err.message !== "Unauthorized") {
                const tempEl = document.getElementById('sliderTemperature');
                if (tempEl) {
                    tempEl.textContent = "--°C";
                }
            }
        });
}

// Actualizar hora en el slider
function updateSliderClock() {
    const clockElement = document.getElementById('sliderClock');
    if (clockElement) {
        const now = new Date();
        const timeString = now.toLocaleTimeString('es-ES', { hour12: false });
        clockElement.textContent = timeString;
    }
}

// Función para establecer el modo del ventilador
function setFanMode(mode) {
    // Remover clase active de todos los botones
    const buttons = document.querySelectorAll('.fan-btn');
    buttons.forEach(btn => btn.classList.remove('active'));
    
    // Agregar clase active al botón seleccionado
    const selectedBtn = document.querySelector(`.fan-btn[data-mode="${mode}"]`);
    if (selectedBtn) {
        selectedBtn.classList.add('active');
    }
    
    // Actualizar el texto de estado
    const statusText = document.getElementById('fanStatusText');
    if (statusText) {
        const modeNames = {
            'off': 'Apagado',
            'silent': 'Silencioso',
            'performance': 'Rendimiento',
            'turbo': 'Turbo'
        };
        statusText.textContent = modeNames[mode] || 'Desconocido';
    }
    
    // Aquí puedes agregar lógica para enviar el comando al ESP32
    console.log(`[FAN] Modo del ventilador cambiado a: ${mode}`);
    
    // Ejemplo: enviar comando al servidor (puedes implementar esto más adelante)
    // fetch(`/fan?mode=${mode}`)
    //     .then(resp => resp.text())
    //     .then(data => console.log(data))
    //     .catch(err => console.error(err));
}

// Inicializar funcionalidad del slider cuando la página carga
if (document.getElementById('sliderClock')) {
    // Actualizar reloj cada segundo
    setInterval(updateSliderClock, 1000);
    updateSliderClock();
    
    // Actualizar temperatura cada segundo
    setInterval(updateSliderTemperature, 1000);
    updateSliderTemperature();
    
    console.log("[SLIDER] Funcionalidad del slider inicializada");
}

// Función de logout
function doLogout() {
    fetch("/logout").then(() => { 
        window.location.href = "/"; 
    }).catch(() => {
        window.location.href = "/";
    });
}