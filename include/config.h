#ifndef MEDIGUARD_CONFIG_H
#define MEDIGUARD_CONFIG_H

#include <Arduino.h>

// ============================================================================
// MediGuard ESP32 Hardware Firmware Configuration
// Target Board: ESP32 DevKit 38-Pin USB-C (ESP-WROOM-32 / esp32dev)
// ============================================================================

// ----------------------------------------------------------------------------
// 1. Network & Cloud Credentials
// ----------------------------------------------------------------------------
#define WIFI_SSID               "medigurd"
#define WIFI_PASSWORD           "123123123"

// Supabase Telemetry Ingestion Endpoint
#define SUPABASE_INGEST_URL     "https://bskjisnvumbsiwcahvyw.supabase.co/functions/v1/ingest-telemetry"

// Dedicated Hardware Device API Key (Registered and authenticated in public.devices)
#define DEVICE_API_KEY          "mediguard-container-01"

// Telemetry uplink interval in seconds (default 10s for live telemetry, 30s in production)
#define TELEMETRY_INTERVAL_SEC  10

// Network & HTTP Timeouts
#define HTTP_REQUEST_TIMEOUT_MS 6000
#define WIFI_CONNECT_TIMEOUT_MS 15000

// ----------------------------------------------------------------------------
// 2. Hardware Pin Assignments (38-Pin ESP32 DevKit)
// Verified for electrical safety, no strapping conflicts, ADC1 for Wi-Fi concurrency
// ----------------------------------------------------------------------------
// DHT22 Digital Temperature/Humidity Sensors
#define PIN_DHT_NORMAL          4   // Normal Compartment DHT22 data (GPIO 4)
#define PIN_DHT_COLD            16  // Cold Compartment DHT22 data (GPIO 16)
#define DHT_TYPE                DHT22

// Magnetic Reed Door Switches (Normally-open, internal pullup)
#define PIN_DOOR_NORMAL         32  // Normal Door Switch (GPIO 32)
#define PIN_DOOR_COLD           33  // Cold Door Switch (GPIO 33)

// I2C 0.96-inch OLED Display (SSD1306, 128x64)
#define PIN_OLED_SDA            21  // Hardware I2C SDA (GPIO 21)
#define PIN_OLED_SCL            22  // Hardware I2C SCL (GPIO 22)
#define OLED_I2C_ADDR           0x3C
#define SCREEN_WIDTH            128
#define SCREEN_HEIGHT           64

// RC522 RFID Reader (SPI - VSPI Bus) - VCC MUST BE 3.3V ONLY
#define PIN_RC522_SCK           18  // VSPI SCK (GPIO 18)
#define PIN_RC522_MISO          19  // VSPI MISO (GPIO 19)
#define PIN_RC522_MOSI          23  // VSPI MOSI (GPIO 23)
#define PIN_RC522_SS            17  // Chip Select / NSS (GPIO 17)
#define PIN_RC522_RST           27  // Hardware Reset (GPIO 27)

// Light-Dependent Resistors (LDR) - ADC1 Channels (Safe during active Wi-Fi)
#define PIN_LDR_NORMAL          34  // Normal LDR Analog (ADC1_CH6, input only)
#define PIN_LDR_COLD            35  // Cold LDR Analog (ADC1_CH7, input only)
#define LDR_LIGHT_THRESHOLD     1800 // ADC threshold (0-4095) for detected light

// Actuators & Indicators
#define PIN_BUZZER              25  // Active/Passive Buzzer (GPIO 25)
#define PIN_LED_NORMAL          13  // Green Status LED (GPIO 13)
#define PIN_LED_ALERT           14  // Red Alert / Warning LED (GPIO 14)

// 12V 2-Channel Relay Module (Cooling / Peltier & Fan control)
#define PIN_RELAY_COOLING       26  // IN1: Relay for Cold Compartment Cooling (GPIO 26)

// ----------------------------------------------------------------------------
// 3. Hardware Safety & Guardrails
// ----------------------------------------------------------------------------
// CRITICAL: Relay & Peltier activation disabled by default until hardware,
// electrical isolation, flyback diode, and current capacities are verified.
#define ENABLE_RELAY_OUTPUT     false
#define ENABLE_PELTIER_OUTPUT   false

// Relay logic: Configurable trigger level based on physical module
// Set to true if the relay board is Active-LOW (standard opto-isolated modules)
// Set to false if the relay board is Active-HIGH
#define RELAY_IS_ACTIVE_LOW     true

#if RELAY_IS_ACTIVE_LOW
  #define RELAY_ACTIVE_LEVEL    LOW
  #define RELAY_INACTIVE_LEVEL  HIGH
#else
  #define RELAY_ACTIVE_LEVEL    HIGH
  #define RELAY_INACTIVE_LEVEL  LOW
#endif

// Safe Temperature Limits (Celsius)
#define NORMAL_TEMP_MIN_C       15.0f
#define NORMAL_TEMP_MAX_C       25.0f
#define COLD_TEMP_MIN_C         2.0f
#define COLD_TEMP_MAX_C         8.0f

// Door open warning time before local audio-visual buzzer alarm (seconds)
#define DOOR_OPEN_ALARM_SEC     60

// Serial Diagnostics
#define SERIAL_BAUD_RATE        115200

#endif // MEDIGUARD_CONFIG_H
