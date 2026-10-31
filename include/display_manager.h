#ifndef MEDIGUARD_DISPLAY_MANAGER_H
#define MEDIGUARD_DISPLAY_MANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "config.h"
#include "types.h"

class DisplayManager {
public:
    DisplayManager();
    bool begin();
    void update(const CompartmentReading& normal,
                const CompartmentReading& cold,
                SensorHealthStatus health,
                bool wifiConnected,
                bool lastTxSuccess,
                const RfidScanResult& scanResult);
    void showSplash();
    void showMessage(const char* title, const char* msg);

private:
    Adafruit_SSD1306 _display;
    bool _isAvailable;
    uint32_t _lastRenderTime;
    uint8_t _currentPage;
    uint32_t _lastPageSwitchTime;

    void drawTopStatusBar(bool wifiConnected, bool lastTxSuccess, const char* pageTitle);
    void renderColdScreen(const CompartmentReading& cold, bool wifiConnected, bool lastTxSuccess);
    void renderNormalScreen(const CompartmentReading& normal, bool wifiConnected, bool lastTxSuccess);
    void renderSystemScreen(const CompartmentReading& normal, const CompartmentReading& cold, SensorHealthStatus health, bool wifiConnected, bool lastTxSuccess);
    void renderDoorAlertScreen(const CompartmentReading& normal, const CompartmentReading& cold);
};

#endif // MEDIGUARD_DISPLAY_MANAGER_H
