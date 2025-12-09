from flask import Flask, render_template, request, jsonify
import requests

ESP32_IP = "http://192.168.4.1"   # La IP del ESP32 en tu red

app = Flask(__name__)

@app.route("/")
def home():
    return render_template("index.html")

@app.route("/dashboard")
def dashboard():
    return render_template("dashboard.html")

@app.route("/api/estado")
def estado():
    try:
        r = requests.get(f"{ESP32_IP}/estado")
        return r.json()
    except:
        return jsonify({"error": "No se pudo contactar al ESP32"}), 500

@app.route("/api/modo", methods=["POST"])
def modo():
    modo = request.json.get("modo")
    r = requests.post(f"{ESP32_IP}/modo", json={"modo": modo})
    return r.text, r.status_code

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=80)

