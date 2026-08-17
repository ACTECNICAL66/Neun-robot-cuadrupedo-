#ifndef ACTUADORES_BUZZER_H
#define ACTUADORES_BUZZER_H

#include <Arduino.h>

// ============================================================================
// Módulo Buzzer (pasivo, PWM LEDC) — Araña Robot
//
// Implementa el protocolo documentado en esp32/README.md, que es el que envía
// el Control Center PC (pc/app.py -> dispatch "buzzer" / "buzzer_stop"):
//
//   POST /api/command
//   {"cmd":"buzzer","value":{"pattern":"BEEP","freq":880,"duration":150,
//                            "wave":"square","volume":70}}
//   {"cmd":"buzzer_stop"}
//
// El estado se publica en /api/status -> sensors.buzzer.state ("playing"|"idle")
// y sensors.buzzer.online (bool), tal como espera pc/app.py (_merge_sensors).
//
// Patrones = identificadores de BUZZER_PRESETS del panel web (index.html):
//   BEEP / SIREN / ALARM / RISE / FALL / CUSTOM.
// `wave` (sine/square/triangle/sawtooth) no cambia el sonido real del buzzer
// pasivo (solo produce onda cuadrada); se acepta por compatibilidad de protocolo.
//
// La reproducción es NO bloqueante: se debe llamar a buzzerUpdate() desde loop().
// ============================================================================

// Identificadores de patrón (coinciden con el JSON del panel PC)
enum BuzzerPattern : uint8_t {
  BUZZER_PATTERN_BEEP   = 0,
  BUZZER_PATTERN_SIREN,
  BUZZER_PATTERN_ALARM,
  BUZZER_PATTERN_RISE,
  BUZZER_PATTERN_FALL,
  BUZZER_PATTERN_CUSTOM,
  BUZZER_PATTERN_UNKNOWN,
};

// Inicializa el PWM LEDC y deja el buzzer en "idle".
void buzzerInit();

// Máquina de estados del sonido. Llamar en cada iteración de loop().
void buzzerUpdate();

// Reproduce un tono. Parámetros = campos exactos del JSON que envía la PC.
// Devuelve true si el comando fue aceptado, false si el patrón es inválido.
bool buzzerPlay(const char* pattern, uint16_t freqHz, uint16_t durationMs,
                const char* wave, uint8_t volume);

// Detiene el sonido inmediatamente.
void buzzerStop();

bool        buzzerIsPlaying();
const char* buzzerGetState();   // "playing" | "idle"   -> sensors.buzzer.state
bool        buzzerIsOnline();   //                      -> sensors.buzzer.online

// Convierte el string del protocolo a BuzzerPattern.
BuzzerPattern buzzerPatternFromString(const char* pattern);

#endif  // ACTUADORES_BUZZER_H