#ifndef I2C_MANAGER_H
#define I2C_MANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include <vector>
#include <memory>
#include "devices/BME280Device.h"
#include "config/ConfigManager.h"
#include "diagnostics/DiagnosticManager.h"

class TimeManager; // Forward declaration
class DeviceManager; // Forward declaration

class I2CManager {
public:
    void begin(ConfigManager* config, DiagnosticManager* diag);
    void scanBus();
    void autoDetectDevices();
    void rescanDevices(); // Will also auto-register BME280s
    bool devicePresent(uint8_t address);
    void writeByte(uint8_t address, uint8_t reg, uint8_t value);
    uint8_t readByte(uint8_t address, uint8_t reg);
    void autoRegisterBME280s(TimeManager* timeMgr, class DeviceManager* deviceMgr = nullptr);
    int getSdaPin() const { return sdaPin; }
    int getSclPin() const { return sclPin; }
    const std::vector<uint8_t>& getDetectedDevices() const { return detectedDevices; }
    const std::vector<std::unique_ptr<BME280Device>>& getBME280Devices() const { return bme280Devices; }
    cJSON* getI2CInfoJson() const; // Returns I2C info as a cJSON object

    // Wire-like API for compatibility with device drivers
    void beginTransmission(uint8_t address);
    void write(uint8_t data);
    uint8_t endTransmission();
    uint8_t requestFrom(uint8_t address, uint8_t quantity);
    int read();
private:
    int sdaPin = 21;
    int sclPin = 22;
    ConfigManager* configManager = nullptr;
    DiagnosticManager* diagnosticManager = nullptr;
    std::vector<uint8_t> detectedDevices;
    std::vector<std::unique_ptr<BME280Device>> bme280Devices;
};

#endif // I2C_MANAGER_H
