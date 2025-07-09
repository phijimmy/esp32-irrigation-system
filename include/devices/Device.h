#ifndef DEVICE_H
#define DEVICE_H

#include <Arduino.h>

enum class DeviceType {
    UNKNOWN,
    BME280,
    SOIL_MOISTURE_SENSOR,
    MQ135_SENSOR,
    LED,
    RELAY,
    RELAY_CONTROLLER,
    TOUCH_SENSOR
};

class Device {
public:
    Device(DeviceType type) : deviceType(type) {}
    virtual void begin() = 0;
    virtual void update() = 0;
    DeviceType getDeviceType() const { return deviceType; }

private:
    DeviceType deviceType;
};

#endif // DEVICE_H
