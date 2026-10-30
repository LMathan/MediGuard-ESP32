#ifndef MEDIGUARD_ACTUATORS_MANAGER_H
#define MEDIGUARD_ACTUATORS_MANAGER_H

#include <Arduino.h>
#include "config.h"
#include "types.h"

class ActuatorsManager {
public:
    ActuatorsManager();
    void begin();
    void update(const CompartmentReading& normal,
                const CompartmentReading& cold,
                SensorHealthStatus health,
                bool doorAlarmActive);

    void beep(uint16_t durationMs = 50);
    void triggerRfidBeep();
    void triggerInvalidRfidAlert();
    void setRelayOverride(bool state);
    bool isCoolingActive() const { return _coolingActive; }
    bool isFanActive() const { return _fanActive; }

private:
    bool _coolingActive;
    bool _fanActive;
    uint32_t _beepEndTime;
    bool _beeping;

    uint32_t _lastHeartbeatTime;
    bool _heartbeatState;

    uint32_t _lastAlarmBeepTime;
    bool _alarmBeepState;

    uint32_t _lastCoolingStateChange;

    void updateRelaySafety(const CompartmentReading& cold, SensorHealthStatus health);
    void updateLeds(bool normalOk, bool alertActive);
    void updateBuzzer(bool doorAlarmActive);
};

#endif // MEDIGUARD_ACTUATORS_MANAGER_H
