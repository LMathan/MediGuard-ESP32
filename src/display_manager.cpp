#include "display_manager.h"

DisplayManager::DisplayManager()
    : _display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1),
      _isAvailable(false),
      _lastRenderTime(0),
      _currentPage(0),
      _lastPageSwitchTime(0) {
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
                            const RfidScanResult& scanResult) {
    if (!_isAvailable) return;

    uint32_t now = millis();
    if (now - _lastRenderTime < 150) {
        return; // Render at ~6Hz to minimize I2C bus traffic
    }
    _lastRenderTime = now;

    _display.clearDisplay();

    // ========================================================================
    // PRIORITY 1: RFID / NFC Scan Event Override
    // Displays immediately when card is tapped, stays for 2.5 seconds, then resumes
    // ========================================================================
    if (scanResult.uid.length() > 0 && (now - scanResult.timestamp < 2500)) {
        _display.drawRect(2, 2, 124, 60, SSD1306_WHITE);
        _display.fillRect(4, 4, 120, 12, SSD1306_WHITE);
        _display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
        _display.setTextSize(1);

        if (scanResult.status == RFID_STATUS_INVALID) {
            // Header for Unregistered / Invalid ID
            _display.setCursor(14, 6);
            _display.println(F("[!] INVALID ID [!]"));

            _display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
            _display.setCursor(8, 20);
            _display.printf("UID: %s\n", scanResult.uid.c_str());
            _display.setCursor(8, 32);
            _display.println(F("NOT REGISTERED!"));
            _display.setCursor(8, 44);
            _display.println(F("Sent to Mobile App"));
        } else if (scanResult.status == RFID_STATUS_VALID) {
            // Header for Recognized & Registered Medicine
            _display.setCursor(12, 6);
            _display.println(F("MEDICINE VERIFIED"));

            _display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
            _display.setCursor(8, 20);
            _display.printf("UID: %s\n", scanResult.uid.c_str());
            _display.setCursor(8, 32);
            if (scanResult.medicineName.length() > 0) {
                _display.printf("%.14s\n", scanResult.medicineName.c_str());
            } else {
                _display.println(F("Status: Registered OK"));
            }
            _display.setCursor(8, 44);
            _display.printf("Zone: %s Storage\n", scanResult.compartment.c_str());
        } else {
            // Header while verifying with cloud
            _display.setCursor(16, 6);
            _display.println(F("CHECKING RFID..."));

            _display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
            _display.setCursor(8, 22);
            _display.printf("UID: %s\n", scanResult.uid.c_str());
            _display.setCursor(8, 36);
            _display.println(F("Verifying in Cloud..."));
            _display.setCursor(8, 48);
            _display.println(F("Please hold..."));
        }

        _display.display();
        return;
    }

    // ========================================================================
    // PRIORITY 2: Emergency Door Open Alert
    // Immediately interrupts carousel to show which door is open
    // ========================================================================
    if (cold.doorOpen || normal.doorOpen) {
        renderDoorAlertScreen(normal, cold);
        _display.display();
        return;
    }

    // ========================================================================
    // NORMAL ROTATING CAROUSEL LOOP
    // Cycles every 2.5 seconds between:
    // Screen 0: Cold Storage Compartment (Big Temp & Hum)
    // Screen 1: Normal Room Storage Compartment (Big Temp & Hum)
    // Screen 2: System Health, Wi-Fi & LDR Light Monitor
    // ========================================================================
    if (now - _lastPageSwitchTime >= 2500) {
        _currentPage = (_currentPage + 1) % 3;
        _lastPageSwitchTime = now;
    }

    switch (_currentPage) {
        case 0:
            renderColdScreen(cold, wifiConnected, lastTxSuccess);
            break;
        case 1:
            renderNormalScreen(normal, wifiConnected, lastTxSuccess);
            break;
        case 2:
        default:
            renderSystemScreen(normal, cold, health, wifiConnected, lastTxSuccess);
            break;
    }

    _display.display();
}

void DisplayManager::drawTopStatusBar(bool wifiConnected, bool lastTxSuccess, const char* pageTitle) {
    _display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
    _display.setTextSize(1);
    _display.setCursor(0, 0);
    _display.print(pageTitle);

    _display.setCursor(76, 0);
    _display.print(wifiConnected ? F("W:OK") : F("W:--"));

    _display.setCursor(106, 0);
    _display.print(lastTxSuccess ? F("C:OK") : F("C:--"));

    _display.drawFastHLine(0, 9, 128, SSD1306_WHITE);
}

void DisplayManager::renderColdScreen(const CompartmentReading& cold, bool wifiConnected, bool lastTxSuccess) {
    drawTopStatusBar(wifiConnected, lastTxSuccess, "COLD STORAGE");

    _display.setTextSize(1);
    _display.setCursor(0, 13);
    _display.println(F("Refrigerated (2-8C)"));

    // Big Temperature (Size 2 font)
    _display.setCursor(4, 25);
    _display.setTextSize(2);
    if (cold.valid) {
        _display.printf("%4.1f C\n", cold.temperature);
    } else {
        _display.setTextSize(1);
        _display.setCursor(14, 28);
        _display.println(F("[SENSOR FAULT]"));
    }

    // Humidity & Status Details
    _display.setTextSize(1);
    _display.setCursor(0, 44);
    if (cold.valid) {
        _display.printf("Humidity: %2.0f%% RH\n", cold.humidity);
    } else {
        _display.println(F("Check sensor wire"));
    }

    _display.setCursor(0, 55);
    _display.printf("Door: CLOSED | Dark: OK");
}

void DisplayManager::renderNormalScreen(const CompartmentReading& normal, bool wifiConnected, bool lastTxSuccess) {
    drawTopStatusBar(wifiConnected, lastTxSuccess, "ROOM STORAGE");

    _display.setTextSize(1);
    _display.setCursor(0, 13);
    _display.println(F("Standard (15-25C)"));

    // Big Temperature (Size 2 font)
    _display.setCursor(4, 25);
    _display.setTextSize(2);
    if (normal.valid) {
        _display.printf("%4.1f C\n", normal.temperature);
    } else {
        _display.setTextSize(1);
        _display.setCursor(14, 28);
        _display.println(F("[SENSOR FAULT]"));
    }

    // Humidity & Status Details
    _display.setTextSize(1);
    _display.setCursor(0, 44);
    if (normal.valid) {
        _display.printf("Humidity: %2.0f%% RH\n", normal.humidity);
    } else {
        _display.println(F("Check sensor wire"));
    }

    _display.setCursor(0, 55);
    _display.printf("Door: CLOSED | Dark: OK");
}

void DisplayManager::renderSystemScreen(const CompartmentReading& normal,
                                       const CompartmentReading& cold,
                                       SensorHealthStatus health,
                                       bool wifiConnected,
                                       bool lastTxSuccess) {
    drawTopStatusBar(wifiConnected, lastTxSuccess, "SYSTEM STATUS");

    _display.setTextSize(1);
    _display.setCursor(0, 13);
    _display.printf("Wi-Fi: %s\n", wifiConnected ? "ONLINE" : "OFFLINE");

    _display.setCursor(0, 24);
    _display.printf("Cloud: %s\n", lastTxSuccess ? "Supabase Synced" : "Pending...");

    _display.setCursor(0, 36);
    _display.println(F("Photosensitive LDRs:"));

    _display.setCursor(0, 47);
    _display.printf("Cold: %s\n", cold.lightDetected ? "! LIGHT LEAK !" : "DARK [Protected]");

    _display.setCursor(0, 56);
    _display.printf("Room: %s\n", normal.lightDetected ? "! LIGHT LEAK !" : "DARK [Protected]");
}

void DisplayManager::renderDoorAlertScreen(const CompartmentReading& normal, const CompartmentReading& cold) {
    _display.drawRect(2, 2, 124, 60, SSD1306_WHITE);
    _display.fillRect(4, 4, 120, 12, SSD1306_WHITE);
    _display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    _display.setTextSize(1);
    _display.setCursor(8, 6);
    _display.println(F("! DOOR OPEN ALERT !"));

    _display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
    _display.setCursor(10, 22);
    _display.printf("Cold Door:   %s\n", cold.doorOpen ? "OPEN  <--" : "CLOSED");

    _display.setCursor(10, 35);
    _display.printf("Normal Door: %s\n", normal.doorOpen ? "OPEN  <--" : "CLOSED");

    _display.setCursor(10, 49);
    _display.println(F("Close door to seal!"));
}
