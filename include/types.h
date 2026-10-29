#ifndef MEDIGUARD_TYPES_H
#define MEDIGUARD_TYPES_H

#include <Arduino.h>

// ============================================================================
// MediGuard Common Data Types & Enums
// ============================================================================

enum SensorHealthStatus {
    SENSOR_HEALTHY = 0,
    SENSOR_WARNING = 1,
    SENSOR_FAULT   = 2
};

inline const char* sensorHealthToString(SensorHealthStatus s) {
    switch (s) {
        case SENSOR_HEALTHY: return "healthy";
        case SENSOR_WARNING: return "warning";
        case SENSOR_FAULT:   return "error";
        default:             return "healthy";
    }
}

struct CompartmentReading {
    float temperature;      // In Celsius (-40.0 to +80.0)
    float humidity;         // Relative Humidity % (0.0 to 100.0)
    bool doorOpen;          // true = Door is physically open
    bool lightDetected;     // true = Internal compartment light sensed
    uint16_t ldrRaw;        // Raw ADC reading (0-4095)
    bool valid;             // true = Sensor reading verified & healthy
    uint32_t doorOpenTime;  // Timestamp (millis) when door opened
};

struct SystemTelemetry {
    CompartmentReading normal;
    CompartmentReading cold;
    bool coolingActive;     // State of cold cooling element
    bool fanActive;         // State of normal compartment fan
    SensorHealthStatus health;
    String lastRfidUid;     // Last scanned RFID card UID
    uint32_t rfidScanTime;  // Timestamp of last card scan
    String isoTimestamp;    // UTC ISO-8601 timestamp string
};

#endif // MEDIGUARD_TYPES_H
