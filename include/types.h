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

enum RfidScanStatus {
    RFID_STATUS_NONE = 0,
    RFID_STATUS_VALID = 1,       // Recognized / registered medicine
    RFID_STATUS_INVALID = 2,     // Unregistered / invalid ID
    RFID_STATUS_PENDING = 3      // Verifying with cloud backend
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

struct RfidScanResult {
    String uid;
    RfidScanStatus status;
    String medicineName;
    String compartment;
    uint32_t timestamp;

    RfidScanResult()
        : uid(""),
          status(RFID_STATUS_NONE),
          medicineName(""),
          compartment(""),
          timestamp(0) {}
};

struct SystemTelemetry {
    CompartmentReading normal;
    CompartmentReading cold;
    bool coolingActive;     // State of cold cooling element
    bool fanActive;         // State of normal compartment fan
    SensorHealthStatus health;
    RfidScanResult lastScan; // Last scanned RFID card and validation result
    String isoTimestamp;    // UTC ISO-8601 timestamp string
};

#endif // MEDIGUARD_TYPES_H
