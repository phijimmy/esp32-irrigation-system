#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include <Arduino.h>
#include "diagnostics/DiagnosticManager.h"
#include "config/ConfigManager.h"

class PowerManager {
public:
    enum PowerMode {
        NORMAL = 0,
        LOW_POWER = 1
    };
    void begin(ConfigManager* config, DiagnosticManager* diag);
    void update();
    void setPowerMode(PowerMode mode);
    PowerMode getPowerMode() const;
    float getSupplyVoltage() const;
    bool isBrownout() const;
    int getCpuSpeedMhz() const;
    int getMaxCpuSpeedMhz() const;
    void stepDownCpuSpeed();
    void stepUpCpuSpeed();
private:
    DiagnosticManager* diagnosticManager = nullptr;
    ConfigManager* configManager = nullptr;
    PowerMode currentMode = NORMAL;
    float lastVoltage = 3.3f;
    bool brownout = false;
    const int voltagePin = 34;
    const float voltageDividerRatio = 2.0f; // Divider halves the voltage
    const float adcMax = 4095.0f;
    const float vRef = 3.3f;
    float brownoutThreshold = 2.5f; // Configurable if needed
    int maxCpuSpeedMhz = 160;
    int currentCpuSpeedMhz = 160;
    void checkVoltage();
    void applyCpuSpeed(int mhz);
};

#endif // POWER_MANAGER_H
