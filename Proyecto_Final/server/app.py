"""
============================================================================
ARCHIVO: app.py
============================================================================

RESUMEN:
Servidor Flask que actúa como frontend y proxy entre el cliente web y el ESP32.
Este servidor se ejecuta en la Raspberry Pi y maneja:
- Servicio de páginas HTML (templates)
- Servicio de archivos estáticos (CSS, JS)
- Proxy de API hacia el ESP32
- Autenticación y gestión de sesiones

Arquitectura:
- Frontend: Flask sirve HTML/CSS/JS desde templates/ y static/
- Backend: ESP32 expone API REST liviana (/estado, /modo, etc.)
- Comunicación: Flask hace requests HTTP al ESP32

============================================================================
"""

from flask import Flask, render_template, request, jsonify, redirect, url_for, session
import requests
from functools import wraps
import time

# ===== CONFIGURACIÓN =====
ESP32_IP = "http://192.168.4.1"  # IP del ESP32 en la red
SESSION_TIMEOUT = 180  # Timeout de sesión en segundos (3 minutos)

app = Flask(__name__)
app.secret_key = 'esp32_secret_key_change_in_production'  # Cambiar en producción

# ===== DECORADOR DE AUTENTICACIÓN =====
def login_required(f):
    """Decorador para proteger rutas que requieren autenticación"""
    @wraps(f)
    def decorated_function(*args, **kwargs):
        if 'authenticated' not in session or not session['authenticated']:
            return redirect(url_for('login'))
        # Verificar timeout de sesión
        if 'last_activity' in session:
            if time.time() - session['last_activity'] > SESSION_TIMEOUT:
                session.clear()
                return redirect(url_for('login'))
        session['last_activity'] = time.time()
        return f(*args, **kwargs)
    return decorated_function

# ===== RUTAS DE PÁGINAS WEB =====

@app.route("/")
def home():
    """Página raíz: redirige a login o dashboard según autenticación"""
    if 'authenticated' in session and session['authenticated']:
        return redirect(url_for('dashboard'))
    return redirect(url_for('login'))

@app.route("/login", methods=["GET", "POST"])
def login():
    """Página y handler de login"""
    if request.method == "POST":
        user = request.form.get('user')
        password = request.form.get('pass')
        
        # Validar credenciales (root/matrix123)
        if user == "root" and password == "matrix123":
            session['authenticated'] = True
            session['last_activity'] = time.time()
            return jsonify({"status": "OK"}), 200
        else:
            return jsonify({"error": "Usuario o contraseña incorrectos"}), 401
    
    # GET: mostrar página de login
    return render_template("login.html")

@app.route("/logout")
@login_required
def logout():
    """Cerrar sesión y redirigir a login"""
    session.clear()
    return redirect(url_for('login'))

@app.route("/dashboard")
@login_required
def dashboard():
    """Página principal del dashboard"""
    return render_template("dashboard.html")

@app.route("/terminal")
@login_required
def terminal():
    """Página de terminal web retro"""
    return render_template("terminal.html")

@app.route("/slider")
@login_required
def slider():
    """Página de panel de control (slider)"""
    return render_template("slider.html")

# ===== RUTAS DE API (PROXY AL ESP32) =====

# Nota: Los endpoints /api/estado y /api/modo pueden implementarse en el ESP32 si se necesitan
# Por ahora, usamos los endpoints básicos: temperature, time, logs, terminal

@app.route("/api/temperature")
@login_required
def temperature():
    """Obtener temperatura actual del ESP32"""
    try:
        r = requests.get(f"{ESP32_IP}/api/temperature", timeout=2)
        if r.status_code == 200:
            return r.text, 200, {'Content-Type': 'application/json'}
        else:
            return jsonify({"error": f"ESP32 respondió con código {r.status_code}"}), r.status_code
    except requests.exceptions.RequestException as e:
        return jsonify({"error": "No se pudo contactar al ESP32", "details": str(e)}), 500

@app.route("/api/time")
@login_required
def time():
    """Obtener hora actual del ESP32"""
    try:
        r = requests.get(f"{ESP32_IP}/api/time", timeout=2)
        if r.status_code == 200:
            return r.text, 200, {'Content-Type': 'application/json'}
        else:
            return jsonify({"error": f"ESP32 respondió con código {r.status_code}"}), r.status_code
    except requests.exceptions.RequestException as e:
        return jsonify({"error": "No se pudo contactar al ESP32", "details": str(e)}), 500

@app.route("/api/logs", methods=["GET"])
@login_required
def logs():
    """Obtener registros de horarios"""
    try:
        r = requests.get(f"{ESP32_IP}/api/logs", timeout=2)
        return r.text, r.status_code, {'Content-Type': 'application/json'}
    except requests.exceptions.RequestException as e:
        return jsonify({"error": "No se pudo contactar al ESP32", "details": str(e)}), 500

@app.route("/api/terminal", methods=["POST"])
@login_required
def terminal():
    """Ejecutar comando en el ESP32 (para terminal)"""
    try:
        data = request.json
        if not data or 'command' not in data:
            return jsonify({"error": "Campo 'command' requerido"}), 400
        
        r = requests.post(f"{ESP32_IP}/api/terminal", json=data, timeout=5)
        return r.text, r.status_code, {'Content-Type': 'application/json'}
    except requests.exceptions.RequestException as e:
        return jsonify({"error": "No se pudo contactar al ESP32", "details": str(e)}), 500

# Endpoints de compatibilidad (redirigen a los nuevos)
@app.route("/api/cmd")
@login_required
def cmd():
    """Ejecutar comando en el ESP32 (compatibilidad - redirige a /api/terminal)"""
    try:
        command = request.args.get('c', '')
        if not command:
            return jsonify({"error": "Parámetro 'c' requerido"}), 400
        
        r = requests.post(f"{ESP32_IP}/api/terminal", json={"command": command}, timeout=5)
        if r.status_code == 200:
            # Convertir respuesta JSON a texto plano para compatibilidad
            try:
                response_json = r.json()
                return response_json.get("response", ""), 200, {'Content-Type': 'text/plain'}
            except:
                return r.text, r.status_code, {'Content-Type': 'text/plain'}
        return r.text, r.status_code, {'Content-Type': 'text/plain'}
    except requests.exceptions.RequestException as e:
        return jsonify({"error": "No se pudo contactar al ESP32", "details": str(e)}), 500

@app.route("/api/registros", methods=["POST"])
@login_required
def registros_post():
    """Agregar registro de horario (compatibilidad)"""
    try:
        data = request.json
        r = requests.post(f"{ESP32_IP}/api/logs", json=data, timeout=2)
        return r.text, r.status_code
    except requests.exceptions.RequestException as e:
        return jsonify({"error": "No se pudo contactar al ESP32", "details": str(e)}), 500

# ===== INICIO DEL SERVIDOR =====
if __name__ == "__main__":
    print("=" * 60)
    print("Servidor Flask iniciando...")
    print(f"ESP32 IP: {ESP32_IP}")
    print("=" * 60)
    app.run(host="0.0.0.0", port=80, debug=True)
