#include "devices/DeviceManager.h"
#include "devices/Device.h"
#include "devices/BME280Device.h"
#include "devices/SoilMoistureSensor.h"

void DeviceManager::begin(ConfigManager& configMgr) {
    configManager = &configMgr;
    for (int i = 0; i < deviceCount; ++i) {
        if (devices[i]) devices[i]->begin();
    }
}

void DeviceManager::update() {
    for (int i = 0; i < deviceCount; ++i) {
        if (devices[i]) devices[i]->update();
    }
}

void DeviceManager::addDevice(Device* device) {
    if (deviceCount < MAX_DEVICES) {
        devices[deviceCount++] = device;
    }
}

BME280Device* DeviceManager::getBME280Device() {
    return bme280Device;
}

SoilMoistureSensor* DeviceManager::getSoilMoistureSensor() {
    // Replace with actual instance if available
    extern SoilMoistureSensor soilMoistureSensor;
    return &soilMoistureSensor;
}

void DeviceManager::setBME280Device(BME280Device* device) {
    bme280Device = device;
}
