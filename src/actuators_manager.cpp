#include "actuators_manager.h"

ActuatorsManager::ActuatorsManager()
    : _coolingActive(false),
      _fanActive(false),
      _beepEndTime(0),
      _beeping(false),
      _lastHeartbeatTime(0),
      _heartbeatState(false),
      _lastAlarmBeepTime(0),
      _alarmBeepState(false),
      _lastCoolingStateChange(0) {
}

void ActuatorsManager::begin() {
    Serial.println("[ACTUATORS] Initializing buzzer, status LEDs, and relay outputs...");

    // Configure Status LEDs
    pinMode(PIN_LED_NORMAL, OUTPUT);
    digitalWrite(PIN_LED_NORMAL, LOW);

    pinMode(PIN_LED_ALERT, OUTPUT);
    digitalWrite(PIN_LED_ALERT, LOW);

    // Configure Buzzer
    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_BUZZER, LOW);

    // Configure Relay Pin
    pinMode(PIN_RELAY_COOLING, OUTPUT);
    // Explicitly enforce inactive state at boot
    digitalWrite(PIN_RELAY_COOLING, RELAY_INACTIVE_LEVEL);

    if (!ENABLE_RELAY_OUTPUT) {
        Serial.println("[ACTUATORS:SAFETY] Relay IN1 is HARDWARE-LOCKED to OFF. (ENABLE_RELAY_OUTPUT=false).");
        Serial.println("[ACTUATORS:SAFETY] Cold cooling relay will NOT energize until hardware verification.");
    } else {
        Serial.println("[ACTUATORS:SAFETY] WARNING: Relay output is ENABLED. Thermal protection active.");
    }

    // Startup chirp
    beep(80);
}

void ActuatorsManager::beep(uint16_t durationMs) {
    digitalWrite(PIN_BUZZER, HIGH);
    _beepEndTime = millis() + durationMs;
    _beeping = true;
}

void ActuatorsManager::triggerRfidBeep() {
    beep(60); // 60ms clean confirmation chirp for verified RFID scan
}

void ActuatorsManager::triggerInvalidRfidAlert() {
    // Distinct double warning chirp and alert LED flash for unregistered/invalid tag
    digitalWrite(PIN_LED_ALERT, HIGH);
    digitalWrite(PIN_BUZZER, HIGH);
    delay(100);
    digitalWrite(PIN_BUZZER, LOW);
    delay(70);
    digitalWrite(PIN_BUZZER, HIGH);
    delay(160);
    digitalWrite(PIN_BUZZER, LOW);
    digitalWrite(PIN_LED_ALERT, LOW);
}

void ActuatorsManager::update(const CompartmentReading& normal,
                              const CompartmentReading& cold,
                              SensorHealthStatus health,
                              bool doorAlarmActive) {
    uint32_t now = millis();

    // 1. Handle non-blocking single beep timer
    if (_beeping && (now >= _beepEndTime)) {
        digitalWrite(PIN_BUZZER, LOW);
        _beeping = false;
    }

    // 2. Cooling & Relay Safety Logic
    updateRelaySafety(cold, health);

    // 3. Normal fan logic (simulated / safe state)
    _fanActive = (normal.valid && normal.temperature > 24.0f);

    // 4. Alert & Indicator Status
    bool tempAlert = (cold.valid && (cold.temperature > COLD_TEMP_MAX_C || cold.temperature < COLD_TEMP_MIN_C)) ||
                     (normal.valid && (normal.temperature > NORMAL_TEMP_MAX_C || normal.temperature < NORMAL_TEMP_MIN_C));
    bool faultAlert = (health == SENSOR_FAULT);
    bool overallAlert = tempAlert || faultAlert || doorAlarmActive;

    updateLeds(!overallAlert, overallAlert);

    // 5. Door Alarm / Emergency Buzzer
    updateBuzzer(doorAlarmActive);
}

void ActuatorsManager::updateRelaySafety(const CompartmentReading& cold, SensorHealthStatus health) {
    uint32_t now = millis();

    // HARD FAILSAFE 1: If relay output is disabled in config, NEVER energize relay.
    if (!ENABLE_RELAY_OUTPUT) {
        _coolingActive = false;
        digitalWrite(PIN_RELAY_COOLING, RELAY_INACTIVE_LEVEL);
        return;
    }

    // HARD FAILSAFE 2: If cold sensor is faulted or invalid, cut cooling immediately!
    // Prevents uncontrolled freezing of insulin / sensitive biologics.
    if (!cold.valid || health == SENSOR_FAULT) {
        if (_coolingActive) {
            Serial.println("[ACTUATORS:FAILSAFE] Cold sensor fault detected! Cutting cooling relay immediately!");
        }
        _coolingActive = false;
        digitalWrite(PIN_RELAY_COOLING, RELAY_INACTIVE_LEVEL);
        return;
    }

    // Thermostat Hysteresis for Cold Compartment:
    // Target range: 2.0°C to 8.0°C.
    // Turn ON cooling when temperature rises above 6.5°C.
    // Turn OFF cooling when temperature drops below 3.5°C.
    // Short-cycle delay: minimum 60 seconds between state transitions.
    if (now - _lastCoolingStateChange >= 60000) {
        if (!_coolingActive && cold.temperature > 6.5f) {
            _coolingActive = true;
            _lastCoolingStateChange = now;
            digitalWrite(PIN_RELAY_COOLING, RELAY_ACTIVE_LEVEL);
            Serial.printf("[ACTUATORS:RELAY] Cold temp (%.1fC) > 6.5C. Cooling relay ON.\n", cold.temperature);
        } else if (_coolingActive && cold.temperature < 3.5f) {
            _coolingActive = false;
            _lastCoolingStateChange = now;
            digitalWrite(PIN_RELAY_COOLING, RELAY_INACTIVE_LEVEL);
            Serial.printf("[ACTUATORS:RELAY] Cold temp (%.1fC) < 3.5C. Cooling relay OFF.\n", cold.temperature);
        }
    }
}

void ActuatorsManager::updateLeds(bool normalOk, bool alertActive) {
    uint32_t now = millis();

    // Heartbeat LED (Green): pulses every 1 second when healthy
    if (normalOk) {
        if (now - _lastHeartbeatTime >= 1000) {
            _lastHeartbeatTime = now;
            _heartbeatState = !_heartbeatState;
            digitalWrite(PIN_LED_NORMAL, _heartbeatState ? HIGH : LOW);
        }
    } else {
        digitalWrite(PIN_LED_NORMAL, LOW);
    }

    // Alert LED (Red): flashes at 2Hz if alert condition exists
    if (alertActive) {
        static uint32_t lastAlertFlash = 0;
        static bool alertState = false;
        if (now - lastAlertFlash >= 250) {
            lastAlertFlash = now;
            alertState = !alertState;
            digitalWrite(PIN_LED_ALERT, alertState ? HIGH : LOW);
        }
    } else {
        digitalWrite(PIN_LED_ALERT, LOW);
    }
}

void ActuatorsManager::updateBuzzer(bool doorAlarmActive) {
    uint32_t now = millis();

    // If door left open > 60s, beep every 500ms
    if (doorAlarmActive && !_beeping) {
        if (now - _lastAlarmBeepTime >= 500) {
            _lastAlarmBeepTime = now;
            _alarmBeepState = !_alarmBeepState;
            digitalWrite(PIN_BUZZER, _alarmBeepState ? HIGH : LOW);
        }
    }
}
