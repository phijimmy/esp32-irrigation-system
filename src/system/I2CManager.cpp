#include "system/I2CManager.h"
#include "devices/BME280Device.h"
#include "devices/DeviceManager.h"
#include "system/TimeManager.h"

void I2CManager::begin(ConfigManager* config, DiagnosticManager* diag) {
    configManager = config;
    diagnosticManager = diag;
    if (i2cMutex == nullptr) {
        i2cMutex = xSemaphoreCreateMutex();
    }
    const char* sdaStr = configManager->get("i2c_sda");
    const char* sclStr = configManager->get("i2c_scl");
    sdaPin = sdaStr && *sdaStr ? atoi(sdaStr) : 21;
    sclPin = sclStr && *sclStr ? atoi(sclStr) : 22;
    Wire.begin(sdaPin, sclPin);
    if (diagnosticManager) diagnosticManager->log(DiagnosticManager::LOG_INFO, "I2C", "I2C bus initialized: SDA=%d, SCL=%d", sdaPin, sclPin);
    autoDetectDevices();
}

void I2CManager::autoDetectDevices() {
    detectedDevices.clear();
    if (diagnosticManager) diagnosticManager->log(DiagnosticManager::LOG_INFO, "I2C", "Auto-detecting I2C devices...");
    for (uint8_t addr = 1; addr < 127; ++addr) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            detectedDevices.push_back(addr);
            if (diagnosticManager) diagnosticManager->log(DiagnosticManager::LOG_INFO, "I2C", "Device detected at 0x%02X", addr);
        }
    }
    if (diagnosticManager) diagnosticManager->log(DiagnosticManager::LOG_INFO, "I2C", "Total I2C devices detected: %d", (int)detectedDevices.size());
}

void I2CManager::autoRegisterBME280s(TimeManager* timeMgr, class DeviceManager* deviceMgr) {
    bme280Devices.clear();
    for (uint8_t addr : detectedDevices) {
        // BME280 has known addresses: 0x76 or 0x77 (sometimes 0x70-0x77)
        if (addr == 0x76 || addr == 0x77 || addr == 0x70) {
            std::unique_ptr<BME280Device> dev(new BME280Device(addr, this, diagnosticManager));
            dev->setTimeManager(timeMgr);
            if (dev->begin()) {
                // Register the first BME280 device with DeviceManager if provided
                if (deviceMgr && bme280Devices.empty()) {
                    deviceMgr->setBME280Device(dev.get());
                }
                bme280Devices.push_back(std::move(dev));
                if (diagnosticManager) diagnosticManager->log(DiagnosticManager::LOG_INFO, "I2C", "BME280 auto-registered at 0x%02X", addr);
            }
        }
    }
}

void I2CManager::rescanDevices() {
    autoDetectDevices();
}

void I2CManager::scanBus() {
    if (diagnosticManager) diagnosticManager->log(DiagnosticManager::LOG_INFO, "I2C", "Scanning I2C bus...");
    for (uint8_t addr = 1; addr < 127; ++addr) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            if (diagnosticManager) diagnosticManager->log(DiagnosticManager::LOG_INFO, "I2C", "Device found at 0x%02X", addr);
        }
    }
}

bool I2CManager::devicePresent(uint8_t address) {
    Wire.beginTransmission(address);
    return Wire.endTransmission() == 0;
}

void I2CManager::writeByte(uint8_t address, uint8_t reg, uint8_t value) {
    Wire.beginTransmission(address);
    Wire.write(reg);
    Wire.write(value);
    Wire.endTransmission();
}

uint8_t I2CManager::readByte(uint8_t address, uint8_t reg) {
    Wire.beginTransmission(address);
    Wire.write(reg);
    Wire.endTransmission(false);
    Wire.requestFrom(address, (uint8_t)1);
    if (Wire.available()) {
        return Wire.read();
    }
    return 0xFF;
}

// Wire-like API for compatibility with device drivers
void I2CManager::beginTransmission(uint8_t address) {
    Wire.beginTransmission(address);
}
void I2CManager::write(uint8_t data) {
    Wire.write(data);
}
uint8_t I2CManager::endTransmission() {
    return Wire.endTransmission();
}
uint8_t I2CManager::requestFrom(uint8_t address, uint8_t quantity) {
    return Wire.requestFrom(address, quantity);
}
int I2CManager::read() {
    return Wire.read();
}

cJSON* I2CManager::getI2CInfoJson() const {
    cJSON* info = cJSON_CreateObject();
    cJSON_AddNumberToObject(info, "sda_pin", sdaPin);
    cJSON_AddNumberToObject(info, "scl_pin", sclPin);
    cJSON* devices = cJSON_CreateArray();
    for (uint8_t addr : detectedDevices) {
        cJSON* dev = cJSON_CreateObject();
        char addrStr[6];
        snprintf(addrStr, sizeof(addrStr), "0x%02X", addr);
        cJSON_AddStringToObject(dev, "address", addrStr);
        // Map known addresses to device names
        const char* name = "Unknown";
        if (addr == 0x76 || addr == 0x77) name = "BME280";
        else if (addr == 0x68) name = "DS3231";
        else if (addr == 0x48) name = "ADS1115";
        else if (addr == 0x57) name = "DS3231_ALARM";
        cJSON_AddStringToObject(dev, "name", name);
        cJSON_AddItemToArray(devices, dev);
    }
    cJSON_AddItemToObject(info, "devices", devices);
    return info;
}
