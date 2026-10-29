#ifndef MEDIGUARD_NETWORK_MANAGER_H
#define MEDIGUARD_NETWORK_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include "config.h"
#include "types.h"

class NetworkManager {
public:
    NetworkManager();
    void begin();
    void update(); // Non-blocking Wi-Fi monitor & NTP sync

    bool isConnected() const { return WiFi.status() == WL_CONNECTED; }
    int getRssi() const { return WiFi.RSSI(); }
    String getIpAddress() const { return WiFi.localIP().toString(); }

    bool sendTelemetry(const CompartmentReading& normal,
                       const CompartmentReading& cold,
                       bool coolingActive,
                       bool fanActive,
                       SensorHealthStatus health);

    bool sendRfidScanEvent(const String& rfidUid, const char* compartment);

    uint32_t getTotalSent() const { return _totalPacketsSent; }
    uint32_t getTotalFailed() const { return _totalPacketsFailed; }
    bool getLastTxSuccess() const { return _lastTxSuccess; }
    String getIso8601Timestamp();

private:
    struct QueuedTelemetry {
        float normTemp;
        float normHum;
        float coldTemp;
        float coldHum;
        bool normDoor;
        bool coldDoor;
        bool normLight;
        bool coldLight;
        bool cooling;
        bool fan;
        SensorHealthStatus health;
        char recordedAt[25];
    };

    static const uint8_t QUEUE_CAPACITY = 10;
    QueuedTelemetry _offlineQueue[QUEUE_CAPACITY];
    uint8_t _queueHead;
    uint8_t _queueTail;
    uint8_t _queueCount;

    uint32_t _lastWiFiCheck;
    uint32_t _reconnectBackoffMs;
    bool _ntpSynced;
    uint32_t _totalPacketsSent;
    uint32_t _totalPacketsFailed;
    bool _lastTxSuccess;

    void connectWiFi();
    void syncNtp();
    void queueReading(const CompartmentReading& normal,
                      const CompartmentReading& cold,
                      bool coolingActive,
                      bool fanActive,
                      SensorHealthStatus health,
                      const String& isoTime);
    void flushQueue();
    bool postTelemetryPayload(const String& jsonBuffer);
};

#endif // MEDIGUARD_NETWORK_MANAGER_H
