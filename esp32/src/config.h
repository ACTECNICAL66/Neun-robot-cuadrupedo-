#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ============================================================================
// ARAÑA ROBOT — Configuración global
// Pines, WiFi y constantes. El pinout se alinea con el estado de sensores que
// define la PC (pc/app.py -> SensorState).
// ============================================================================

// ------------------------------ WiFi (Access Point) --------------------------
#define WIFI_SSID         "ARANA-ROBOT"   // red a la que se conecta la PC
#define WIFI_PASSWORD     ""              // AP abierto (sin contraseña)
#define WIFI_CHANNEL      1
#define HTTP_PORT         80              // la PC espera el API en 192.168.4.1:80

// --------------------------------- Pinout ------------------------------------
// Sensores
#define ULTRASONIC_TRIG_PIN  10           // HC-SR04  -> Trigger
#define ULTRASONIC_ECHO_PIN  11           // HC-SR04  -> Echo
#define IR_PIN               12           // TCRT5000 (analógico/digital)
#define DHT_PIN              13           // DHT22    -> data
// Actuadores
#define NEOPIXEL_PIN         14           // Matriz NeoPixel 8x8 (64 LEDs)
#define BUZZER_PIN           15           // Buzzer pasivo (PWM LEDC)
// Bus I2C compartido: MPU6050 (0x68) + PCA9685 (0x40)
#define I2C_SDA_PIN           8           // por defecto en ESP32-S3-DevKitC-1
#define I2C_SCL_PIN           9

// ------------------------------- Constantes ----------------------------------
#define NEOPIXEL_COUNT       64           // matriz 8x8
#define SERVO_COUNT          12           // PCA9685: 4 patas x 3 articulaciones
#define PCA9685_I2C_ADDR     0x40
#define MPU6050_I2C_ADDR     0x68
#define DHT_TYPE             DHT22        // usado por dht_sensor.cpp

// ------------------------------ Buzzer (LEDC) --------------------------------
// Límites del protocolo del panel PC (ver esp32/README.md).
#define BUZZER_LEDC_CHANNEL      0        // canal PWM (solo Arduino core 2.x)
#define BUZZER_LEDC_RESOLUTION   8        // bits -> duty 0..255
#define BUZZER_FREQ_MIN_HZ       100
#define BUZZER_FREQ_MAX_HZ       4000
#define BUZZER_DURATION_MIN_MS   50
// README documenta 50-2000, pero los presets del panel web envían duración 3000;
// se deja margen para no cortar el sonido antes de tiempo.
#define BUZZER_DURATION_MAX_MS   5000
#define BUZZER_DEFAULT_FREQ_HZ   880
#define BUZZER_DEFAULT_DURATION_MS 150
#define BUZZER_DEFAULT_VOLUME    70

#endif  // CONFIG_H