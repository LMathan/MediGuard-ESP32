#include "rfid_manager.h"

RfidManager::RfidManager()
    : _mfrc522(PIN_RC522_SS, PIN_RC522_RST),
      _isAvailable(false),
      _lastUid(""),
      _lastScanTime(0),
      _lastPollTime(0) {
}

bool RfidManager::begin() {
    Serial.println("[RFID] Initializing RC522 SPI RFID Reader (SCK=18, MISO=19, MOSI=23, SS=17, RST=27)...");
    SPI.begin(PIN_RC522_SCK, PIN_RC522_MISO, PIN_RC522_MOSI, PIN_RC522_SS);
    _mfrc522.PCD_Init();

    // Check communication with reader chip
    byte version = _mfrc522.PCD_ReadRegister(_mfrc522.VersionReg);
    Serial.printf("[RFID] RC522 Firmware Version Register: 0x%02X\n", version);

    if (version == 0x00 || version == 0xFF) {
        Serial.println("[RFID] Warning: RC522 reader communication error. Check 3.3V power, GND, and SPI wiring.");
        _isAvailable = false;
        return false;
    }

    _isAvailable = true;
    Serial.println("[RFID] RC522 Reader detected and ready.");
    return true;
}

bool RfidManager::update() {
    if (!_isAvailable) return false;

    uint32_t now = millis();
    // Poll RC522 every 100ms
    if (now - _lastPollTime < 100) {
        return false;
    }
    _lastPollTime = now;

    // Check for card presence
    if (!_mfrc522.PICC_IsNewCardPresent()) {
        return false;
    }

    // Read card serial UID
    if (!_mfrc522.PICC_ReadCardSerial()) {
        return false;
    }

    // Convert UID bytes to uppercase Hex string
    String uidStr = "";
    for (byte i = 0; i < _mfrc522.uid.size; i++) {
        if (_mfrc522.uid.uidByte[i] < 0x10) {
            uidStr += "0";
        }
        uidStr += String(_mfrc522.uid.uidByte[i], HEX);
    }
    uidStr.toUpperCase();

    // Debounce: Ignore identical card within 3 seconds
    if (uidStr == _lastUid && (now - _lastScanTime < 3000)) {
        _mfrc522.PICC_HaltA();
        _mfrc522.PCD_StopCrypto1();
        return false;
    }

    _lastUid = uidStr;
    _lastScanTime = now;

    Serial.printf("[RFID] Scanned Card UID: %s (Size: %d bytes)\n", uidStr.c_str(), _mfrc522.uid.size);

    _mfrc522.PICC_HaltA();
    _mfrc522.PCD_StopCrypto1();
    return true;
}
