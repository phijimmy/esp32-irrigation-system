#ifndef SYSTEM_MANAGER_H
#define SYSTEM_MANAGER_H

#include <Arduino.h>
#include <cJSON.h>
#include "filesystem/FileSystemManager.h"
#include "config/ConfigManager.h"
#include "devices/DeviceManager.h"
#include "diagnostics/DiagnosticManager.h"
#include "system/HealthManager.h"
#include "system/PowerManager.h"
#include "system/NetworkManager.h"
#include "system/I2CManager.h"
#include "system/TimeManager.h"
#include "system/ADS1115Manager.h"


class SystemManager {
public:
    void begin();
    void update();
    void restart();
    bool isHealthy();
    FileSystemManager& getFileSystemManager();
    ConfigManager& getConfigManager();
    DeviceManager& getDeviceManager();
    DiagnosticManager& getDiagnosticManager();
    HealthManager& getHealthManager();
    PowerManager& getPowerManager();
    NetworkManager& getNetworkManager();
    I2CManager& getI2CManager();
    TimeManager& getTimeManager();
    ADS1115Manager& getADS1115Manager();
    cJSON* getSystemInfoJson(); // Returns system info as a cJSON object

private:
    void initHardware();
    void checkSystemHealth();
    bool healthy = true;
    FileSystemManager fileSystemManager;
    ConfigManager configManager;
    DeviceManager deviceManager;
    DiagnosticManager diagnosticManager;
    HealthManager healthManager;
    PowerManager powerManager;
    NetworkManager networkManager;
    I2CManager i2cManager;
    TimeManager timeManager;
    ADS1115Manager ads1115Manager;
};

#endif // SYSTEM_MANAGER_H
