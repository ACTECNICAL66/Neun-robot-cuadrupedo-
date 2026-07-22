# NEUN Robot Cuadrúpedo

Robot araña cuadrúpedo controlado por PC con interfaz web vía WiFi.

**IPET N°66** — Electrónica Digital III

## Estructura del repositorio

```
pc/           → Control Center (Flask + interfaz web para PC)
esp32/        → Firmware para ESP32-S3 (próximamente)
docs/         → Documentación y guías
```

## Componentes del sistema

| Capa | Tecnología | Función |
|------|-----------|---------|
| PC (Flask) | Python + Flask | Servidor web, API REST, simulador |
| Frontend | HTML + CSS + JS | Panel táctil de control |
| Conexión | WiFi (HTTP) | Comunicación PC ↔ ESP32 |
| Microcontrolador | ESP32-S3 | Control de servos y sensores |

## Modos de operación

- **MODO REAL**: Conectado al ESP32-S3 físico
- **MODO DEMO**: Simulación sin hardware (ideal para testing)

Ver `pc/README.md` para instrucciones de uso.
