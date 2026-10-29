#ifndef MEDIGUARD_SENSORS_MANAGER_H
#define MEDIGUARD_SENSORS_MANAGER_H

#include <Arduino.h>
#include <DHT.h>
#include "config.h"
#include "types.h"

class SensorsManager {
public:
    SensorsManager();
    void begin();
    void update(); // Non-blocking sampling routine

    const CompartmentReading& getNormal() const { return _normal; }
    const CompartmentReading& getCold() const { return _cold; }
    SensorHealthStatus getOverallHealth() const;
    bool isAnyDoorOpen() const { return _normal.doorOpen || _cold.doorOpen; }
    bool isDoorAlarmTriggered() const;

private:
    DHT _dhtNormal;
    DHT _dhtCold;

    CompartmentReading _normal;
    CompartmentReading _cold;

    uint32_t _lastDhtSampleTime;
    uint32_t _lastDoorSampleTime;

    uint8_t _normalFaultCount;
    uint8_t _coldFaultCount;

    // Debounce tracking
    bool _lastRawNormalDoor;
    bool _lastRawColdDoor;
    uint32_t _normalDoorDebounceTime;
    uint32_t _coldDoorDebounceTime;

    void sampleDoors();
    void sampleLDRs();
    void sampleDHTs();
};

#endif // MEDIGUARD_SENSORS_MANAGER_H
