# Firmware ESP32-S3

Código para el microcontrolador ESP32-S3 del robot cuadrúpedo.

## Pendiente

- [ ] Conexión WiFi en modo Access Point (SSID: `ARANA-ROBOT`)
- [ ] Servidor HTTP embebido con API REST
- [ ] Control de servos PCA9685 (12 canales)
- [ ] Lectura de sensores (HC-SR04, MPU6050, TCRT5000, DHT22)
- [ ] Control de matriz NeoPixel 8x8
- [ ] Reenvío de comandos desde la PC

## Protocolo Buzzer (módulo pasivo vía PWM)

El Control Center PC envía estos comandos para generar sonido en el módulo buzzer:

### Reproducir tono

```
POST /api/command
{"cmd": "buzzer", "value": {"pattern": "BEEP", "freq": 880, "duration": 150, "wave": "square", "volume": 70}}
```

| Campo     | Tipo | Descripción |
|-----------|------|-------------|
| `pattern` | str  | Identificador del patrón (`BEEP`, `SIREN`, `ALARM`, `RISE`, `FALL`, `CUSTOM`) |
| `freq`    | int  | Frecuencia del tono en Hz (100–4000) |
| `duration`| int  | Duración del tono en milisegundos (50–2000) |
| `wave`    | str  | Forma de onda sugerida (`sine`, `square`, `triangle`, `sawtooth`). En un buzzer pasivo suele mapearse a PWM con frecuencia y duty fijos. |
| `volume`  | int  | Volumen 0–100 (duty cycle del PWM) |

### Detener sonido

```
POST /api/command
{"cmd": "buzzer_stop"}
```

### Sugerencia de implementación (ESP32-S3)

- Usar `ledc` (PWM) para generar la frecuencia: `ledcWriteTone(channel, freq)`.
- `wave` puede ignorarse (el buzzer pasivo solo produce onda cuadrada); usarlo solo para debugging.
- `volume` → duty cycle (`ledcWrite`), y `duration` → temporizador para apagar el tono.
- Notificar en `/api/status` → `sensors.buzzer.state` (`playing` / `idle`) y `sensors.buzzer.online`.
