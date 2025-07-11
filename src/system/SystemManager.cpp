#include "system/SystemManager.h"
#include "system/I2CManager.h"
#include "devices/BME280Device.h"
#include "system/TimeManager.h"
#include <cJSON.h>

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

cJSON* SystemManager::getSystemInfoJson() {
    cJSON* info = cJSON_CreateObject();
    // Free heap (bytes)
    cJSON_AddNumberToObject(info, "free_heap", ESP.getFreeHeap());
    // Uptime (ms)
    cJSON_AddNumberToObject(info, "uptime_ms", millis());
    // Chip revision
    cJSON_AddNumberToObject(info, "chip_revision", ESP.getChipRevision());
    // CPU frequency (MHz)
    cJSON_AddNumberToObject(info, "cpu_freq_mhz", ESP.getCpuFreqMHz());
    // Flash chip size (bytes)
    cJSON_AddNumberToObject(info, "flash_chip_size", ESP.getFlashChipSize());
    // SDK version
    cJSON_AddStringToObject(info, "sdk_version", ESP.getSdkVersion());
    // Sketch size (bytes)
    cJSON_AddNumberToObject(info, "sketch_size", ESP.getSketchSize());
    // Sketch free space (bytes)
    cJSON_AddNumberToObject(info, "sketch_free_space", ESP.getFreeSketchSpace());
    // Chip ID (MAC address as string)
    cJSON_AddStringToObject(info, "chip_id", String((uint32_t)ESP.getEfuseMac(), HEX).c_str());
    return info;
}
