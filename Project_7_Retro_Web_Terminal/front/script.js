/**
 * ============================================================================
 * ARCHIVO: script.js
 * ============================================================================
 * 
 * RESUMEN:
 * Script principal del frontend que gestiona toda la funcionalidad del cliente.
 * Este archivo contiene:
 * 
 * 1. Gestión de orientación móvil:
 *    - Fuerza orientación vertical en dispositivos móviles
 *    - Detecta cambios de orientación y los corrige
 * 
 * 2. Sistema de autenticación:
 *    - Manejo del formulario de login
 *    - Envío de credenciales al servidor
 *    - Redirección según resultado
 * 
 * 3. Terminal web retro:
 *    - Interfaz de terminal con prompt
 *    - Historial de comandos (flechas arriba/abajo)
 *    - Autocompletado con TAB
 *    - Envío de comandos al servidor
 *    - Auto-logout por inactividad (3 minutos)
 * 
 * 4. Actualización en tiempo real:
 *    - Reloj actualizado cada segundo
 *    - Temperatura actualizada cada segundo desde /temperature
 * 
 * 5. Panel de control (slider.html):
 *    - Control de ventilador con 4 modos (apagado, horario, temperatura, manual)
 *    - Gestión de horarios programados
 *    - Control de velocidad con slider
 *    - Monitoreo de temperatura para control automático
 * 
 * 6. Navegación:
 *    - Funciones para ir al dashboard
 *    - Funciones de logout/suspender sesión
 * 
 * ============================================================================
 * ÍNDICE DE SECCIONES:
 * ============================================================================
 * Sección 1: MÓDULO PRINCIPAL (IIFE) se encuentra en las líneas 42 a 1362
 *   - Subsección 1.1: CONSTANTES se encuentra en las líneas 48 a 62
 *   - Subsección 1.2: INICIALIZACIÓN Y DEBUG se encuentra en las líneas 64 a 66
 *   - Subsección 1.3: ESTADO PRIVADO DEL MÓDULO se encuentra en las líneas 68 a 100
 *   - Subsección 1.4: GESTIÓN DE ORIENTACIÓN MÓVIL se encuentra en las líneas 102 a 152
 *   - Subsección 1.5: LÓGICA DE LOGIN se encuentra en las líneas 154 a 195
 *   - Subsección 1.6: RELOJ EN TIEMPO REAL se encuentra en las líneas 197 a 212
 *   - Subsección 1.7: TEMPERATURA EN TIEMPO REAL se encuentra en las líneas 214 a 276
 *   - Subsección 1.8: LÓGICA DE LA TERMINAL se encuentra en las líneas 278 a 379
 *   - Subsección 1.9: FUNCIONALIDAD PARA LA PÁGINA SLIDER se encuentra en las líneas 381 a 441
 *   - Subsección 1.10: CONTROL DE VENTILADOR se encuentra en las líneas 442 a 616
 *   - Subsección 1.11: FUNCIONES DEL MODO HORARIO se encuentra en las líneas 617 a 1119
 *   - Subsección 1.12: FUNCIONES DEL MODO TEMPERATURA se encuentra en las líneas 1120 a 1333
 *   - Subsección 1.13: EXPOSICIÓN PÚBLICA se encuentra en las líneas 1335 a 1360
 * Sección 2: FUNCIONES DE COMPATIBILIDAD (Wrappers globales) se encuentra en las líneas 1364 a 1423
 * ============================================================================
 */

// ===== MÓDULO PRINCIPAL: Patrón IIFE para encapsulación =====
// Todo el código está encapsulado dentro de este módulo para evitar variables globales.
// El estado se mantiene privado dentro del módulo y solo se exponen funciones públicas.
(function() {
    'use strict';

    // ===== SECCIÓN: CONSTANTES =====
    const SESSION_TIMEOUT_MS = 3 * 60 * 1000;        // 3 minutos de inactividad
    const UPDATE_INTERVAL_MS = 1000;                 // Intervalo de actualización (1 segundo)
    const ORIENTATION_LOCK_DELAY_MS = 100;          // Delay para bloqueo de orientación
    const MOBILE_BREAKPOINT = 768;                   // Ancho máximo para considerar móvil
    const TEMP_UPDATE_INTERVAL_MS = 1000;            // Intervalo de actualización de temperatura
    
    // Comandos disponibles en la terminal
    const TERMINAL_COMMANDS = [
        "led y on", "led y off", "led b on", "led b off",
        "led all on", "led all off", "status", "help", "clear"
    ];
    
    // Días de la semana para el modo horario del ventilador
    const WEEK_DAYS = ['lunes', 'martes', 'miercoles', 'jueves', 'viernes', 'sabado', 'domingo'];

    // ===== SECCIÓN: INICIALIZACIÓN Y DEBUG =====
    console.log("=== SCRIPT.JS CARGADO ===");
    console.log("Estado del DOM:", document.readyState);

    // ===== SECCIÓN: ESTADO PRIVADO DEL MÓDULO =====
    // Todas las variables de estado están encapsuladas aquí (no son globales)
    
    // Estado de temperatura
    let isUpdatingTemperature = false;
    
    // Estado de la terminal
    let terminalState = {
        lastActivity: Date.now(),
        history: [],
        historyIndex: -1,
        commands: TERMINAL_COMMANDS
    };
    
    // Estado del ventilador
    let fanState = {
        // Modo manual
        manualPowerState: false,
        manualSpeed: 0,
        // Modo horario
        scheduleSpeed: 0,
        scheduleDay: WEEK_DAYS[0],
        scheduleTime: '00:00',
        scheduleRecords: [],
        selectedScheduleRecord: null,
        // Modo temperatura
        temperatureMode: false,
        currentSpeed: 0,
        temperatureInterval: null
    };
    
    // Estado de visibilidad de página
    let isPageVisible = true;

    // ===== SECCIÓN: GESTIÓN DE ORIENTACIÓN MÓVIL =====
    /**
     * Fuerza la orientación vertical en dispositivos móviles
     * Intenta usar diferentes APIs según el navegador
     */
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

    /**
     * Detecta si el dispositivo es móvil
     * @return {boolean} true si es móvil, false en caso contrario
     */
    function isMobile() {
        const mobileUserAgents = /Android|webOS|iPhone|iPad|iPod|BlackBerry|IEMobile|Opera Mini/i;
        return mobileUserAgents.test(navigator.userAgent) || 
               (window.innerWidth <= MOBILE_BREAKPOINT);
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
            setTimeout(lockOrientation, ORIENTATION_LOCK_DELAY_MS);
        });
        
        // También escuchar cambios de resize
        let resizeTimer;
        window.addEventListener('resize', function() {
            clearTimeout(resizeTimer);
            resizeTimer = setTimeout(lockOrientation, ORIENTATION_LOCK_DELAY_MS);
        });
    }

    // ===== SECCIÓN: LÓGICA DE LOGIN =====
    /**
     * Maneja el formulario de login
     * Envía credenciales al servidor mediante POST /login
     * Redirige al dashboard si el login es exitoso
     */
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

    // ===== SECCIÓN: RELOJ EN TIEMPO REAL =====
    /**
     * Actualiza el reloj cada segundo
     * Formato: HH:MM:SS (24 horas)
     */
    function updateClock() {
        const clockElement = document.getElementById('clock');
        if (clockElement) {
            const now = new Date();
            const timeString = now.toLocaleTimeString('es-ES', { hour12: false });
            clockElement.innerText = timeString;
        }
    }
    setInterval(updateClock, UPDATE_INTERVAL_MS);
    updateClock(); // Primera llamada inmediata
    console.log("[CLOCK] Reloj inicializado");

    // ===== SECCIÓN: TEMPERATURA EN TIEMPO REAL =====
    /**
     * Actualiza la temperatura cada segundo desde el servidor
     * Hace petición GET a /temperature y muestra el valor JSON
     */
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
    setInterval(updateTemperature, TEMP_UPDATE_INTERVAL_MS);
    updateTemperature(); // Primera llamada inmediata
    console.log("[TEMP] Temperatura inicializada - actualización cada", TEMP_UPDATE_INTERVAL_MS, "ms");

    // ===== SECCIÓN: LÓGICA DE LA TERMINAL =====
    /**
     * Gestiona la interfaz de terminal web retro
     * - Historial de comandos con navegación
     * - Autocompletado con TAB
     * - Envío de comandos al servidor
     * - Auto-logout por inactividad
     */
    const term = document.getElementById("term");
    const input = document.getElementById("inputLine");

    if (term && input) {

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
            const matches = terminalState.commands.filter(c => c.startsWith(current));
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
                    terminalState.lastActivity = Date.now();
                })
                .catch(e => {
                    appendLine("[ERROR] " + e.message);
                    setTimeout(() => { window.AppModule.doLogout(); }, 1200);
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
                    terminalState.history.unshift(cmd);
                    terminalState.historyIndex = -1;
                }
                input.value = "";
                terminalState.lastActivity = Date.now();
                e.preventDefault();
            } else if (e.key === "ArrowUp") {
                if (terminalState.history.length === 0) return;
                if (terminalState.historyIndex + 1 < terminalState.history.length) terminalState.historyIndex++;
                input.value = terminalState.history[terminalState.historyIndex];
                e.preventDefault();
            } else if (e.key === "ArrowDown") {
                if (terminalState.historyIndex > 0) {
                    terminalState.historyIndex--;
                    input.value = terminalState.history[terminalState.historyIndex];
                } else {
                    terminalState.historyIndex = -1;
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
            if (Date.now() - terminalState.lastActivity > SESSION_TIMEOUT_MS) {
                appendLine("[INFO] Sesión expirada. Cerrando...");
                setTimeout(() => { window.AppModule.doLogout(); }, 800);
            }
        }, 2000);

        showWelcome();
    }

    // ===== SECCIÓN: FUNCIONALIDAD PARA LA PÁGINA SLIDER =====

    /**
     * Actualiza la temperatura en la página slider
     * Similar a updateTemperature() pero para el elemento específico del slider
     */
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

    // ===== SECCIÓN: CONTROL DE VENTILADOR =====
    /**
     * Establece el modo de operación del ventilador
     * Modos disponibles: 'off', 'schedule', 'temperature', 'manual'
     * 
     * @param {string} mode - Modo a activar
     */
    function setFanMode(mode) {
        // No hacer nada si estamos en modo manual o horario
        const manualView = document.getElementById('fanManualView');
        const scheduleView = document.getElementById('fanScheduleView');
        if ((manualView && manualView.style.display !== 'none') || 
            (scheduleView && scheduleView.style.display !== 'none')) {
            return;
        }
        
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
                'schedule': 'Horario',
                'temperature': 'Temperatura'
            };
            statusText.textContent = modeNames[mode] || 'Desconocido';
        }
        
        // Manejar el modo temperatura
        if (mode === 'temperature') {
            fanState.temperatureMode = true;
            startTemperatureMode();
        } else {
            fanState.temperatureMode = false;
            stopTemperatureMode();
        }
        
        // Si el modo es 'off', apagar el ventilador
        if (mode === 'off') {
            fanState.currentSpeed = 0;
            stopTemperatureMode();
            // Aquí puedes agregar lógica para enviar comando de apagado al ESP32
            console.log('[FAN] Ventilador apagado');
        }
        
        console.log(`[FAN] Modo del ventilador cambiado a: ${mode}`);
    }

    // Función para detectar si es móvil
    function isMobileDevice() {
        return /Android|webOS|iPhone|iPad|iPod|BlackBerry|IEMobile|Opera Mini/i.test(navigator.userAgent) || 
               (window.innerWidth <= 768);
    }

    // Función para entrar al modo manual
    function enterFanManualMode() {
        const normalView = document.getElementById('fanNormalView');
        const manualView = document.getElementById('fanManualView');
        
        if (normalView && manualView) {
            const isMobile = isMobileDevice();
            const transitionTime = isMobile ? 250 : 300; // Más rápido en móvil
            
            normalView.style.display = 'none';
            manualView.style.display = 'block';
            
            // Animación de entrada
            manualView.style.opacity = '0';
            manualView.style.transform = 'translateY(10px)';
            setTimeout(() => {
                manualView.style.transition = `opacity ${transitionTime}ms ease, transform ${transitionTime}ms ease`;
                manualView.style.opacity = '1';
                manualView.style.transform = 'translateY(0)';
            }, 10);
            
            console.log('[FAN] Modo manual activado');
        }
    }

    // Función para salir del modo manual
    function exitFanManualMode() {
        const normalView = document.getElementById('fanNormalView');
        const manualView = document.getElementById('fanManualView');
        
        if (normalView && manualView) {
            const isMobile = isMobileDevice();
            const transitionTime = isMobile ? 250 : 300; // Más rápido en móvil
            
            // Animación de salida
            manualView.style.transition = `opacity ${transitionTime}ms ease, transform ${transitionTime}ms ease`;
            manualView.style.opacity = '0';
            manualView.style.transform = 'translateY(10px)';
            
            setTimeout(() => {
                manualView.style.display = 'none';
                normalView.style.display = 'block';
                
                // Resetear animación para la próxima vez
                manualView.style.opacity = '1';
                manualView.style.transform = 'translateY(0)';
            }, transitionTime);
            
            console.log('[FAN] Modo manual desactivado');
        }
    }

    // Función para alternar el estado de encendido/apagado del ventilador
    function toggleFanPower() {
        fanState.manualPowerState = !fanState.manualPowerState;
        
        const powerIcon = document.getElementById('fanPowerIcon');
        const powerText = document.getElementById('fanPowerText');
        const powerToggle = document.getElementById('fanPowerToggle');
        const slider = document.getElementById('fanSlider');
        
        if (fanState.manualPowerState) {
            powerIcon.textContent = '▶';
            powerText.textContent = 'Encendido';
            powerToggle.classList.add('fan-power-on');
            slider.disabled = false;
            console.log('[FAN] Ventilador encendido');
        } else {
            powerIcon.textContent = '⏸';
            powerText.textContent = 'Apagado';
            powerToggle.classList.remove('fan-power-on');
            slider.disabled = true;
            // Resetear velocidad a 0 cuando se apaga
            slider.value = 0;
            updateFanSlider(0);
            console.log('[FAN] Ventilador apagado');
        }
        
        // Aquí puedes agregar lógica para enviar el comando al ESP32
        // fetch(`/fan/manual?power=${fanState.manualPowerState ? 'on' : 'off'}&speed=${fanState.manualSpeed}`)
        //     .then(resp => resp.text())
        //     .then(data => console.log(data))
        //     .catch(err => console.error(err));
    }

    // Función para actualizar el slider de velocidad
    function updateFanSlider(value) {
        fanState.manualSpeed = parseInt(value);
        
        const sliderValue = document.getElementById('fanSliderValue');
        const sliderFill = document.getElementById('fanSliderFill');
        
        if (sliderValue) {
            sliderValue.textContent = fanState.manualSpeed + '%';
        }
        
        if (sliderFill) {
            sliderFill.style.width = fanState.manualSpeed + '%';
        }
        
        // Si el ventilador está encendido, enviar el comando
        if (fanState.manualPowerState) {
            console.log(`[FAN] Velocidad ajustada a: ${fanState.manualSpeed}%`);
            
            // Aquí puedes agregar lógica para enviar el comando al ESP32
            // fetch(`/fan/manual?power=on&speed=${fanState.manualSpeed}`)
            //     .then(resp => resp.text())
            //     .then(data => console.log(data))
            //     .catch(err => console.error(err));
        }
    }

    // ===== SECCIÓN: FUNCIONES DEL MODO HORARIO =====
    /**
     * Entra al modo horario del ventilador
     * Muestra la interfaz para programar horarios
     */
    function enterFanScheduleMode() {
        const normalView = document.getElementById('fanNormalView');
        const scheduleView = document.getElementById('fanScheduleView');
        
        if (normalView && scheduleView) {
            const isMobile = isMobileDevice();
            const transitionTime = isMobile ? 250 : 300;
            
            // Activar el botón de horario
            const scheduleBtn = document.querySelector('.fan-btn-schedule');
            if (scheduleBtn) {
                scheduleBtn.classList.add('active');
            }
            
            normalView.style.display = 'none';
            scheduleView.style.display = 'block';
            
            // Animación de entrada
            scheduleView.style.opacity = '0';
            scheduleView.style.transform = 'translateY(10px)';
            setTimeout(() => {
                scheduleView.style.transition = `opacity ${transitionTime}ms ease, transform ${transitionTime}ms ease`;
                scheduleView.style.opacity = '1';
                scheduleView.style.transform = 'translateY(0)';
            }, 10);
            
            console.log('[FAN] Modo horario activado');
        }
    }

    // Función para salir del modo horario
    function exitFanScheduleMode() {
        const normalView = document.getElementById('fanNormalView');
        const scheduleView = document.getElementById('fanScheduleView');
        
        if (normalView && scheduleView) {
            const isMobile = isMobileDevice();
            const transitionTime = isMobile ? 250 : 300;
            
            // Animación de salida
            scheduleView.style.transition = `opacity ${transitionTime}ms ease, transform ${transitionTime}ms ease`;
            scheduleView.style.opacity = '0';
            scheduleView.style.transform = 'translateY(10px)';
            
            setTimeout(() => {
                scheduleView.style.display = 'none';
                normalView.style.display = 'block';
                
                // Resetear animación para la próxima vez
                scheduleView.style.opacity = '1';
                scheduleView.style.transform = 'translateY(0)';
            }, transitionTime);
            
            console.log('[FAN] Modo horario desactivado');
        }
    }

    // Función para actualizar el slider de velocidad del modo horario
    function updateFanScheduleSlider(value) {
        fanState.scheduleSpeed = parseInt(value);
        
        const sliderValue = document.getElementById('fanScheduleSliderValue');
        const sliderFill = document.getElementById('fanScheduleSliderFill');
        
        if (sliderValue) {
            sliderValue.textContent = fanState.scheduleSpeed + '%';
        }
        
        if (sliderFill) {
            sliderFill.style.width = fanState.scheduleSpeed + '%';
        }
        
        console.log(`[FAN SCHEDULE] Velocidad ajustada a: ${fanState.scheduleSpeed}%`);
    }

    // Función para borrar la configuración actual del modo horario
    function clearFanSchedule() {
        // Resetear valores
        fanState.scheduleSpeed = 0;
        fanState.scheduleDay = 'lunes';
        fanState.scheduleTime = '00:00';
        
        // Resetear UI
        const slider = document.getElementById('fanScheduleSlider');
        const daySelect = document.getElementById('fanScheduleDay');
        const timeInput = document.getElementById('fanScheduleTime');
        
        if (slider) {
            slider.value = 0;
            updateFanScheduleSlider(0);
        }
        
        if (daySelect) {
            daySelect.value = 'lunes';
        }
        
        if (timeInput) {
            timeInput.value = '00:00';
        }
        
        console.log('[FAN SCHEDULE] Configuración borrada');
        
        // Mostrar confirmación visual
        const deleteBtn = event.target.closest('.fan-schedule-btn-delete');
        if (deleteBtn) {
            deleteBtn.style.transform = 'scale(0.95)';
            setTimeout(() => {
                deleteBtn.style.transform = 'scale(1)';
            }, 150);
        }
    }

    // Función para registrar el horario programado
    function registerFanSchedule() {
        const daySelect = document.getElementById('fanScheduleDay');
        const timeInput = document.getElementById('fanScheduleTime');
        
        if (!daySelect || !timeInput) {
            return;
        }
        
        fanState.scheduleDay = daySelect.value;
        fanState.scheduleTime = timeInput.value;
        
        // Crear el registro
        const record = {
            id: Date.now(), // ID único basado en timestamp
            day: fanState.scheduleDay,
            time: fanState.scheduleTime,
            speed: fanState.scheduleSpeed,
            date: new Date().toLocaleDateString('es-ES'),
            timestamp: new Date().toLocaleString('es-ES')
        };
        
        // Agregar al array de registros
        fanState.scheduleRecords.push(record);
        
        // Actualizar la lista de registros
        updateLogsDisplay();
        
        // Actualizar popup si está abierto
        const popup = document.getElementById('logsPopup');
        if (popup && popup.style.display !== 'none') {
            updateLogsPopupContent();
        }
        
        console.log('[FAN SCHEDULE] Registro creado:', record);
        
        // Mostrar mensaje de confirmación
        showScheduleConfirmationMessage();
        
        // Reiniciar campos (como si se diera a borrar)
        clearFanSchedule();
        
        // Aquí puedes agregar lógica para enviar el registro al ESP32
        // fetch('/fan/schedule', {
        //     method: 'POST',
        //     headers: { 'Content-Type': 'application/json' },
        //     body: JSON.stringify(record)
        // })
        // .then(resp => resp.text())
        // .then(data => console.log(data))
        // .catch(err => console.error(err));
    }

    // Función para mostrar mensaje de confirmación
    function showScheduleConfirmationMessage(message) {
        // Crear o obtener el contenedor del mensaje
        let messageContainer = document.getElementById('scheduleConfirmationMessage');
        
        if (!messageContainer) {
            messageContainer = document.createElement('div');
            messageContainer.id = 'scheduleConfirmationMessage';
            messageContainer.className = 'schedule-confirmation-message';
            document.body.appendChild(messageContainer);
        }
        
        // Configurar el mensaje (usar mensaje personalizado o el por defecto)
        messageContainer.textContent = message || '✓ Registro guardado correctamente';
        messageContainer.style.display = 'block';
        messageContainer.style.opacity = '0';
        messageContainer.style.transform = 'translateY(-20px)';
        
        // Animación de entrada
        setTimeout(() => {
            messageContainer.style.transition = 'opacity 0.3s ease, transform 0.3s ease';
            messageContainer.style.opacity = '1';
            messageContainer.style.transform = 'translateY(0)';
        }, 10);
        
        // Ocultar después de 4 segundos
        setTimeout(() => {
            messageContainer.style.transition = 'opacity 0.3s ease, transform 0.3s ease';
            messageContainer.style.opacity = '0';
            messageContainer.style.transform = 'translateY(-20px)';
            
            setTimeout(() => {
                messageContainer.style.display = 'none';
            }, 300);
        }, 4000);
    }

    // Función para actualizar la visualización de registros (barra clickeable)
    function updateLogsDisplay() {
        const logsList = document.getElementById('logsList');
        if (!logsList) {
            return;
        }
        
        // Limpiar contenido anterior
        logsList.innerHTML = '';
        
        if (fanState.scheduleRecords.length === 0) {
            logsList.innerHTML = '<div class="logs-bar-empty" onclick="window.AppModule.toggleLogsPopup()">No hay registros disponibles</div>';
            return;
        }
        
        // Crear barra clickeable con resumen
        const logsBar = document.createElement('div');
        logsBar.className = 'logs-bar';
        logsBar.onclick = window.AppModule.toggleLogsPopup;
        
        const recordCount = fanState.scheduleRecords.length;
        const latestRecord = fanState.scheduleRecords[fanState.scheduleRecords.length - 1];
        const dayFormatted = latestRecord.day.charAt(0).toUpperCase() + latestRecord.day.slice(1);
        
        logsBar.innerHTML = `
            <div class="logs-bar-content">
                <span class="logs-bar-icon">📋</span>
                <span class="logs-bar-text">
                    <span class="logs-bar-count">${recordCount} registro${recordCount > 1 ? 's' : ''}</span>
                    <span class="logs-bar-latest">Último: ${dayFormatted} ${latestRecord.time} - ${latestRecord.speed}%</span>
                </span>
                <span class="logs-bar-arrow">▼</span>
            </div>
        `;
        
        logsList.appendChild(logsBar);
    }

    // Función para mostrar/ocultar el popup de registros
    function toggleLogsPopup() {
        const popup = document.getElementById('logsPopup');
        
        if (!popup) {
            createLogsPopup();
            return;
        }
        
        if (popup.style.display === 'none' || popup.style.display === '') {
            showLogsPopup();
        } else {
            hideLogsPopup();
        }
    }

    // Función para crear el popup de registros
    function createLogsPopup() {
        const popup = document.createElement('div');
        popup.id = 'logsPopup';
        popup.className = 'logs-popup';
        
        const popupContent = document.createElement('div');
        popupContent.className = 'logs-popup-content';
        
        const popupHeader = document.createElement('div');
        popupHeader.className = 'logs-popup-header';
        popupHeader.innerHTML = `
            <h3 class="logs-popup-title">Registros de Horario</h3>
            <button class="logs-popup-close" onclick="window.AppModule.hideLogsPopup()">×</button>
        `;
        
        const popupList = document.createElement('div');
        popupList.id = 'logsPopupList';
        popupList.className = 'logs-popup-list';
        
        const popupFooter = document.createElement('div');
        popupFooter.id = 'logsPopupFooter';
        popupFooter.className = 'logs-popup-footer';
        
        popupContent.appendChild(popupHeader);
        popupContent.appendChild(popupList);
        popupContent.appendChild(popupFooter);
        popup.appendChild(popupContent);
        
        document.body.appendChild(popup);
        
        // Cerrar al hacer clic fuera del popup
        popup.addEventListener('click', function(e) {
            if (e.target === popup) {
                hideLogsPopup();
            }
        });
        
        updateLogsPopupContent();
        updateLogsPopupFooter();
        showLogsPopup();
    }

    // Función para actualizar el contenido del popup
    function updateLogsPopupContent() {
        const popupList = document.getElementById('logsPopupList');
        if (!popupList) {
            return;
        }
        
        popupList.innerHTML = '';
        
        if (fanState.scheduleRecords.length === 0) {
            popupList.innerHTML = '<p class="logs-popup-empty">No hay registros disponibles</p>';
            return;
        }
        
        // Obtener los últimos 5 registros (más recientes primero)
        const sortedRecords = [...fanState.scheduleRecords].reverse();
        const last5Records = sortedRecords.slice(0, 5);
        
        last5Records.forEach((record, index) => {
            const logItem = document.createElement('div');
            logItem.className = 'logs-popup-item';
            
            // Agregar clase selected si este registro está seleccionado
            if (fanState.selectedScheduleRecord && fanState.selectedScheduleRecord.id === record.id) {
                logItem.classList.add('selected');
            }
            
            // Agregar evento de clic para seleccionar
            logItem.onclick = function() {
                selectScheduleRecord(record);
            };
            
            // Formatear el día con primera letra mayúscula
            const dayFormatted = record.day.charAt(0).toUpperCase() + record.day.slice(1);
            
            logItem.innerHTML = `
                <div class="logs-popup-item-number">${index + 1}</div>
                <div class="logs-popup-item-content">
                    <div class="logs-popup-item-header">
                        <span class="logs-popup-item-day">${dayFormatted}</span>
                        <span class="logs-popup-item-time">${record.time}</span>
                    </div>
                    <div class="logs-popup-item-details">
                        <span class="logs-popup-item-speed">Velocidad: ${record.speed}%</span>
                        <span class="logs-popup-item-date">${record.timestamp}</span>
                    </div>
                </div>
                ${fanState.selectedScheduleRecord && fanState.selectedScheduleRecord.id === record.id ? '<div class="logs-popup-item-check">✓</div>' : ''}
            `;
            
            popupList.appendChild(logItem);
        });
        
        // Actualizar el footer después de actualizar el contenido
        updateLogsPopupFooter();
    }

    // Función para seleccionar un registro
    function selectScheduleRecord(record) {
        // Si se hace clic en el mismo registro, deseleccionar
        if (fanState.selectedScheduleRecord && fanState.selectedScheduleRecord.id === record.id) {
            fanState.selectedScheduleRecord = null;
        } else {
            fanState.selectedScheduleRecord = record;
        }
        
        // Actualizar la visualización
        updateLogsPopupContent();
        updateLogsPopupFooter();
        
        console.log('[FAN SCHEDULE] Registro seleccionado:', fanState.selectedScheduleRecord);
    }

    // Función para actualizar el footer del popup (botón Asignar)
    function updateLogsPopupFooter() {
        const popupFooter = document.getElementById('logsPopupFooter');
        if (!popupFooter) {
            return;
        }
        
        popupFooter.innerHTML = '';
        
        if (fanState.selectedScheduleRecord) {
            const assignButton = document.createElement('button');
            assignButton.className = 'logs-popup-assign-btn';
            assignButton.innerHTML = `
                <span class="logs-popup-assign-icon">✓</span>
                <span class="logs-popup-assign-label">Asignar</span>
            `;
            assignButton.onclick = assignSchedule;
            
            popupFooter.appendChild(assignButton);
            popupFooter.style.display = 'flex';
        } else {
            popupFooter.style.display = 'none';
        }
    }

    // Función para asignar el registro seleccionado
    function assignSchedule() {
        if (!fanState.selectedScheduleRecord) {
            console.warn('[FAN SCHEDULE] No hay registro seleccionado');
            return;
        }
        
        console.log('[FAN SCHEDULE] Asignando registro:', fanState.selectedScheduleRecord);
        
        // Aquí se implementará la lógica para enviar las instrucciones al ESP32
        // Por ahora solo mostramos un mensaje en consola
        console.log(`[FAN SCHEDULE] Instrucciones a enviar:
            - Día: ${fanState.selectedScheduleRecord.day}
            - Hora: ${fanState.selectedScheduleRecord.time}
            - Velocidad: ${fanState.selectedScheduleRecord.speed}%`);
        
        // TODO: Implementar envío de instrucciones al ESP32
        // Ejemplo:
        // fetch('/fan/schedule/assign', {
        //     method: 'POST',
        //     headers: { 'Content-Type': 'application/json' },
        //     body: JSON.stringify({
        //         day: fanState.selectedScheduleRecord.day,
        //         time: fanState.selectedScheduleRecord.time,
        //         speed: fanState.selectedScheduleRecord.speed
        //     })
        // })
        // .then(resp => resp.text())
        // .then(data => {
        //     console.log('[FAN SCHEDULE] Respuesta del servidor:', data);
        //     showScheduleConfirmationMessage('Horario asignado correctamente');
        // })
        // .catch(err => {
        //     console.error('[FAN SCHEDULE] Error al asignar:', err);
        // });
        
        // Cerrar el popup automáticamente antes de mostrar el mensaje
        hideLogsPopup();
        
        // Mostrar mensaje de confirmación después de cerrar el popup
        setTimeout(() => {
            showScheduleConfirmationMessage('Horario asignado correctamente');
        }, 350); // Esperar a que termine la animación de cierre
    }

    // Función para mostrar el popup
    function showLogsPopup() {
        const popup = document.getElementById('logsPopup');
        if (!popup) {
            createLogsPopup();
            return;
        }
        
        // Resetear selección al abrir el popup
        fanState.selectedScheduleRecord = null;
        
        updateLogsPopupContent();
        updateLogsPopupFooter();
        
        popup.style.display = 'flex';
        popup.style.opacity = '0';
        
        setTimeout(() => {
            popup.style.transition = 'opacity 0.3s ease';
            popup.style.opacity = '1';
            
            const content = popup.querySelector('.logs-popup-content');
            if (content) {
                content.style.transform = 'scale(0.9) translateY(-20px)';
                setTimeout(() => {
                    content.style.transition = 'transform 0.3s ease';
                    content.style.transform = 'scale(1) translateY(0)';
                }, 10);
            }
        }, 10);
    }

    // Función para ocultar el popup
    function hideLogsPopup() {
        const popup = document.getElementById('logsPopup');
        if (!popup) {
            return;
        }
        
        // Resetear selección al cerrar
        fanState.selectedScheduleRecord = null;
        
        const content = popup.querySelector('.logs-popup-content');
        if (content) {
            content.style.transition = 'transform 0.3s ease';
            content.style.transform = 'scale(0.9) translateY(-20px)';
        }
        
        popup.style.transition = 'opacity 0.3s ease';
        popup.style.opacity = '0';
        
        setTimeout(() => {
            popup.style.display = 'none';
        }, 300);
    }

    // ===== SECCIÓN: FUNCIONES DEL MODO TEMPERATURA =====
    /**
     * Inicia el modo temperatura del ventilador
     * Monitorea la temperatura y ajusta la velocidad automáticamente:
     * - > 45°C: 100% velocidad
     * - > 30°C: 40% velocidad
     * - < 30°C: Apagado
     */
    function startTemperatureMode() {
        // Si ya hay un intervalo activo, no crear otro
        if (fanState.temperatureInterval !== null) {
            return;
        }
        
        console.log('[FAN TEMPERATURE] Modo temperatura activado');
        
        // Optimización para móvil: intervalo más largo para ahorrar batería
        // Desktop: 1 segundo, Móvil: 2 segundos
        const updateInterval = isMobileDevice() ? 2000 : 1000;
        
        // Primera lectura inmediata
        updateTemperatureForFan();
        
        // Monitorear temperatura periódicamente
        fanState.temperatureInterval = setInterval(() => {
            if (!fanState.temperatureMode) {
                stopTemperatureMode();
                return;
            }
            
            updateTemperatureForFan();
        }, updateInterval);
    }

    // Función auxiliar para actualizar temperatura (reutilizable)
    function updateTemperatureForFan() {
        // No actualizar si la página no está visible (optimización móvil)
        if (!isPageVisible) {
            return;
        }
        
        // Obtener temperatura actual
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
                        console.error("[FAN TEMP] Error parseando JSON:", text);
                        throw e;
                    }
                });
            })
            .then(data => {
                if (data && typeof data.temperature === 'number' && isFinite(data.temperature)) {
                    const temp = data.temperature;
                    controlFanByTemperature(temp);
                }
            })
            .catch(err => {
                if (err.message !== "Unauthorized") {
                    console.error("[FAN TEMP] Error obteniendo temperatura:", err);
                }
            });
    }

    // Función para detener el modo temperatura
    function stopTemperatureMode() {
        if (fanState.temperatureInterval !== null) {
            clearInterval(fanState.temperatureInterval);
            fanState.temperatureInterval = null;
            console.log('[FAN TEMPERATURE] Modo temperatura desactivado');
        }
    }

    // Función para controlar el ventilador basado en la temperatura
    function controlFanByTemperature(temperature) {
        let newSpeed = 0;
        
        if (temperature > 45.0) {
            // Si supera 45°C, encender al 100%
            newSpeed = 100;
        } else if (temperature > 30.0) {
            // Si supera 30°C, encender al 40%
            newSpeed = 40;
        } else {
            // Si está por debajo de 30°C, apagar
            newSpeed = 0;
        }
        
        // Optimización: solo actualizar si hay cambio significativo (evitar actualizaciones innecesarias)
        // En móvil, usar umbral más grande para reducir actualizaciones
        const speedThreshold = isMobileDevice() ? 5 : 1;
        const speedChanged = Math.abs(newSpeed - fanState.currentSpeed) >= speedThreshold;
        
        // Si el ventilador ya estaba encendido y la temperatura aumenta,
        // aumentar la velocidad. Si la temperatura baja, ajustar según la nueva temperatura.
        if (speedChanged) {
            const previousSpeed = fanState.currentSpeed;
            fanState.currentSpeed = newSpeed;
            
            if (newSpeed > previousSpeed) {
                console.log(`[FAN TEMP] Temperatura: ${temperature.toFixed(1)}°C - Aumentando velocidad de ${previousSpeed}% a ${newSpeed}%`);
            } else if (newSpeed < previousSpeed && newSpeed > 0) {
                console.log(`[FAN TEMP] Temperatura: ${temperature.toFixed(1)}°C - Reduciendo velocidad de ${previousSpeed}% a ${newSpeed}%`);
            } else if (newSpeed > 0) {
                console.log(`[FAN TEMP] Temperatura: ${temperature.toFixed(1)}°C - Encendiendo ventilador a ${newSpeed}%`);
            } else {
                console.log(`[FAN TEMP] Temperatura: ${temperature.toFixed(1)}°C - Apagando ventilador`);
            }
            
            // Actualizar estado visual solo cuando hay cambio
            updateFanTemperatureStatus(temperature, fanState.currentSpeed);
            
            // Aquí puedes agregar lógica para enviar el comando al ESP32
            // fetch(`/fan/temperature?speed=${fanState.currentSpeed}`)
            //     .then(resp => resp.text())
            //     .then(data => console.log(data))
            //     .catch(err => console.error(err));
        } else {
            // Actualizar solo la temperatura en el estado (sin cambiar velocidad)
            updateFanTemperatureStatus(temperature, fanState.currentSpeed, true);
        }
    }

    // Función para actualizar el estado visual del modo temperatura
    function updateFanTemperatureStatus(temperature, speed, temperatureOnly = false) {
        const statusText = document.getElementById('fanStatusText');
        if (statusText && fanState.temperatureMode) {
            // Optimización para móvil: formato más compacto
            if (isMobileDevice()) {
                if (speed > 0) {
                    statusText.textContent = `${temperature.toFixed(1)}°C | ${speed}%`;
                } else {
                    statusText.textContent = `${temperature.toFixed(1)}°C | OFF`;
                }
            } else {
                // Formato completo para desktop
                if (speed > 0) {
                    statusText.textContent = `Temperatura: ${temperature.toFixed(1)}°C - ${speed}%`;
                } else {
                    statusText.textContent = `Temperatura: ${temperature.toFixed(1)}°C - Apagado`;
                }
            }
            
            // Agregar clase para indicar estado activo
            if (speed > 0) {
                statusText.classList.add('fan-temp-active');
            } else {
                statusText.classList.remove('fan-temp-active');
            }
        }
    }

    // Inicializar la visualización de registros al cargar
    if (document.getElementById('logsList')) {
        updateLogsDisplay();
    }

    // Optimización: Pausar monitoreo cuando la página no está visible (ahorro de batería en móvil)
    document.addEventListener('visibilitychange', function() {
        isPageVisible = !document.hidden;
        
        if (fanState.temperatureMode) {
            if (isPageVisible) {
                // Reanudar monitoreo si estaba activo
                if (fanState.temperatureInterval === null) {
                    startTemperatureMode();
                }
            } else {
                // Pausar monitoreo cuando la página no está visible
                stopTemperatureMode();
            }
        }
    });

    // Limpiar recursos al salir de la página
    window.addEventListener('beforeunload', function() {
        stopTemperatureMode();
    });

    // Inicializar funcionalidad del slider cuando la página carga
    if (document.getElementById('sliderClock')) {
        // Actualizar reloj cada segundo
        setInterval(updateSliderClock, UPDATE_INTERVAL_MS);
        updateSliderClock();
        
        // Actualizar temperatura cada segundo
        setInterval(updateSliderTemperature, TEMP_UPDATE_INTERVAL_MS);
        updateSliderTemperature();
        
        console.log("[SLIDER] Funcionalidad del slider inicializada");
    }

    /**
     * Cierra la sesión del usuario y redirige al login
     * Hace una petición al servidor para cerrar la sesión
     */
    function doLogout() {
        fetch("/logout")
            .then(() => { 
                window.location.href = "/"; 
            })
            .catch(() => {
                // Redirigir incluso si hay error en la petición
                window.location.href = "/";
            });
    }

    // ===== EXPOSICIÓN PÚBLICA: Solo las funciones necesarias para HTML =====
    // Crear objeto público para exponer funciones que se llaman desde HTML
    window.AppModule = {
        // Navegación
        goToDashboard: function() {
            window.location.href = "/dashboard";
        },
        suspendSession: function() {
            doLogout();
        },
        doLogout: doLogout,
        
        // Control de ventilador
        setFanMode: setFanMode,
        enterFanManualMode: enterFanManualMode,
        exitFanManualMode: exitFanManualMode,
        toggleFanPower: toggleFanPower,
        updateFanSlider: updateFanSlider,
        enterFanScheduleMode: enterFanScheduleMode,
        exitFanScheduleMode: exitFanScheduleMode,
        updateFanScheduleSlider: updateFanScheduleSlider,
        clearFanSchedule: clearFanSchedule,
        registerFanSchedule: registerFanSchedule,
        toggleLogsPopup: toggleLogsPopup,
        hideLogsPopup: hideLogsPopup
    };

})();

// ===== FUNCIONES DE COMPATIBILIDAD: Wrappers globales para HTML =====
// Estas funciones son wrappers globales que delegan a window.AppModule.
// Se mantienen globales únicamente para compatibilidad con atributos onclick en HTML.
// El estado real se mantiene encapsulado dentro del módulo.
function goToDashboard() {
    window.AppModule.goToDashboard();
}

function suspendSession() {
    window.AppModule.suspendSession();
}

function setFanMode(mode) {
    window.AppModule.setFanMode(mode);
}

function enterFanManualMode() {
    window.AppModule.enterFanManualMode();
}

function exitFanManualMode() {
    window.AppModule.exitFanManualMode();
}

function toggleFanPower() {
    window.AppModule.toggleFanPower();
}

function updateFanSlider(value) {
    window.AppModule.updateFanSlider(value);
}

function enterFanScheduleMode() {
    window.AppModule.enterFanScheduleMode();
}

function exitFanScheduleMode() {
    window.AppModule.exitFanScheduleMode();
}

function updateFanScheduleSlider(value) {
    window.AppModule.updateFanScheduleSlider(value);
}

function clearFanSchedule() {
    window.AppModule.clearFanSchedule();
}

function registerFanSchedule() {
    window.AppModule.registerFanSchedule();
}

function toggleLogsPopup() {
    window.AppModule.toggleLogsPopup();
}

function hideLogsPopup() {
    window.AppModule.hideLogsPopup();
}
