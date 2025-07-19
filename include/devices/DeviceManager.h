#ifndef DEVICE_MANAGER_H
#define DEVICE_MANAGER_H

#include <Arduino.h>

#include "devices/Device.h"
#include "config/ConfigManager.h"
#include "devices/SoilMoistureSensor.h"

class Device;
class BME280Device;

class DeviceManager {
public:
    void begin(ConfigManager& configManager);
    void update();
    void addDevice(Device* device);
    
    // Getter methods for specific device types
    BME280Device* getBME280Device();
    SoilMoistureSensor* getSoilMoistureSensor();
    
    // Setter methods for sensor devices (these are managed separately)
    void setBME280Device(BME280Device* device);
private:
    static const int MAX_DEVICES = 8;
    Device* devices[MAX_DEVICES];
    int deviceCount = 0;
    ConfigManager* configManager = nullptr;
    
    // Direct references to sensor devices
    BME280Device* bme280Device = nullptr;
};

#endif // DEVICE_MANAGER_H
