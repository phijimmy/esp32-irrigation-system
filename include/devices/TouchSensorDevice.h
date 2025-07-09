#ifndef TOUCH_SENSOR_DEVICE_H
#define TOUCH_SENSOR_DEVICE_H

#include "devices/Device.h"
#include "config/ConfigManager.h"
#include "diagnostics/DiagnosticManager.h"

class TouchSensorDevice : public Device {
public:
    TouchSensorDevice();
    void begin() override;
    void update() override;
    void handleTouch(uint16_t value);
    void setDiagnosticManager(DiagnosticManager* diag);
    void setConfigManager(ConfigManager* config);
    void setGpio(uint8_t gpioNum);
    void setLongPressDuration(uint32_t durationMs);
    void setThreshold(uint16_t thresholdVal);
    bool isTouched() const;
    bool isLongPressed() const;
    uint8_t getGpio() const;
    uint32_t getLongPressDuration() const;
    uint16_t getThreshold() const;

private:
    void handleTouch();
    uint8_t gpio = 4;
    uint32_t longPressDuration = 5000;
    uint16_t threshold = 40;
    uint32_t touchStartTime = 0;
    bool touched = false;
    bool longPressed = false;
    ConfigManager* configManager = nullptr;
    DiagnosticManager* diagnosticManager = nullptr;
};

#endif // TOUCH_SENSOR_DEVICE_H
