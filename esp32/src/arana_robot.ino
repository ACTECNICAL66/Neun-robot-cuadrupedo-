/*
 * ============================================================================
 *  ARAÑA ROBOT — Firmware ESP32-S3 (central)
 *  IPET N°66 — Electrónica Digital III
 *
 *  Este archivo es el punto de entrada: setup() inicializa los módulos y
 *  loop() los actualiza. Aquí vive también el estado global del robot
 *  (espejo mínimo de SpiderState en pc/app.py).
 *
 *  Arquitectura de comunicación con la PC:
 *    [PC Flask] --WiFi--> [ESP32-S3 AP "ARANA-ROBOT" 192.168.4.1]
 *       localhost:5000            GET  /api/status   (sensores/estado)
 *                                 POST /api/command  ({"cmd":..., "value":...})
 *
 *  Módulo activo actualmente: Buzzer (protocolo completo del panel PC).
 *  El resto de los módulos (WiFi, WebServer, sensores, servos, NeoPixel)
 *  se incorporan a medida que se implementan (ver TODO más abajo).
 * ============================================================================
 */

#include <Arduino.h>
#include "config.h"
#include "actuadores/buzzer.h"    // módulo Buzzer (activo)

// ----------------------------------------------------------------------------
// Módulos pendientes — se habilita su include cuando exista implementación
// ----------------------------------------------------------------------------
// #include "comunicacion/wifi.h"       // TODO: AP ARANA-ROBOT + arranque server
// #include "comunicacion/webserver.h"  // TODO: GET /api/status + POST /api/command
// #include "sensores/ultrasonic.h"     // TODO: HC-SR04
// #include "sensores/imu.h"            // TODO: MPU6050
// #include "sensores/ir.h"             // TODO: TCRT5000
// #include "sensores/dht_sensor.h"     // TODO: DHT22
// #include "actuadores/servos.h"       // TODO: PCA9685 (12 servos)
// #include "actuadores/neopixel.h"     // TODO: Matriz 8x8

// ----------------------------------------------------------------------------
// Estado global del robot
// (los mismos campos que lee/actualiza la PC: mode, speed, emergency_stop)
// ----------------------------------------------------------------------------
struct RobotState {
  String  mode          = "STAND";   // STAND, WALK, TROT, GATEAR, GIRAR, ...
  uint8_t speed         = 50;        // 0..100
  bool    emergencyStop = false;     // Emergency Stop activo
} robotState;

// ----------------------------------------------------------------------------
// Consola de prueba por Monitor Serie
// (útil para probar el buzzer hasta que exista el webserver)
// ----------------------------------------------------------------------------
static void printBuzzerHelp() {
  Serial.println(F("  Comandos de prueba del Buzzer:"));
  Serial.println(F("    beep | siren | alarm | rise | fall | stop"));
  Serial.println(F("    tone <freq> <dur_ms> <vol>   (CUSTOM, ej: tone 880 500 70)"));
  Serial.println(F("    status"));
}

static void handleSerialConsole() {
  if (!Serial.available()) return;

  String line = Serial.readStringUntil('\n');
  line.trim();
  line.toLowerCase();
  if (line.length() == 0) return;

  // Presets = mismos parámetros que BUZZER_PRESETS del panel web (index.html)
  if      (line == "beep")  { buzzerPlay("BEEP",  880, 1000, "square",   70); }
  else if (line == "siren") { buzzerPlay("SIREN", 600, 3000, "sawtooth", 60); }
  else if (line == "alarm") { buzzerPlay("ALARM", 440, 3000, "square",   80); }
  else if (line == "rise")  { buzzerPlay("RISE",  300, 3000, "sine",     70); }
  else if (line == "fall")  { buzzerPlay("FALL", 1200, 3000, "triangle", 70); }
  else if (line == "stop")  { buzzerStop(); }
  else if (line == "status") {
    Serial.printf("[STATE] mode=%s speed=%u emergency=%s | buzzer=%s online=%d\n",
                  robotState.mode.c_str(), (unsigned)robotState.speed,
                  robotState.emergencyStop ? "ON" : "OFF",
                  buzzerGetState(), buzzerIsOnline() ? 1 : 0);
  }
  else if (line.startsWith("tone ")) {
    // Formato: tone <freq> <dur_ms> <vol>
    String rest = line.substring(5);
    const int p1 = rest.indexOf(' ');
    const int p2 = rest.indexOf(' ', p1 + 1);
    if (p1 > 0 && p2 > p1) {
      const uint16_t f = (uint16_t)rest.substring(0, p1).toInt();
      const uint16_t d = (uint16_t)rest.substring(p1 + 1, p2).toInt();
      const uint8_t  v = (uint8_t)rest.substring(p2 + 1).toInt();
      buzzerPlay("CUSTOM", f, d, "square", v);
    } else {
      Serial.println(F("[CONSOLE] Uso: tone <freq> <dur_ms> <vol>"));
    }
  }
  else if (line == "help" || line == "?") { printBuzzerHelp(); }
  else { Serial.println(F("[CONSOLE] Comando no reconocido. Escribí 'help'.")); }
}
// ----------------------------------------------------------------------------
// setup() — inicialización
// ----------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(300);

  Serial.println();
  Serial.println(F("======================================="));
  Serial.println(F("   ARAÑA ROBOT — Firmware ESP32-S3"));
  Serial.println(F("   IPET N°66 · Electrónica Digital III"));
  Serial.println(F("======================================="));

  // --- Buzzer ---
  buzzerInit();
  Serial.printf("[INIT] Buzzer -> GPIO%d %s | estado=%s\n", BUZZER_PIN,
                buzzerIsOnline() ? "ONLINE" : "ERROR", buzzerGetState());

  // Beep corto de arranque (autotest del módulo)
  buzzerPlay("BEEP", BUZZER_DEFAULT_FREQ_HZ, BUZZER_DEFAULT_DURATION_MS,
             "square", BUZZER_DEFAULT_VOLUME);

  // --- Módulos pendientes (por orden de integración) ---
  // wifiInit();            // TODO: crear AP "ARANA-ROBOT" y arrancar el server
  // webserverInit();       // TODO: GET /api/status + POST /api/command
  //                        //       "buzzer"      -> buzzerPlay(...)
  //                        //       "buzzer_stop" -> buzzerStop()
  // servosInit();
  // imuInit(); dhtInit(); irInit(); ultrasonicInit(); neopixelInit();

  printBuzzerHelp();
}

// ----------------------------------------------------------------------------
// loop() — actualización continua
// ----------------------------------------------------------------------------
void loop() {
  buzzerUpdate();            // máquina de estados del sonido (no bloqueante)
  handleSerialConsole();     // consola de prueba por Monitor Serie

  // TODO (cuando existan los módulos):
  //   ultrasonicUpdate(); imuUpdate(); dhtUpdate(); irUpdate();
  //   webserverHandle();   // ESPAsyncWebServer se atiende solo, sin loop()
}