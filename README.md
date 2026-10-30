# MediGuard ESP32 Hardware Firmware

Production-grade embedded firmware for the **MediGuard Smart Clinical Medicine Storage & Monitoring System**, running on the **ESP32 DevKit 38-Pin USB-C** micro-controller.

It orchestrates real-time environmental monitoring across two separate clinical compartments:
1. **Normal Compartment** (15.0°C – 25.0°C): Room-temperature medications, tablets, dry suspensions.
2. **Cold Storage Compartment** (2.0°C – 8.0°C): Refrigerated biologics, insulins, vaccines, and eye drops.

---

## 1. Hardware Architecture & Electrical Pin Mapping

> [!CAUTION]
> **ELECTRICAL SAFETY & LOGIC LEVEL WARNINGS**:
> - ESP32 GPIOs operate strictly at **3.3V logic**. Never connect 5V or 12V signals directly to any GPIO pin.
> - **RC522 RFID Reader**: VCC MUST be connected to the **3.3V** rail (never 5V).
> - **LDR Sensors**: Connected to **ADC1** pins (GPIO 34 & 35). ADC1 is immune to Wi-Fi transmission interference (unlike ADC2 which is disabled during active Wi-Fi).
> - **Relay Module**: Must be optically isolated with a separate 5V/12V VCC supply and common GND. Relay activation is **DISABLED BY DEFAULT** in firmware for safety.

| Component | ESP32 GPIO | Pin Mode / Type | Electrical Characteristics & Notes |
| :--- | :--- | :--- | :--- |
| **Normal DHT22 Sensor** | `GPIO 4` | Digital Bidirectional | 3.3V VCC. 4.7kΩ–10kΩ pull-up to 3.3V. |
| **Cold DHT22 Sensor** | `GPIO 16` | Digital Bidirectional | 3.3V VCC. 4.7kΩ–10kΩ pull-up to 3.3V. |
| **Normal Door Switch** | `GPIO 32` | Digital Input (`INPUT_PULLUP`) | Magnetic Reed Switch. Switch to GND. LOW=Closed, HIGH=Open. |
| **Cold Door Switch** | `GPIO 33` | Digital Input (`INPUT_PULLUP`) | Magnetic Reed Switch. Switch to GND. LOW=Closed, HIGH=Open. |
| **OLED Display (SDA)** | `GPIO 21` | I²C Bus (Data) | SSD1306 0.96" 128x64 display. 3.3V VCC. |
| **OLED Display (SCL)** | `GPIO 22` | I²C Bus (Clock) | SSD1306 0.96" 128x64 display. 3.3V VCC. |
| **RC522 RFID (SCK)** | `GPIO 18` | SPI Bus (SCK) | VSPI Clock. 3.3V logic. |
| **RC522 RFID (MISO)** | `GPIO 19` | SPI Bus (MISO) | VSPI Master In Slave Out. |
| **RC522 RFID (MOSI)** | `GPIO 23` | SPI Bus (MOSI) | VSPI Master Out Slave In. |
| **RC522 RFID (SS/SDA)** | `GPIO 17` | Digital Output | Chip Select / NSS. 3.3V logic. |
| **RC522 RFID (RST)** | `GPIO 27` | Digital Output | Hardware Reset. 3.3V logic. |
| **Normal Compartment LDR** | `GPIO 34` | Analog Input (`ADC1_CH6`) | Voltage divider (LDR + 10kΩ to GND). Input only. |
| **Cold Compartment LDR** | `GPIO 35` | Analog Input (`ADC1_CH7`) | Voltage divider (LDR + 10kΩ to GND). Input only. |
| **Status LED: Normal (Green)** | `GPIO 13` | Digital Output | Active-HIGH. Series resistor 220Ω–330Ω to GND. |
| **Status LED: Alert (Red)** | `GPIO 14` | Digital Output | Active-HIGH. Series resistor 220Ω–330Ω to GND. |
| **Buzzer Signal** | `GPIO 25` | Digital Output | Transistor driver (NPN 2N2222) recommended. |
| **Relay IN1 (Cooling Control)** | `GPIO 26` | Digital Output | **DISABLED BY DEFAULT**. Hardware-locked to OFF. Polarity configurable via `RELAY_IS_ACTIVE_LOW`. Verify physical module inputs before enabling. |

---

## 2. Firmware Architecture & Module Structure

The firmware employs a non-blocking, timer-driven architecture where no sensor, display, or network operation blocks the cooperative loop:

```
include/
  config.h             # Network credentials, pin assignments, safety thresholds, and flags
  types.h              # Common structs (SystemTelemetry, CompartmentReading) and enums
  sensors_manager.h    # Sampling routines for DHT22s, debounced reed switches, and LDRs
  display_manager.h    # SSD1306 OLED layout, status header, metrics, and RFID scan modals
  rfid_manager.h       # MFRC522 SPI driver with 3-second debouncing and UID hex extraction
  actuators_manager.h  # Buzzer chirps/alarms, heartbeat LED, and fail-safe relay watchdog
  network_manager.h    # Non-blocking Wi-Fi with backoff, NTP UTC clock, offline queue, and Supabase HTTPS POST
src/
  sensors_manager.cpp
  display_manager.cpp
  rfid_manager.cpp
  actuators_manager.cpp
  network_manager.cpp
  main.cpp             # Main scheduling loop and interactive Serial CLI
platformio.ini         # PlatformIO build configuration for esp32dev board
```

---

## 3. Telemetry Payload & Database Integration

Telemetry is transmitted via HTTPS POST to the Supabase Ingest Edge Function:
`https://bskjisnvumbsiwcahvyw.supabase.co/functions/v1/ingest-telemetry`

### Authenticated Request Headers:
```http
Content-Type: application/json
x-device-key: <YOUR_GENERATED_DEVICE_KEY>
```

### JSON Payload Schema (Matching `public.sensor_readings`):
```json
{
  "data_source": "hardware",
  "normal_temperature_c": 21.4,
  "normal_humidity_pct": 49.2,
  "cold_temperature_c": 4.6,
  "cold_humidity_pct": 53.8,
  "normal_door_open": false,
  "cold_door_open": false,
  "normal_light_detected": false,
  "cold_light_detected": false,
  "cooling_active": false,
  "normal_fan_active": false,
  "sensor_health": "healthy",
  "recorded_at": "2026-10-29T12:00:00Z"
}
```

### Real-Time Event Uplinks:
- **Door Transition**: Immediate packet sent upon door open or closed transition (debounced 50ms).
- **RFID Scan Event & Verification**:
  - Checks if the scanned UID is registered in Supabase.
  - **If Registered**: Plays a clean confirmation chirp and shows `MEDICINE VERIFIED` with medicine name and storage zone on the OLED.
  - **If Unregistered / Invalid**: Sounds a distinct warning double-beep, flashes the Red Alert LED, and displays `[!] INVALID ID [!]` with `NOT REGISTERED! Sent to Mobile App`. Automatically logs the unknown UID to `public.unregistered_medicines` for mobile app onboarding.

---

### Supabase Schema for Unregistered RFID Tags
See [supabase_unregistered_medicines.sql](file:///Users/aitech/Documents/App/MediGuard-ESP32/supabase_unregistered_medicines.sql) for table creation, indexes, and RLS policies.

---

## 4. Hardware Safety Watchdog & Guardrails

1. **Relay Failsafe & Module Verification**:
   - `ENABLE_RELAY_OUTPUT` is set to `false` by default.
   - 12V relay modules vary (active-low with optocoupler vs. active-high with NPN driver). Always check your module's input pins and VCC/JD-VCC jumper before connecting. Configure `RELAY_IS_ACTIVE_LOW` in `include/config.h` accordingly.
   - Never allow 5V or 12V to back-feed into ESP32 GPIO 26.
   - Even if enabled, the relay will **immediately cut off** if the cold DHT22 reports an invalid reading or fault (`health == SENSOR_FAULT`). This prevents medicine freezing.
2. **Door Open Alarm**:
   - If either compartment door remains open for more than **60 seconds**, an intermittent audio alarm sounds on the buzzer and the Red LED flashes continuously.
3. **Network Resiliency & Offline Queue**:
   - If Wi-Fi is lost, local sensor monitoring, display, buzzer, and RFID operations continue without interruption.
   - Up to 10 historical readings are stored in a RAM FIFO queue and flushed with original timestamps once connectivity restores.
   - Reconnection uses exponential backoff (5s up to 60s max).

---

## 5. Compiling and Flashing

### Using PlatformIO CLI
From this directory (`MediGuard-ESP32`):

```bash
# 1. Compile the firmware
/Users/aitech/.platformio/penv/bin/pio run

# 2. Upload to connected ESP32
/Users/aitech/.platformio/penv/bin/pio run --target upload

# 3. Open Serial Monitor (115200 baud)
/Users/aitech/.platformio/penv/bin/pio device monitor -b 115200
```

---

## 6. Interactive Serial Monitor Commands

Open the Serial Monitor at **115200 baud** to access the live diagnostics console:

| Command | Action |
| :--- | :--- |
| `status` or `s` | Prints complete real-time diagnostics: Wi-Fi RSSI, IP, temperatures, door states, LDR ADC, relay status, packet counters. |
| `send` or `tx` | Forces an immediate telemetry transmission to Supabase. |
| `beep` | Tests the buzzer with a 100ms audible chirp. |
| `scan_wifi` | Scans and lists all visible 2.4GHz Wi-Fi access points and signal levels. |
| `interval <S>` | Updates telemetry reporting interval dynamically (e.g. `interval 30`). |
| `help` or `?` | Displays available interactive commands. |

---

## 7. Supabase Device Registration (Security Notice)

> [!IMPORTANT]
> Do NOT use the example/test key `mg_dev_sec_c3_test_987654321` in production. It is a publicly exposed credential in git history.
> Generate a fresh, cryptographically random key:
> ```bash
> openssl rand -hex 16
> # Example output: 3b9e4a81f20d6c55981240a17eb9102c
> ```

1. Set your newly generated key in `include/config.h`:
   ```cpp
   #define DEVICE_API_KEY "YOUR_GENERATED_KEY_HERE"
   ```

2. Register the device key in the Supabase SQL Editor:
   ```sql
   INSERT INTO public.devices (
       id,
       user_id,
       device_name,
       model_version,
       serial_number,
       is_online,
       data_source
   ) VALUES (
       gen_random_uuid(),
       '<YOUR_SUPABASE_AUTH_USER_UUID>', -- Find in Supabase Auth > Users
       'MediGuard ESP32 Hardware Box #01',
       'ESP32-MediGuard-v1',
       'YOUR_GENERATED_KEY_HERE',        -- Matches DEVICE_API_KEY exactly
       true,
       'hardware'
   );
   ```
