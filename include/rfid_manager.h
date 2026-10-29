#ifndef MEDIGUARD_RFID_MANAGER_H
#define MEDIGUARD_RFID_MANAGER_H

#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include "config.h"

class RfidManager {
public:
    RfidManager();
    bool begin();
    bool update(); // Returns true if a NEW card was detected

    String getLastUid() const { return _lastUid; }
    uint32_t getLastScanTime() const { return _lastScanTime; }
    void clearLastUid() { _lastUid = ""; }
    bool isAvailable() const { return _isAvailable; }

private:
    MFRC522 _mfrc522;
    bool _isAvailable;
    String _lastUid;
    uint32_t _lastScanTime;
    uint32_t _lastPollTime;
};

#endif // MEDIGUARD_RFID_MANAGER_H
