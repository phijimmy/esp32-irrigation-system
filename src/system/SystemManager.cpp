#include "system/SystemManager.h"
#include "system/I2CManager.h"
#include "devices/BME280Device.h"
#include "system/TimeManager.h"

void SystemManager::begin() {
    initHardware();
    fileSystemManager.begin(&diagnosticManager);
    configManager.begin(fileSystemManager, &diagnosticManager);
    diagnosticManager.begin(configManager);
    deviceManager.begin(configManager);
    healthManager.begin(&configManager, &diagnosticManager);
    powerManager.begin(&configManager, &diagnosticManager);
    networkManager.begin(&configManager, &diagnosticManager, &powerManager);
    i2cManager.begin(&configManager, &diagnosticManager);
    timeManager.begin(&i2cManager, &configManager, &diagnosticManager);
    // Connect TimeManager to NetworkManager for NTP sync on WiFi connect
    networkManager.setTimeManager(&timeManager);
    i2cManager.autoRegisterBME280s(&timeManager, &deviceManager);
    ads1115Manager.begin(&i2cManager);
    healthy = true;
}

void SystemManager::update() {
    powerManager.update();
    healthManager.update();
    networkManager.update();
    timeManager.update();
    checkSystemHealth();
    deviceManager.update();
}

void SystemManager::restart() {
    ESP.restart();
}

bool SystemManager::isHealthy() {
    return healthy;
}

FileSystemManager& SystemManager::getFileSystemManager() {
    return fileSystemManager;
}

ConfigManager& SystemManager::getConfigManager() {
    return configManager;
}

DeviceManager& SystemManager::getDeviceManager() {
    return deviceManager;
}

DiagnosticManager& SystemManager::getDiagnosticManager() {
    return diagnosticManager;
}

HealthManager& SystemManager::getHealthManager() {
    return healthManager;
}

PowerManager& SystemManager::getPowerManager() {
    return powerManager;
}

NetworkManager& SystemManager::getNetworkManager() {
    return networkManager;
}

I2CManager& SystemManager::getI2CManager() {
    return i2cManager;
}

TimeManager& SystemManager::getTimeManager() {
    return timeManager;
}

ADS1115Manager& SystemManager::getADS1115Manager() {
    return ads1115Manager;
}

void SystemManager::initHardware() {
    Serial.begin(115200);
    while (!Serial) { delay(1); } // Wait for Serial to be ready (prevents garbled output)
    delay(100); // Give time for serial port to stabilize
    diagnosticManager.log(DiagnosticManager::LOG_INFO, "System", "SystemManager: Hardware initialized");
}

void SystemManager::checkSystemHealth() {
    healthy = healthManager.isHealthy();
}
