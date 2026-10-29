#include "sensors_manager.h"

SensorsManager::SensorsManager()
    : _dhtNormal(PIN_DHT_NORMAL, DHT_TYPE),
      _dhtCold(PIN_DHT_COLD, DHT_TYPE),
      _lastDhtSampleTime(0),
      _lastDoorSampleTime(0),
      _normalFaultCount(0),
      _coldFaultCount(0),
      _lastRawNormalDoor(false),
      _lastRawColdDoor(false),
      _normalDoorDebounceTime(0),
      _coldDoorDebounceTime(0) {
    memset(&_normal, 0, sizeof(_normal));
    memset(&_cold, 0, sizeof(_cold));
    _normal.temperature = -40.0f;
    _normal.humidity = 0.0f;
    _normal.valid = false;
    _cold.temperature = -40.0f;
    _cold.humidity = 0.0f;
    _cold.valid = false;
}

void SensorsManager::begin() {
    Serial.println("[SENSORS] Initializing DHT22 sensors, door switches, and LDR ADCs...");

    // Configure Door Reed Switches (Internal Pullup: Magnet closed = LOW, Magnet open = HIGH)
    pinMode(PIN_DOOR_NORMAL, INPUT_PULLUP);
    pinMode(PIN_DOOR_COLD, INPUT_PULLUP);

    _lastRawNormalDoor = (digitalRead(PIN_DOOR_NORMAL) == HIGH);
    _lastRawColdDoor = (digitalRead(PIN_DOOR_COLD) == HIGH);
    _normal.doorOpen = _lastRawNormalDoor;
    _cold.doorOpen = _lastRawColdDoor;

    // Configure LDR pins (ADC1 - input only)
    pinMode(PIN_LDR_NORMAL, INPUT);
    pinMode(PIN_LDR_COLD, INPUT);

    // Initialize DHT22 sensors
    _dhtNormal.begin();
    _dhtCold.begin();

    // Initial immediate sample
    sampleLDRs();
    sampleDHTs();

    Serial.println("[SENSORS] Initialization complete.");
}

void SensorsManager::update() {
    uint32_t now = millis();

    // Sample doors & LDRs frequently (every 50ms) for debounced switch detection
    if (now - _lastDoorSampleTime >= 50) {
        _lastDoorSampleTime = now;
        sampleDoors();
        sampleLDRs();
    }

    // Sample DHT22 sensors every 2000ms (respects DHT22 0.5Hz hardware limit)
    if (now - _lastDhtSampleTime >= 2000) {
        _lastDhtSampleTime = now;
        sampleDHTs();
    }
}

void SensorsManager::sampleDoors() {
    uint32_t now = millis();

    // 1. Debounce Normal Door Switch (50ms filter)
    bool rawNorm = (digitalRead(PIN_DOOR_NORMAL) == HIGH);
    if (rawNorm != _lastRawNormalDoor) {
        _normalDoorDebounceTime = now;
        _lastRawNormalDoor = rawNorm;
    }
    if ((now - _normalDoorDebounceTime) >= 50 && (_normal.doorOpen != rawNorm)) {
        _normal.doorOpen = rawNorm;
        if (rawNorm) {
            _normal.doorOpenTime = now;
            Serial.println("[DOOR] Normal compartment door OPENED");
        } else {
            _normal.doorOpenTime = 0;
            Serial.println("[DOOR] Normal compartment door CLOSED");
        }
    }

    // 2. Debounce Cold Door Switch (50ms filter)
    bool rawCold = (digitalRead(PIN_DOOR_COLD) == HIGH);
    if (rawCold != _lastRawColdDoor) {
        _coldDoorDebounceTime = now;
        _lastRawColdDoor = rawCold;
    }
    if ((now - _coldDoorDebounceTime) >= 50 && (_cold.doorOpen != rawCold)) {
        _cold.doorOpen = rawCold;
        if (rawCold) {
            _cold.doorOpenTime = now;
            Serial.println("[DOOR] Cold storage compartment door OPENED");
        } else {
            _cold.doorOpenTime = 0;
            Serial.println("[DOOR] Cold storage compartment door CLOSED");
        }
    }
}

void SensorsManager::sampleLDRs() {
    // Read 12-bit ADC (0 - 4095)
    _normal.ldrRaw = analogRead(PIN_LDR_NORMAL);
    _cold.ldrRaw = analogRead(PIN_LDR_COLD);

    // High analog voltage = high ambient light inside compartment
    _normal.lightDetected = (_normal.ldrRaw > LDR_LIGHT_THRESHOLD);
    _cold.lightDetected = (_cold.ldrRaw > LDR_LIGHT_THRESHOLD);
}

void SensorsManager::sampleDHTs() {
    // Read Normal DHT22
    float tNorm = _dhtNormal.readTemperature();
    float hNorm = _dhtNormal.readHumidity();

    if (isnan(tNorm) || isnan(hNorm) || tNorm < -40.0f || tNorm > 80.0f || hNorm < 0.0f || hNorm > 100.0f) {
        _normalFaultCount++;
        if (_normalFaultCount >= 2) {
            _normal.valid = false;
            _normal.temperature = -40.0f;
            _normal.humidity = 0.0f;
        }
    } else {
        _normal.temperature = tNorm;
        _normal.humidity = hNorm;
        _normal.valid = true;
        _normalFaultCount = 0;
    }

    // Read Cold DHT22
    float tCold = _dhtCold.readTemperature();
    float hCold = _dhtCold.readHumidity();

    if (isnan(tCold) || isnan(hCold) || tCold < -40.0f || tCold > 80.0f || hCold < 0.0f || hCold > 100.0f) {
        _coldFaultCount++;
        if (_coldFaultCount >= 2) {
            _cold.valid = false;
            _cold.temperature = -40.0f;
            _cold.humidity = 0.0f;
        }
    } else {
        _cold.temperature = tCold;
        _cold.humidity = hCold;
        _cold.valid = true;
        _coldFaultCount = 0;
    }
}

SensorHealthStatus SensorsManager::getOverallHealth() const {
    if (!_normal.valid || !_cold.valid) {
        return SENSOR_FAULT;
    }
    if (_normalFaultCount > 0 || _coldFaultCount > 0) {
        return SENSOR_WARNING;
    }
    return SENSOR_HEALTHY;
}

bool SensorsManager::isDoorAlarmTriggered() const {
    uint32_t now = millis();
    uint32_t maxDurationMs = DOOR_OPEN_ALARM_SEC * 1000UL;

    if (_normal.doorOpen && _normal.doorOpenTime > 0 && (now - _normal.doorOpenTime >= maxDurationMs)) {
        return true;
    }
    if (_cold.doorOpen && _cold.doorOpenTime > 0 && (now - _cold.doorOpenTime >= maxDurationMs)) {
        return true;
    }
    return false;
}
