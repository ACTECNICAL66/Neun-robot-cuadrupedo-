# Control Center — PC

Interfaz de control táctil para el robot araña. Corre en PC y se comunica con el ESP32-S3 por WiFi.

## Requisitos

- Python 3.10+
- Conexión WiFi a la red `ARANA-ROBOT` (creada por el ESP32)

## Instalación

```bash
pip install -r requirements.txt
```

## Uso

1. Conectá la PC a la red WiFi `ARANA-ROBOT`
2. Ejecutá el servidor:
   ```bash
   python app.py
   ```
3. Abrí el navegador en `http://localhost:5000`
4. Hacé clic en **"Conectar"** en el diálogo emergente

## API REST

| Ruta | Método | Descripción |
|------|--------|-------------|
| `/` | GET | Interfaz web |
| `/api/status` | GET | Estado completo del robot |
| `/api/command` | POST | Enviar comando al robot |
| `/api/chat` | POST | Chat copiloto |
| `/api/connect_esp32` | GET | Conectar al ESP32 |
| `/api/disconnect` | GET | Desconectar del ESP32 |

## Modo Demo

Si el ESP32 no está disponible, el sistema entra automáticamente en **Modo Demo** con sensores simulados.

## Buzzer / Sonido

El panel **Sonido / Buzzer** permite reproducir tonos en el módulo buzzer del ESP32:

- **Patrones predefinidos**: Beep, Sirena, Alarma, Ascendente, Descendente.
- **Regulación manual**: forma de onda, frecuencia (100–4000 Hz), duración y volumen.
- En la PC se reproduce una vista previa del tono (Web Audio) y el comando se reenvía al ESP32 vía `{"cmd":"buzzer", "value":{...}}`. Ver `esp32/README.md` para el protocolo completo.
