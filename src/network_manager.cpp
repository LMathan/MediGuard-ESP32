#include "network_manager.h"

NetworkManager::NetworkManager()
    : _queueHead(0),
      _queueTail(0),
      _queueCount(0),
      _lastWiFiCheck(0),
      _reconnectBackoffMs(5000),
      _ntpSynced(false),
      _totalPacketsSent(0),
      _totalPacketsFailed(0),
      _lastTxSuccess(false) {
    memset(_offlineQueue, 0, sizeof(_offlineQueue));
}

void NetworkManager::begin() {
    Serial.println("[NETWORK] Initializing Wi-Fi station subsystem...");

    if (strcmp(DEVICE_API_KEY, "REPLACE_WITH_UNIQUE_DEVICE_SECRET_KEY") == 0) {
        Serial.println("[NETWORK:SECURITY WARNING] DEVICE_API_KEY is currently set to placeholder.");
        Serial.println("[NETWORK:SECURITY WARNING] Generate a unique key and register it in public.devices.serial_number.");
    }

    WiFi.mode(WIFI_STA);
    connectWiFi();
}

void NetworkManager::connectWiFi() {
    Serial.printf("[NETWORK:WiFi] Connecting to SSID: '%s'...\n", WIFI_SSID);
    WiFi.disconnect(true);
    delay(100);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    uint32_t startAttempt = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - startAttempt < WIFI_CONNECT_TIMEOUT_MS)) {
        delay(400);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[NETWORK:WiFi] Connected! IP: %s | RSSI: %d dBm\n",
                      WiFi.localIP().toString().c_str(), WiFi.RSSI());
        _reconnectBackoffMs = 5000;
        syncNtp();
        flushQueue();
    } else {
        Serial.printf("[NETWORK:WiFi] Connection attempt timed out (Status: %d). Will retry in background.\n",
                      WiFi.status());
    }
}

void NetworkManager::update() {
    uint32_t now = millis();

    // Check Wi-Fi state with backoff
    if (now - _lastWiFiCheck >= _reconnectBackoffMs) {
        _lastWiFiCheck = now;

        if (WiFi.status() != WL_CONNECTED) {
            Serial.printf("[NETWORK:WiFi] Disconnected. Reconnecting to '%s'...\n", WIFI_SSID);
            WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

            // Exponential backoff up to 60 seconds
            uint32_t nextBackoff = _reconnectBackoffMs * 2;
            _reconnectBackoffMs = (nextBackoff > 60000) ? 60000 : nextBackoff;
        } else {
            _reconnectBackoffMs = 10000; // Normal check interval once connected
            if (!_ntpSynced) {
                syncNtp();
            }
            if (_queueCount > 0) {
                flushQueue();
            }
        }
    }
}

void NetworkManager::syncNtp() {
    if (WiFi.status() != WL_CONNECTED) return;

    Serial.println("[NETWORK:NTP] Synchronizing UTC clock with pool.ntp.org...");
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");

    time_t now = time(nullptr);
    uint32_t start = millis();
    while (now < 24 * 3600 && (millis() - start < 4000)) {
        delay(300);
        now = time(nullptr);
    }

    if (now > 24 * 3600) {
        _ntpSynced = true;
        Serial.printf("[NETWORK:NTP] UTC Time synchronized: %s\n", getIso8601Timestamp().c_str());
    } else {
        Serial.println("[NETWORK:NTP] NTP sync still pending; will retry.");
    }
}

String NetworkManager::getIso8601Timestamp() {
    time_t now = time(nullptr);
    if (now < 24 * 3600) {
        return ""; // Empty string allows server to use its current timestamp
    }
    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);
    char buf[30];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
    return String(buf);
}

void NetworkManager::queueReading(const CompartmentReading& normal,
                                  const CompartmentReading& cold,
                                  bool coolingActive,
                                  bool fanActive,
                                  SensorHealthStatus health,
                                  const String& isoTime) {
    QueuedTelemetry& item = _offlineQueue[_queueTail];
    item.normTemp = normal.temperature;
    item.normHum = normal.humidity;
    item.coldTemp = cold.temperature;
    item.coldHum = cold.humidity;
    item.normDoor = normal.doorOpen;
    item.coldDoor = cold.doorOpen;
    item.normLight = normal.lightDetected;
    item.coldLight = cold.lightDetected;
    item.cooling = coolingActive;
    item.fan = fanActive;
    item.health = health;
    strncpy(item.recordedAt, isoTime.c_str(), sizeof(item.recordedAt) - 1);
    item.recordedAt[sizeof(item.recordedAt) - 1] = '\0';

    _queueTail = (_queueTail + 1) % QUEUE_CAPACITY;
    if (_queueCount < QUEUE_CAPACITY) {
        _queueCount++;
    } else {
        // Drop oldest to advance head
        _queueHead = (_queueHead + 1) % QUEUE_CAPACITY;
    }

    Serial.printf("[NETWORK:QUEUE] Reading buffered for offline recovery (Buffered: %d/%d)\n",
                  _queueCount, QUEUE_CAPACITY);
}

void NetworkManager::flushQueue() {
    if (WiFi.status() != WL_CONNECTED || _queueCount == 0) return;

    Serial.printf("[NETWORK:QUEUE] Flushing %d buffered historical readings...\n", _queueCount);

    while (_queueCount > 0) {
        const QueuedTelemetry& item = _offlineQueue[_queueHead];

        JsonDocument doc;
        doc["data_source"] = "hardware";
        doc["normal_temperature_c"] = round(item.normTemp * 10.0f) / 10.0f;
        doc["normal_humidity_pct"] = round(item.normHum * 10.0f) / 10.0f;
        doc["cold_temperature_c"] = round(item.coldTemp * 10.0f) / 10.0f;
        doc["cold_humidity_pct"] = round(item.coldHum * 10.0f) / 10.0f;
        doc["normal_door_open"] = item.normDoor;
        doc["cold_door_open"] = item.coldDoor;
        doc["normal_light_detected"] = item.normLight;
        doc["cold_light_detected"] = item.coldLight;
        doc["cooling_active"] = item.cooling;
        doc["normal_fan_active"] = item.fan;
        doc["sensor_health"] = sensorHealthToString(item.health);

        if (strlen(item.recordedAt) > 0) {
            doc["recorded_at"] = item.recordedAt;
        }

        String jsonBuffer;
        serializeJson(doc, jsonBuffer);

        if (postTelemetryPayload(jsonBuffer)) {
            _queueHead = (_queueHead + 1) % QUEUE_CAPACITY;
            _queueCount--;
            delay(100); // Small interval between queued bursts
        } else {
            Serial.println("[NETWORK:QUEUE] Flush paused due to network error.");
            break;
        }
    }
}

bool NetworkManager::postTelemetryPayload(const String& jsonBuffer) {
    WiFiClientSecure client;
    client.setInsecure(); // Cloudflare Supabase gateway TLS connection

    HTTPClient https;
    https.setTimeout(HTTP_REQUEST_TIMEOUT_MS);

    if (https.begin(client, SUPABASE_INGEST_URL)) {
        https.addHeader("Content-Type", "application/json");
        https.addHeader("x-device-key", DEVICE_API_KEY);

        int httpCode = https.POST(jsonBuffer);

        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED) {
            String response = https.getString();
            _totalPacketsSent++;
            _lastTxSuccess = true;
            Serial.printf("[NETWORK:HTTP 200] Telemetry accepted by MediGuard backend.\n");
            Serial.printf("[NETWORK:RX] %s\n", response.c_str());
            https.end();
            return true;
        } else if (httpCode > 0) {
            _totalPacketsFailed++;
            _lastTxSuccess = false;
            Serial.printf("[NETWORK:HTTP %d] Telemetry rejected: %s\n", httpCode, https.getString().c_str());
            https.end();
            return false;
        } else {
            _totalPacketsFailed++;
            _lastTxSuccess = false;
            Serial.printf("[NETWORK:HTTP ERROR] Network transmission failed: %s\n", https.errorToString(httpCode).c_str());
            https.end();
            return false;
        }
    } else {
        _totalPacketsFailed++;
        _lastTxSuccess = false;
        Serial.println("[NETWORK:HTTP ERROR] Failed to initialize HTTPS connection.");
        return false;
    }
}

bool NetworkManager::sendTelemetry(const CompartmentReading& normal,
                                  const CompartmentReading& cold,
                                  bool coolingActive,
                                  bool fanActive,
                                  SensorHealthStatus health) {
    String timestamp = getIso8601Timestamp();

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[NETWORK:HTTP] Wi-Fi not connected. Buffering reading into offline queue.");
        queueReading(normal, cold, coolingActive, fanActive, health, timestamp);
        _lastTxSuccess = false;
        _totalPacketsFailed++;
        return false;
    }

    // Build payload matching public.sensor_readings schema
    JsonDocument doc;
    doc["data_source"] = "hardware";
    doc["normal_temperature_c"] = round(normal.temperature * 10.0f) / 10.0f;
    doc["normal_humidity_pct"] = round(normal.humidity * 10.0f) / 10.0f;
    doc["cold_temperature_c"] = round(cold.temperature * 10.0f) / 10.0f;
    doc["cold_humidity_pct"] = round(cold.humidity * 10.0f) / 10.0f;
    doc["normal_door_open"] = normal.doorOpen;
    doc["cold_door_open"] = cold.doorOpen;
    doc["normal_light_detected"] = normal.lightDetected;
    doc["cold_light_detected"] = cold.lightDetected;
    doc["cooling_active"] = coolingActive;
    doc["normal_fan_active"] = fanActive;
    doc["sensor_health"] = sensorHealthToString(health);

    if (timestamp.length() > 0) {
        doc["recorded_at"] = timestamp;
    }

    String jsonBuffer;
    serializeJson(doc, jsonBuffer);

    Serial.println("\n--------------------------------------------------------");
    Serial.printf("[NETWORK:TX #%u] Uplinking hardware telemetry to MediGuard Cloud...\n", _totalPacketsSent + 1);
    Serial.printf("[TX Payload] %s\n", jsonBuffer.c_str());

    bool ok = postTelemetryPayload(jsonBuffer);
    if (!ok) {
        queueReading(normal, cold, coolingActive, fanActive, health, timestamp);
    }
    return ok;
}

bool NetworkManager::sendRfidScanEvent(const String& rfidUid, const char* compartment) {
    if (WiFi.status() != WL_CONNECTED || rfidUid.length() == 0) return false;

    // Send RFID inventory scan event
    Serial.printf("[NETWORK:RFID] Reporting tag scan event to backend: UID=%s, Compartment=%s\n",
                  rfidUid.c_str(), compartment);

    JsonDocument doc;
    doc["event_type"] = "rfid_scan";
    doc["rfid_uid"] = rfidUid;
    doc["compartment"] = compartment;
    doc["data_source"] = "hardware";
    
    String timestamp = getIso8601Timestamp();
    if (timestamp.length() > 0) {
        doc["recorded_at"] = timestamp;
    }

    String jsonBuffer;
    serializeJson(doc, jsonBuffer);

    return postTelemetryPayload(jsonBuffer);
}
