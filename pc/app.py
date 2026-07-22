#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
ARAÑA ROBOT - Control Center v2.6 Desktop + ESP32-S3 Bridge
IPET N°66 - Electrónica Digital III

Arquitectura:
  [PC con Flask] <--WiFi--> [ESP32-S3 AP]
       |                         |
  localhost:5000           192.168.4.1:80
       |                         |
  Panel táctico HTML+JS    /api/status (JSON)
                           /api/command (POST)

Flujo:
  1. ESP32 crea AP "ARANA-ROBOT" (192.168.4.1)
  2. PC se conecta a esa red WiFi
  3. Se ejecuta python app.py
  4. Backend intenta conectar con ESP32 en 192.168.4.1
  5. Si responde -> MODO REAL | Si no -> MODO DEMO
"""

from __future__ import annotations

import json
import math
import random
import time
import threading
from dataclasses import dataclass, field
from typing import Any, Optional

import requests
from flask import Flask, jsonify, render_template, request
from flask_cors import CORS

# =============================================================================
# CONFIGURACIÓN
# =============================================================================

@dataclass
class Config:
    esp32_ip: str = "192.168.4.1"
    esp32_port: int = 80
    connection_timeout: float = 1.0
    poll_interval_ms: int = 500
    sensor_update_interval: float = 0.4
    connection_poll_interval: float = 2.0
    max_backoff: float = 30.0
    demo_activation_delay: float = 1.5
    max_logs: int = 100
    max_chat_history: int = 50
    max_faults: int = 50

    @property
    def esp32_base_url(self) -> str:
        return f"http://{self.esp32_ip}:{self.esp32_port}"


config = Config()

# =============================================================================
# ESTADO GLOBAL
# =============================================================================

@dataclass
class SensorState:
    ultrasonic: dict = field(default_factory=lambda: {
        "online": True, "value": 42.5, "unit": "cm", "name": "Radar HC-SR04", "pin": "GPIO10/11"
    })
    imu: dict = field(default_factory=lambda: {
        "online": True, "pitch": -15.2, "roll": 25.4, "yaw": 0.0,
        "ax": 0, "ay": 0, "az": 16500, "gx": 0, "gy": 0, "gz": 0,
        "temp": 36.5, "name": "MPU6050", "pin": "I2C"
    })
    ir: dict = field(default_factory=lambda: {
        "online": True, "value": 847, "name": "TCRT5000", "pin": "GPIO12"
    })
    dht: dict = field(default_factory=lambda: {
        "online": True, "temp": 24.5, "hum": 62.0, "name": "DHT22", "pin": "GPIO13"
    })
    leds: dict = field(default_factory=lambda: {
        "online": True, "brightness": 50, "name": "NeoPixel 8x8", "pin": "GPIO14"
    })
    servos: dict = field(default_factory=lambda: {
        "online": True, "count": 12, "name": "PCA9685", "pin": "I2C"
    })

    def to_dict(self) -> dict:
        return {
            "ultrasonic": self.ultrasonic,
            "imu": self.imu,
            "ir": self.ir,
            "dht": self.dht,
            "leds": self.leds,
            "servos": self.servos,
        }


@dataclass
class LogEntry:
    time: str
    msg: str
    type: str


@dataclass
class ChatEntry:
    role: str
    text: str
    time: str


@dataclass
class FaultEntry:
    time: str
    sensor: str
    msg: str


class SpiderState:
    def __init__(self):
        self.connected = False
        self.mode = "STAND"
        self.speed = 50
        self.emergency_stop = False
        self.stabilize = False
        self.esp32_ip = config.esp32_ip
        self.last_esp32_ping: float = 0
        self.battery = 87.0
        self.sensors = SensorState()
        self.servos: list[list[int]] = [[90, 90, 90] for _ in range(4)]
        self.led_color: dict[str, int] = {"r": 0, "g": 255, "b": 136}
        self.led_matrix: list[list[int]] = [[0] * 8 for _ in range(8)]
        self._logs: list[LogEntry] = []
        self._chat_history: list[ChatEntry] = []
        self._faults: list[FaultEntry] = []

    # -- Logs -----------------------------------------------------------------

    def log(self, msg: str, type: str = "info") -> None:
        self._logs.insert(0, LogEntry(time=time.strftime("%H:%M:%S"), msg=msg, type=type))
        if len(self._logs) > config.max_logs:
            self._logs.pop()

    @property
    def logs(self) -> list[dict]:
        return [{"time": e.time, "msg": e.msg, "type": e.type} for e in self._logs]

    # -- Chat -----------------------------------------------------------------

    def add_chat(self, role: str, text: str) -> None:
        self._chat_history.insert(0, ChatEntry(role=role, text=text, time=time.strftime("%H:%M:%S")))
        if len(self._chat_history) > config.max_chat_history:
            self._chat_history.pop()

    @property
    def chat_history(self) -> list[dict]:
        return [{"role": e.role, "text": e.text, "time": e.time} for e in self._chat_history]

    # -- Faults ---------------------------------------------------------------

    def add_fault(self, sensor: str, msg: str) -> None:
        self._faults.insert(0, FaultEntry(time=time.strftime("%H:%M:%S"), sensor=sensor, msg=msg))
        if len(self._faults) > config.max_faults:
            self._faults.pop()

    @property
    def faults(self) -> list[dict]:
        return [{"time": f.time, "sensor": f.sensor, "msg": f.msg} for f in self._faults[:5]]

    # -- Status snapshot ------------------------------------------------------

    def status_snapshot(self) -> dict:
        return {
            "connected": self.connected,
            "mode": self.mode,
            "speed": self.speed,
            "battery": self.battery,
            "servos": self.servos,
            "sensors": self.sensors.to_dict(),
            "faults": self.faults,
        }


state = SpiderState()

# =============================================================================
# SIMULADOR DE SENSORES
# =============================================================================

class SensorSimulator:
    def __init__(self, state: SpiderState):
        self._state = state

    def update(self) -> None:
        if self._state.connected:
            return

        s = self._state.sensors
        rng = random

        s.ultrasonic["value"] = max(5, min(400, s.ultrasonic["value"] + rng.uniform(-8, 8)))
        s.ir["value"] = max(0, min(1023, s.ir["value"] + rng.randint(-60, 60)))
        s.dht["temp"] = max(15, min(40, s.dht["temp"] + rng.uniform(-0.3, 0.3)))
        s.dht["hum"] = max(20, min(90, s.dht["hum"] + rng.uniform(-1.5, 1.5)))
        s.imu["pitch"] = max(-45, min(45, s.imu["pitch"] + rng.uniform(-3, 3)))
        s.imu["roll"] = max(-45, min(45, s.imu["roll"] + rng.uniform(-3, 3)))
        s.imu["yaw"] = (s.imu["yaw"] + rng.uniform(-2, 2)) % 360
        s.imu["gx"] = rng.uniform(-200, 200)
        s.imu["gy"] = rng.uniform(-200, 200)
        s.imu["gz"] = rng.uniform(-200, 200)
        s.imu["temp"] = max(30, min(50, s.imu["temp"] + rng.uniform(-0.05, 0.05)))
        self._state.battery = max(0, min(100, self._state.battery - 0.02))

        if rng.random() < 0.005:
            sensor = rng.choice(["ultrasonic", "ir", "dht"])
            getattr(s, sensor)["online"] = False
            name = getattr(s, sensor)["name"]
            self._state.add_fault(sensor, f"{name} desconectado")
            self._state.log(f"FALLA: {name} desconectado", "error")

        if rng.random() < 0.008:
            for key in ("ultrasonic", "ir", "dht", "imu", "leds", "servos"):
                if not getattr(s, key)["online"]:
                    getattr(s, key)["online"] = True
                    self._state.log(f"{getattr(s, key)['name']} reconectado", "success")
                    break


simulator = SensorSimulator(state)

# =============================================================================
# CLIENTE DE COMUNICACIÓN CON ESP32
# =============================================================================

class ESP32Client:
    def __init__(self, state: SpiderState):
        self._state = state

    def ping(self) -> bool:
        try:
            resp = requests.get(f"{config.esp32_base_url}/api/status", timeout=0.8)
            return resp.status_code == 200
        except requests.RequestException:
            return False

    def fetch_status(self) -> bool:
        try:
            resp = requests.get(f"{config.esp32_base_url}/api/status", timeout=config.connection_timeout)
            if resp.status_code != 200:
                return False
            data = resp.json()
            if "sensors" in data:
                self._merge_sensors(data["sensors"])
            if "battery" in data:
                self._state.battery = data["battery"]
            if "servos" in data:
                self._state.servos = data["servos"]
            if "mode" in data:
                self._state.mode = data["mode"]
            if "speed" in data:
                self._state.speed = data["speed"]
            self._state.connected = True
            self._state.last_esp32_ping = time.time()
            return True
        except (requests.RequestException, ValueError):
            return False

    def send_command(self, cmd: str, value: Any = None) -> bool:
        if not self._state.connected:
            return False
        try:
            payload: dict[str, Any] = {"cmd": cmd}
            if value is not None:
                payload["value"] = value
            resp = requests.post(
                f"{config.esp32_base_url}/api/command",
                json=payload,
                timeout=config.connection_timeout,
                headers={"Content-Type": "application/json"},
            )
            return resp.status_code == 200
        except requests.RequestException:
            return False

    def _merge_sensors(self, esp_sensors: dict) -> None:
        s = self._state.sensors
        for key in ("ultrasonic", "imu", "ir", "dht", "leds", "servos"):
            if key in esp_sensors:
                for k, v in esp_sensors[key].items():
                    getattr(s, key)[k] = v


esp32 = ESP32Client(state)

# =============================================================================
# HILOS EN SEGUNDO PLANO
# =============================================================================

def _sensor_loop() -> None:
    while True:
        simulator.update()
        time.sleep(config.sensor_update_interval)


def _connection_monitor() -> None:
    attempts = 0
    while True:
        if esp32.ping():
            if not state.connected:
                state.log(f"ESP32-S3 detectado en {config.esp32_ip}", "success")
                state.log("Conectando...", "info")
            esp32.fetch_status()
            attempts = 0
            time.sleep(config.connection_poll_interval)
        else:
            if state.connected:
                state.log(f"ESP32-S3 desconectado ({config.esp32_ip})", "error")
                state.connected = False
            attempts += 1
            sleep_time = min(config.connection_poll_interval * (2 ** (attempts // 10)), config.max_backoff)
            state.last_esp32_ping = 0
            time.sleep(sleep_time)


_CONN_THREAD: Optional[threading.Thread] = None


def _start_connection_monitor() -> None:
    global _CONN_THREAD
    if _CONN_THREAD is None or not _CONN_THREAD.is_alive():
        _CONN_THREAD = threading.Thread(target=_connection_monitor, daemon=True)
        _CONN_THREAD.start()


# Iniciar loop de sensores siempre (necesario para modo demo)
threading.Thread(target=_sensor_loop, daemon=True).start()

# =============================================================================
# MANEJADOR DE COMANDOS (DISPATCH)
# =============================================================================

class CommandDispatcher:
    def __init__(self, state: SpiderState, esp32: ESP32Client):
        self._state = state
        self._esp32 = esp32
        self._handlers: dict[str, callable] = {
            "mode": self._handle_mode,
            "move": self._handle_move,
            "aux": self._handle_aux,
            "speed": self._handle_speed,
            "emergency": self._handle_emergency,
            "stabilize": self._handle_stabilize,
            "servo": self._handle_servo,
            "servo_adv": self._handle_servo_adv,
            "terminal": self._handle_terminal,
        }

    def dispatch(self, cmd: str, value: Any = None) -> dict[str, Any]:
        forwarded = False
        if self._state.connected and cmd not in ("terminal",):
            forwarded = self._esp32.send_command(cmd, value)
            if forwarded:
                self._state.log(f"[ESP32] Cmd enviado: {cmd}", "success")
            else:
                self._state.log(f"[ESP32] Fallo al enviar: {cmd}", "error")

        handler = self._handlers.get(cmd)
        if handler:
            handler(value)

        return {"status": "ok", "esp32_forwarded": forwarded}

    def _handle_mode(self, value: Any) -> None:
        if value:
            self._state.mode = value
            self._state.log(f"Modo: {value}", "success")

    def _handle_move(self, value: Any) -> None:
        self._state.log(f"Move: {value}", "info")

    def _handle_aux(self, value: Any) -> None:
        self._state.log(f"Aux: {value}", "info")

    def _handle_speed(self, value: Any) -> None:
        if value is not None:
            self._state.speed = int(value)

    def _handle_emergency(self, value: Any) -> None:
        self._state.emergency_stop = bool(value)
        self._state.log(f"Emergency: {'ON' if value else 'OFF'}", "error" if value else "success")

    def _handle_stabilize(self, value: Any) -> None:
        self._state.stabilize = bool(value)

    def _handle_servo(self, value: Any) -> None:
        if isinstance(value, dict):
            leg = value.get("leg")
            joint = value.get("joint")
            angle = value.get("angle")
            if leg is not None and joint is not None and angle is not None:
                self._state.servos[leg][joint] = angle

    def _handle_servo_adv(self, value: Any) -> None:
        if isinstance(value, dict):
            ch = value.get("channel")
            angle = value.get("angle")
            if ch is not None and angle is not None:
                self._state.servos[ch // 3][ch % 3] = angle

    def _handle_terminal(self, value: Any) -> None:
        self._state.log(f"Terminal: {value}", "info")


dispatcher = CommandDispatcher(state, esp32)

# =============================================================================
# CHAT (COPILOTO IA)
# =============================================================================

_CHAT_RESPONSES: dict[str, str] = {
    "hola": "¡Hola! Soy el asistente de la Araña Robot. ¿En qué puedo ayudarte?",
    "modo": "Los modos disponibles son: Stand, Caminar, Trotar, Gatear, Girar, Cangrejo, Balanceo, Baile, Centinela y Explorar.",
    "sensor": "Todos los sensores están online: Radar, IMU, IR, DHT22, NeoPixel y PCA9685.",
    "bateria": None,  # Se resuelve dinámicamente
    "calibrar": "Usa el comando 'calibrate' en la terminal.",
    "parar": "Deteniendo movimiento...",
    "stop": "Deteniendo movimiento...",
    "emergency": "Activando Emergency Stop...",
    "velocidad": None,  # Se resuelve dinámicamente
}


def process_chat_message(msg: str) -> str:
    state.add_chat("user", msg)
    t = msg.lower()

    for keyword, response in _CHAT_RESPONSES.items():
        if keyword in t:
            if keyword == "bateria":
                resp = f"La batería está en {state.battery:.0f}%."
            elif keyword == "velocidad":
                resp = f"La velocidad actual es {state.speed}%."
            elif keyword in ("parar", "stop"):
                resp = response
                state.log("Stop por chat", "warn")
            elif keyword == "emergency":
                resp = response
                state.emergency_stop = True
            else:
                resp = response
            state.add_chat("ai", resp)
            return resp

    resp = (
        "Puedes preguntarme sobre modos, sensores, batería, calibración o control. "
        "También puedes usar la terminal para comandos directos."
    )
    state.add_chat("ai", resp)
    return resp


# =============================================================================
# APLICACIÓN FLASK
# =============================================================================

app = Flask(__name__)
CORS(app)


@app.route("/")
def index():
    return render_template("index.html")


@app.route("/api/esp32_status")
def api_esp32_status():
    return jsonify({
        "connected": state.connected,
        "esp32_ip": state.esp32_ip,
        "last_ping": state.last_esp32_ping,
    })


@app.route("/api/connect_esp32")
def api_connect_esp32():
    _start_connection_monitor()
    if esp32.ping() and esp32.fetch_status():
        state.log("Conexión manual establecida con ESP32", "success")
        return jsonify({"success": True, "message": "Conectado al ESP32"})
    return jsonify({"success": False, "message": "ESP32 no responde"})


@app.route("/api/disconnect")
def api_disconnect():
    state.connected = False
    state.log("Desconexión manual de ESP32", "warn")
    return jsonify({"success": True, "message": "Desconectado"})


@app.route("/api/status")
def api_status():
    return jsonify(state.status_snapshot())


@app.route("/api/command", methods=["POST"])
def api_command():
    data = request.get_json() or {}
    cmd = data.get("cmd")
    value = data.get("value")
    result = dispatcher.dispatch(cmd, value)
    return jsonify(result)


@app.route("/api/chat", methods=["POST"])
def api_chat():
    data = request.get_json() or {}
    msg = data.get("message", "")
    resp = process_chat_message(msg)
    return jsonify({"response": resp})


# =============================================================================
# MAIN
# =============================================================================

if __name__ == "__main__":
    print("=" * 65)
    print("  ARAÑA ROBOT v2.6 - Control Center Desktop + ESP32 Bridge")
    print("=" * 65)
    print(f"  IP del ESP32 configurada: {config.esp32_ip}")
    print("  Para conectar:")
    print("    1. Conecta la PC a la red WiFi 'ARANA-ROBOT'")
    print("    2. Abre el navegador en: http://localhost:5000")
    print("    3. Haz clic en 'Conectar' en el diálogo emergente")
    print("=" * 65)
    app.run(host="0.0.0.0", port=5000, debug=False, threaded=True)
