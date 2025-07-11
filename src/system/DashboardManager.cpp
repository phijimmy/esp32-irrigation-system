#include "system/DashboardManager.h"
#include "system/SystemManager.h"
#include <Arduino.h>
#include "devices/LedDevice.h"

DashboardManager::DashboardManager(TimeManager* timeMgr, ConfigManager* configMgr, SystemManager* sysMgr, DiagnosticManager* diagMgr, LedDevice* ledDev)
    : timeManager(timeMgr), configManager(configMgr), systemManager(sysMgr), diagnosticManager(diagMgr), ledDevice(ledDev) {}

void DashboardManager::begin() {
    if (diagnosticManager) {
        diagnosticManager->log(DiagnosticManager::LOG_INFO, "DashboardManager", "DashboardManager initialized");
    }
}

void DashboardManager::addConfigSettingsToJson(cJSON* root) {
    if (!configManager) return;
    cJSON* configRoot = configManager->getRoot();
    if (!configRoot) return;
    // Deep copy the entire config tree, including nested objects/arrays
    cJSON* configJson = cJSON_Duplicate(configRoot, 1); // 1 = recursive deep copy
    cJSON_AddItemToObject(root, "config", configJson);
}

void DashboardManager::setLedDevice(LedDevice* ledDev) {
    ledDevice = ledDev;
}

cJSON* DashboardManager::getStatusJson() {
    cJSON* root = cJSON_CreateObject();
    if (!timeManager) return root;
    DateTime now = timeManager->getLocalTime();
    char iso8601[25];
    snprintf(iso8601, sizeof(iso8601), "%04d-%02d-%02dT%02d:%02d:%02d",
        now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
    cJSON_AddStringToObject(root, "datetime_iso", iso8601);
    // Human-readable date
    String dateStr = timeManager->getCurrentDateString();
    cJSON_AddStringToObject(root, "date_human", dateStr.c_str());
    // Human-readable time
    String timeStr = timeManager->getCurrentTimeString();
    cJSON_AddStringToObject(root, "time_human", timeStr.c_str());
    // Add LED state if available
    if (ledDevice) {
        cJSON* ledJson = cJSON_CreateObject();
        cJSON_AddNumberToObject(ledJson, "gpio", ledDevice->getGpio());
        cJSON_AddStringToObject(ledJson, "state", ledDevice->isOn() ? "on" : "off");
        cJSON_AddStringToObject(ledJson, "mode", 
            ledDevice->getMode() == LedDevice::ON ? "on" :
            ledDevice->getMode() == LedDevice::OFF ? "off" :
            ledDevice->getMode() == LedDevice::BLINK ? "blink" :
            ledDevice->getMode() == LedDevice::TOGGLE ? "toggle" : "unknown");
        cJSON_AddNumberToObject(ledJson, "blink_rate", ledDevice->getBlinkRate());
        cJSON_AddItemToObject(root, "led", ledJson);
    }
    // Add config settings
    addConfigSettingsToJson(root);
    // Add system info if available
    if (systemManager) {
        cJSON* sysInfo = systemManager->getSystemInfoJson();
        cJSON_AddItemToObject(root, "system", sysInfo);
        cJSON* fsInfo = systemManager->getFileSystemInfoJson();
        cJSON_AddItemToObject(root, "filesystem", fsInfo);
        cJSON* healthInfo = systemManager->getHealthJson();
        cJSON_AddItemToObject(root, "health", healthInfo);
        cJSON* i2cInfo = systemManager->getI2CInfoJson();
        cJSON_AddItemToObject(root, "i2c", i2cInfo);
    }
    return root;
}

String DashboardManager::getStatusString() {
    cJSON* obj = getStatusJson();
    char* jsonStr = cJSON_PrintUnformatted(obj);
    String result(jsonStr);
    cJSON_free(jsonStr);
    cJSON_Delete(obj);
    return result;
}
