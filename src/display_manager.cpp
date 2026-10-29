#include "display_manager.h"

DisplayManager::DisplayManager()
    : _display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1),
      _isAvailable(false),
      _lastRenderTime(0) {
}

bool DisplayManager::begin() {
    Serial.println("[DISPLAY] Initializing SSD1306 128x64 OLED display via I2C (SDA=21, SCL=22)...");
    Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);

    if (!_display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR)) {
        Serial.println("[DISPLAY] Warning: SSD1306 allocation failed or not detected at 0x3C.");
        _isAvailable = false;
        return false;
    }

    _isAvailable = true;
    _display.clearDisplay();
    _display.setTextColor(SSD1306_WHITE);

    showSplash();
    return true;
}

void DisplayManager::showSplash() {
    if (!_isAvailable) return;

    _display.clearDisplay();
    _display.setTextSize(1);
    _display.setCursor(18, 8);
    _display.println(F("MEDIGUARD SMART"));
    _display.setCursor(24, 20);
    _display.println(F("STORAGE BOX"));

    _display.drawFastHLine(10, 32, 108, SSD1306_WHITE);

    _display.setCursor(8, 38);
    _display.println(F("Dual-Zone Clinical"));
    _display.setCursor(8, 48);
    _display.println(F("Monitoring System"));
    _display.display();
    delay(1200);
}

void DisplayManager::showMessage(const char* title, const char* msg) {
    if (!_isAvailable) return;

    _display.clearDisplay();
    _display.setTextSize(1);
    _display.setCursor(0, 0);
    _display.println(title);
    _display.drawFastHLine(0, 10, 128, SSD1306_WHITE);
    _display.setCursor(0, 18);
    _display.println(msg);
    _display.display();
}

void DisplayManager::update(const CompartmentReading& normal,
                            const CompartmentReading& cold,
                            SensorHealthStatus health,
                            bool wifiConnected,
                            bool lastTxSuccess,
                            const String& recentRfid,
                            uint32_t rfidTimestamp) {
    if (!_isAvailable) return;

    uint32_t now = millis();
    if (now - _lastRenderTime < 300) {
        return; // Render at ~3Hz to minimize I2C bus traffic
    }
    _lastRenderTime = now;

    _display.clearDisplay();

    // Check if RFID scan modal should be active (within 3 seconds of scan)
    if (recentRfid.length() > 0 && (now - rfidTimestamp < 3000)) {
        _display.drawRect(2, 2, 124, 60, SSD1306_WHITE);
        _display.fillRect(4, 4, 120, 12, SSD1306_WHITE);
        _display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
        _display.setTextSize(1);
        _display.setCursor(12, 6);
        _display.println(F("RFID BADGE SCANNED"));

        _display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
        _display.setCursor(10, 24);
        _display.println(F("Medicine UID:"));
        _display.setCursor(10, 36);
        _display.setTextSize(1);
        _display.println(recentRfid);

        _display.setCursor(10, 48);
        _display.println(F("Transmitting event..."));
        _display.display();
        return;
    }

    _display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);

    // --- TOP STATUS BAR ---
    _display.setTextSize(1);
    _display.setCursor(0, 0);
    _display.print(F("MEDIGUARD"));

    _display.setCursor(76, 0);
    if (wifiConnected) {
        _display.print(F("W:OK"));
    } else {
        _display.print(F("W:--"));
    }

    _display.setCursor(106, 0);
    if (lastTxSuccess) {
        _display.print(F("C:OK"));
    } else {
        _display.print(F("C:--"));
    }

    _display.drawFastHLine(0, 9, 128, SSD1306_WHITE);

    // --- COLD COMPARTMENT SECTION (Top half) ---
    _display.setCursor(0, 13);
    _display.print(F("COLD "));
    if (cold.valid) {
        _display.printf("%4.1fC %2.0f%%", cold.temperature, cold.humidity);
    } else {
        _display.print(F(" ERR SENSOR"));
    }

    _display.setCursor(96, 13);
    if (cold.doorOpen) {
        _display.print(F("[OPEN]"));
    } else {
        _display.print(F("[CLSD]"));
    }

    // --- NORMAL COMPARTMENT SECTION (Middle) ---
    _display.setCursor(0, 26);
    _display.print(F("NORM "));
    if (normal.valid) {
        _display.printf("%4.1fC %2.0f%%", normal.temperature, normal.humidity);
    } else {
        _display.print(F(" ERR SENSOR"));
    }

    _display.setCursor(96, 26);
    if (normal.doorOpen) {
        _display.print(F("[OPEN]"));
    } else {
        _display.print(F("[CLSD]"));
    }

    _display.drawFastHLine(0, 38, 128, SSD1306_WHITE);

    // --- BOTTOM ALERT / HEALTH FOOTER ---
    _display.setCursor(0, 43);

    // Determine bottom message banner
    if (cold.doorOpen || normal.doorOpen) {
        _display.print(F("! DOOR OPEN !"));
    } else if (cold.temperature > COLD_TEMP_MAX_C) {
        _display.printf("! COLD HIGH: %.1fC !", cold.temperature);
    } else if (cold.temperature < COLD_TEMP_MIN_C) {
        _display.printf("! COLD LOW: %.1fC !", cold.temperature);
    } else if (normal.temperature > NORMAL_TEMP_MAX_C || normal.temperature < NORMAL_TEMP_MIN_C) {
        _display.printf("! NORM TEMP EXCURSION !");
    } else if (health == SENSOR_FAULT) {
        _display.print(F("! SENSOR DISCONNECTED !"));
    } else {
        _display.print(F("Status: ALL OPTIMAL"));
    }

    _display.setCursor(0, 54);
    _display.printf("Cold Light: %s | N: %s",
                   cold.lightDetected ? "ON" : "OFF",
                   normal.lightDetected ? "ON" : "OFF");

    _display.display();
}
