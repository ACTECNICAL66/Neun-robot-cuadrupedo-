// ============================================================================
// Módulo Buzzer — Araña Robot (ESP32-S3)
// Buzzer pasivo controlado por PWM (LEDC). Implementa el protocolo que envía
// el Control Center PC (ver esp32/README.md, sección "Protocolo Buzzer"):
//
//   {"cmd":"buzzer","value":{"pattern":"BEEP","freq":880,"duration":150,
//                            "wave":"square","volume":70}}
//   {"cmd":"buzzer_stop"}
//
// API pública en buzzer.h. Reproducción NO bloqueante: buzzerUpdate() debe
// llamarse desde loop() y avanza la máquina de estados del patrón.
//
// Compatible con Arduino core 2.x (API por canal) y 3.x (API por pin).
// ============================================================================

#include "buzzer.h"
#include "config.h"

#include <esp_arduino_version.h>
#include <strings.h>   // strcasecmp

#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
  #define BUZZER_LEDC_CORE3 1
#else
  #define BUZZER_LEDC_CORE3 0
#endif

// ----------------------------------------------------------------------------
// Estado interno
// ----------------------------------------------------------------------------
static bool          s_online      = false;
static bool          s_playing     = false;
static BuzzerPattern s_pattern     = BUZZER_PATTERN_UNKNOWN;
static uint16_t      s_freqHz      = 0;     // frecuencia base del comando
static uint32_t      s_durationMs  = 0;     // duración total del patrón
static uint8_t       s_duty        = 0;     // duty 0..127 (50% = onda cuadrada)
static uint32_t      s_startMs     = 0;
static uint32_t      s_nextPhaseMs = 0;     // próximo cambio de fase del patrón
static bool          s_phaseOn     = true;  // ALARM: ¿tono activo o silencio?
static bool          s_phaseHigh   = false; // SIREN: ¿frecuencia alta o baja?

// Tiempos de los patrones (ms)
static const uint16_t SIREN_HALF_PERIOD_MS = 400;
static const uint16_t ALARM_ON_MS          = 150;
static const uint16_t ALARM_OFF_MS         = 100;
static const uint16_t SWEEP_STEP_MS        = 30;   // resolución del barrido

// ----------------------------------------------------------------------------
// Helpers PWM (LEDC) con soporte Arduino core 2.x / 3.x
// ----------------------------------------------------------------------------

static void pwmWrite(uint8_t duty) {
#if BUZZER_LEDC_CORE3
  ledcWrite(BUZZER_PIN, duty);
#else
  ledcWrite(BUZZER_LEDC_CHANNEL, duty);
#endif
}

static void pwmSetFreq(uint16_t freqHz) {
  // Mantener la frecuencia dentro de los límites que soporta el panel PC
  if (freqHz < BUZZER_FREQ_MIN_HZ) freqHz = BUZZER_FREQ_MIN_HZ;
  if (freqHz > BUZZER_FREQ_MAX_HZ) freqHz = BUZZER_FREQ_MAX_HZ;
#if BUZZER_LEDC_CORE3
  ledcChangeFrequency(BUZZER_PIN, freqHz, BUZZER_LEDC_RESOLUTION);
  ledcWrite(BUZZER_PIN, s_duty);            // re-aplicar duty tras la frecuencia
#else
  ledcChangeFrequency(BUZZER_LEDC_CHANNEL, freqHz, BUZZER_LEDC_RESOLUTION);
  ledcWrite(BUZZER_LEDC_CHANNEL, s_duty);
#endif
}

static void pwmStart(uint16_t freqHz) {
#if BUZZER_LEDC_CORE3
  ledcAttach(BUZZER_PIN, freqHz, BUZZER_LEDC_RESOLUTION);
  ledcWrite(BUZZER_PIN, s_duty);
#else
  ledcSetup(BUZZER_LEDC_CHANNEL, freqHz, BUZZER_LEDC_RESOLUTION);
  ledcAttachPin(BUZZER_PIN, BUZZER_LEDC_CHANNEL);
  ledcWrite(BUZZER_LEDC_CHANNEL, s_duty);
#endif
}

// Volumen 0..100 -> duty 0..127 (el 50% del duty es la potencia máxima útil
// de una onda cuadrada en un buzzer pasivo).
static uint8_t volumeToDuty(uint8_t volume) {
  if (volume > 100) volume = 100;
  const uint16_t DUTY_MAX = ((1u << BUZZER_LEDC_RESOLUTION) / 2u) - 1u; // 127
  return (uint8_t)(((uint16_t)volume * DUTY_MAX) / 100u);
}

// ----------------------------------------------------------------------------
// API pública
// ----------------------------------------------------------------------------

void buzzerInit() {
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  s_playing = false;
  s_online  = false;
  s_duty    = 0;
  pwmStart(BUZZER_DEFAULT_FREQ_HZ);   // deja el canal listo, en silencio
  s_online = true;
}

BuzzerPattern buzzerPatternFromString(const char* pattern) {
  if (pattern == nullptr) return BUZZER_PATTERN_UNKNOWN;
  if (strcasecmp(pattern, "BEEP")   == 0) return BUZZER_PATTERN_BEEP;
  if (strcasecmp(pattern, "SIREN")  == 0) return BUZZER_PATTERN_SIREN;
  if (strcasecmp(pattern, "ALARM")  == 0) return BUZZER_PATTERN_ALARM;
  if (strcasecmp(pattern, "RISE")   == 0) return BUZZER_PATTERN_RISE;
  if (strcasecmp(pattern, "FALL")   == 0) return BUZZER_PATTERN_FALL;
  if (strcasecmp(pattern, "CUSTOM") == 0) return BUZZER_PATTERN_CUSTOM;
  return BUZZER_PATTERN_UNKNOWN;
}
bool buzzerPlay(const char* pattern, uint16_t freqHz, uint16_t durationMs,
                const char* wave, uint8_t volume) {
  const BuzzerPattern p = buzzerPatternFromString(pattern);
  if (p == BUZZER_PATTERN_UNKNOWN) {
    Serial.printf("[BUZZER] error: patrón desconocido '%s'\n",
                  pattern ? pattern : "(null)");
    return false;
  }
  if (!s_online) return false;

  // Aplicar los límites que define el protocolo del panel PC
  if (freqHz     < BUZZER_FREQ_MIN_HZ)     freqHz     = BUZZER_FREQ_MIN_HZ;
  if (freqHz     > BUZZER_FREQ_MAX_HZ)     freqHz     = BUZZER_FREQ_MAX_HZ;
  if (durationMs < BUZZER_DURATION_MIN_MS) durationMs = BUZZER_DURATION_MIN_MS;
  if (durationMs > BUZZER_DURATION_MAX_MS) durationMs = BUZZER_DURATION_MAX_MS;

  s_pattern    = p;
  s_freqHz     = freqHz;
  s_durationMs = durationMs;
  s_duty       = volumeToDuty(volume);
  s_startMs    = millis();
  s_phaseOn    = true;
  s_phaseHigh  = false;
  s_playing    = true;

  pwmSetFreq(freqHz);   // arranca sonando a la frecuencia base
  pwmWrite(s_duty);

  // Agendar la primera transición de fase según el patrón
  switch (p) {
    case BUZZER_PATTERN_SIREN:
      s_nextPhaseMs = s_startMs + SIREN_HALF_PERIOD_MS;
      break;
    case BUZZER_PATTERN_ALARM:
      s_nextPhaseMs = s_startMs + ALARM_ON_MS;
      break;
    case BUZZER_PATTERN_RISE:
    case BUZZER_PATTERN_FALL:
      s_nextPhaseMs = s_startMs + SWEEP_STEP_MS;
      break;
    default:
      s_nextPhaseMs = 0;
      break;
  }

  Serial.printf("[BUZZER] play pattern=%s freq=%uHz dur=%ums wave=%s vol=%u%%\n",
                pattern, (unsigned)freqHz, (unsigned)durationMs,
                wave ? wave : "-", (unsigned)volume);
  return true;
}

void buzzerUpdate() {
  if (!s_playing) return;

  const uint32_t now     = millis();
  const uint32_t elapsed = now - s_startMs;   // seguro ante overflow de millis()

  if (elapsed >= s_durationMs) {              // el patrón terminó
    buzzerStop();
    return;
  }

  switch (s_pattern) {
    case BUZZER_PATTERN_BEEP:
    case BUZZER_PATTERN_CUSTOM:
      break;                                  // tono fijo hasta finalizar

    case BUZZER_PATTERN_SIREN:                // alterna frec. base y x1.5
      if ((int32_t)(now - s_nextPhaseMs) >= 0) {
        s_phaseHigh = !s_phaseHigh;
        pwmSetFreq(s_phaseHigh ? (uint16_t)(s_freqHz * 3u / 2u) : s_freqHz);
        s_nextPhaseMs = now + SIREN_HALF_PERIOD_MS;
      }
      break;

    case BUZZER_PATTERN_ALARM:                // beep 150ms / pausa 100ms
      if ((int32_t)(now - s_nextPhaseMs) >= 0) {
        s_phaseOn = !s_phaseOn;
        pwmWrite(s_phaseOn ? s_duty : 0);
        s_nextPhaseMs = now + (s_phaseOn ? ALARM_ON_MS : ALARM_OFF_MS);
      }
      break;

    case BUZZER_PATTERN_RISE:                 // barrido lineal x1 -> x4
    case BUZZER_PATTERN_FALL:                 // barrido lineal x1 -> x1/4
      if ((int32_t)(now - s_nextPhaseMs) >= 0) {
        const float k    = (float)elapsed / (float)s_durationMs;   // 0.0..1.0
        const float mult = (s_pattern == BUZZER_PATTERN_RISE)
                               ? (1.0f + 3.0f * k)
                               : (1.0f - 0.75f * k);
        pwmSetFreq((uint16_t)(s_freqHz * mult));
        s_nextPhaseMs = now + SWEEP_STEP_MS;
      }
      break;

    default:
      break;
  }
}

void buzzerStop() {
  if (!s_playing) return;
  pwmWrite(0);
  s_playing = false;
  Serial.println("[BUZZER] stop -> idle");
}

bool buzzerIsPlaying() { return s_playing; }

const char* buzzerGetState() { return s_playing ? "playing" : "idle"; }

bool buzzerIsOnline() { return s_online; }