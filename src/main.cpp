#include <Arduino.h>
#include "config.h"
#include "types.h"
#include "sensors_manager.h"
#include "display_manager.h"
#include "rfid_manager.h"
#include "actuators_manager.h"
#include "network_manager.h"

// ============================================================================
// MediGuard ESP32 Hardware Firmware
// Version: 1.0.0
// Target: ESP32 DevKit 38-Pin USB-C (esp32dev)
// ============================================================================

SensorsManager   sensors;
DisplayManager   display;
RfidManager      rfid;
ActuatorsManager actuators;
NetworkManager   network;

uint32_t lastTelemetryTime = 0;
uint32_t telemetryIntervalMs = TELEMETRY_INTERVAL_SEC * 1000UL;

bool lastNormalDoorState = false;
bool lastColdDoorState = false;

// Serial CLI prototypes
void processSerialCli();
void printDiagnostics();
void printHelp();

void setup() {
    Serial.begin(SERIAL_BAUD_RATE);
    delay(1000);

    Serial.println();
    Serial.println(F("========================================================"));
    Serial.println(F("   MediGuard ESP32 Clinical Storage System v1.0.0      "));
    Serial.println(F("   Target: ESP32 DevKit 38-Pin (esp32dev)              "));
    Serial.println(F("========================================================"));

    // 1. Initialize Display
    display.begin();

    // 2. Initialize Actuators & Safety Watchdog
    actuators.begin();

    // 3. Initialize Sensors Subsystem
    sensors.begin();

    // 4. Initialize RFID Reader
    rfid.begin();

    // 5. Initialize Network & Wi-Fi
    network.begin();

    // Store initial door states
    lastNormalDoorState = sensors.getNormal().doorOpen;
    lastColdDoorState = sensors.getCold().doorOpen;

    printHelp();
    Serial.println(F("[SYSTEM] Boot sequence complete. Entering cooperative operational loop...\n"));
}

void loop() {
    uint32_t now = millis();

    // 1. Cooperative subsystem updates
    sensors.update();
    network.update();

    // 2. Process RFID Reader
    bool newRfidCard = rfid.update();
    if (newRfidCard) {
        String uid = rfid.getLastUid();
        Serial.printf("[SYSTEM:RFID] Card Scanned: %s\n", uid.c_str());

        // Local auditory confirmation (50ms beep)
        actuators.triggerRfidBeep();

        // Determine compartment based on open door, or default to cold
        const char* comp = sensors.getNormal().doorOpen ? "normal" : "cold";
        network.sendRfidScanEvent(uid, comp);

        // Also trigger an immediate telemetry packet with the updated state
        network.sendTelemetry(
            sensors.getNormal(),
            sensors.getCold(),
            actuators.isCoolingActive(),
            actuators.isFanActive(),
            sensors.getOverallHealth()
        );
        lastTelemetryTime = now;
    }

    // 3. Immediate Event Uplink on Door State Transition
    bool curNormDoor = sensors.getNormal().doorOpen;
    bool curColdDoor = sensors.getCold().doorOpen;

    if (curNormDoor != lastNormalDoorState || curColdDoor != lastColdDoorState) {
        Serial.printf("[SYSTEM:EVENT] Door state transition detected (Norm: %s, Cold: %s). Triggering immediate uplink.\n",
                      curNormDoor ? "OPEN" : "CLOSED",
                      curColdDoor ? "OPEN" : "CLOSED");
        lastNormalDoorState = curNormDoor;
        lastColdDoorState = curColdDoor;

        network.sendTelemetry(
            sensors.getNormal(),
            sensors.getCold(),
            actuators.isCoolingActive(),
            actuators.isFanActive(),
            sensors.getOverallHealth()
        );
        lastTelemetryTime = now;
    }

    // 4. Actuators & Safety Interlock Update
    actuators.update(
        sensors.getNormal(),
        sensors.getCold(),
        sensors.getOverallHealth(),
        sensors.isDoorAlarmTriggered()
    );

    // 5. Display Update
    display.update(
        sensors.getNormal(),
        sensors.getCold(),
        sensors.getOverallHealth(),
        network.isConnected(),
        network.getLastTxSuccess(),
        rfid.getLastUid(),
        rfid.getLastScanTime()
    );

    // 6. Scheduled Telemetry Uplink
    if (now - lastTelemetryTime >= telemetryIntervalMs) {
        lastTelemetryTime = now;

        network.sendTelemetry(
            sensors.getNormal(),
            sensors.getCold(),
            actuators.isCoolingActive(),
            actuators.isFanActive(),
            sensors.getOverallHealth()
        );
    }

    // 7. Interactive Serial Commands
    processSerialCli();

    delay(10); // Small yield to prevent CPU starvation
}

// ============================================================================
// Interactive Serial Monitor Console
// ============================================================================
void processSerialCli() {
    if (!Serial.available()) return;

    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.length() == 0) return;

    String lower = cmd;
    lower.toLowerCase();

    if (lower == "status" || lower == "s") {
        printDiagnostics();
    } else if (lower == "send" || lower == "tx") {
        Serial.println(F("\n>>> [CLI] Triggering immediate telemetry transmission..."));
        network.sendTelemetry(
            sensors.getNormal(),
            sensors.getCold(),
            actuators.isCoolingActive(),
            actuators.isFanActive(),
            sensors.getOverallHealth()
        );
        lastTelemetryTime = millis();
    } else if (lower == "beep" || lower == "test_buzzer") {
        Serial.println(F(">>> [CLI] Testing buzzer (100ms chirp)..."));
        actuators.beep(100);
    } else if (lower == "help" || lower == "h" || lower == "?") {
        printHelp();
    } else if (lower == "scan_wifi") {
        Serial.println(F("\n[CLI] Scanning 2.4GHz Wi-Fi networks..."));
        int n = WiFi.scanNetworks();
        if (n == 0) {
            Serial.println(F("[CLI] No Wi-Fi networks found."));
        } else {
            Serial.printf("[CLI] Found %d networks:\n", n);
            for (int i = 0; i < n; ++i) {
                Serial.printf("  [%d] SSID: '%s' | RSSI: %d dBm | %s\n",
                              i + 1, WiFi.SSID(i).c_str(), WiFi.RSSI(i),
                              WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "OPEN" : "ENCRYPTED");
            }
        }
    } else if (lower.startsWith("interval ")) {
        int sec = lower.substring(9).toInt();
        if (sec >= 5 && sec <= 3600) {
            telemetryIntervalMs = sec * 1000UL;
            Serial.printf(">>> [CLI] Telemetry interval updated to %d seconds.\n", sec);
        } else {
            Serial.println(F(">>> [CLI] Error: Interval must be between 5 and 3600 seconds."));
        }
    } else {
        Serial.printf(">>> [CLI] Unknown command: '%s'. Type 'help' for available commands.\n", cmd.c_str());
    }
}

void printDiagnostics() {
    const CompartmentReading& norm = sensors.getNormal();
    const CompartmentReading& cold = sensors.getCold();

    Serial.println(F("\n================= MEDIGUARD DIAGNOSTICS ================="));
    Serial.printf("System Uptime:         %lu seconds\n", millis() / 1000UL);
    Serial.printf("Wi-Fi Status:          %s (SSID: '%s', IP: %s, RSSI: %d dBm)\n",
                  network.isConnected() ? "CONNECTED" : "DISCONNECTED",
                  WIFI_SSID,
                  network.getIpAddress().c_str(),
                  network.getRssi());
    Serial.printf("Packets Sent/Failed:   %u sent | %u failed\n",
                  network.getTotalSent(), network.getTotalFailed());
    Serial.printf("Sensor Health Status:  %s\n",
                  sensorHealthToString(sensors.getOverallHealth()));
    Serial.printf("Cold Compartment:      %.1f C | %.1f%% RH | Door: %s | Light ADC: %u (%s)\n",
                  cold.temperature, cold.humidity,
                  cold.doorOpen ? "OPEN" : "CLOSED",
                  cold.ldrRaw, cold.lightDetected ? "LIGHT" : "DARK");
    Serial.printf("Normal Compartment:    %.1f C | %.1f%% RH | Door: %s | Light ADC: %u (%s)\n",
                  norm.temperature, norm.humidity,
                  norm.doorOpen ? "OPEN" : "CLOSED",
                  norm.ldrRaw, norm.lightDetected ? "LIGHT" : "DARK");
    Serial.printf("Cooling Relay (IN1):   %s (Hardware Enabled: %s)\n",
                  actuators.isCoolingActive() ? "ACTIVE" : "INACTIVE",
                  ENABLE_RELAY_OUTPUT ? "YES" : "NO (SAFETY LOCKED)");
    Serial.printf("Normal Fan:            %s\n",
                  actuators.isFanActive() ? "ON" : "OFF");
    Serial.printf("Last Scanned RFID UID: %s\n",
                  rfid.getLastUid().length() > 0 ? rfid.getLastUid().c_str() : "None");
    Serial.println(F("=========================================================\n"));
}

void printHelp() {
    Serial.println(F("\n--- MediGuard Serial Console Commands ---"));
    Serial.println(F("  status        : Display full hardware and network diagnostic report"));
    Serial.println(F("  send          : Force immediate telemetry packet uplink to Supabase"));
    Serial.println(F("  beep          : Test 100ms buzzer chirp"));
    Serial.println(F("  scan_wifi     : Scan all visible 2.4GHz Wi-Fi access points"));
    Serial.println(F("  interval <S>  : Set telemetry report interval in seconds (5-3600)"));
    Serial.println(F("  help          : Show this help menu"));
    Serial.println(F("-----------------------------------------\n"));
}