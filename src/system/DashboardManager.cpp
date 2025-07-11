#include "system/DashboardManager.h"
#include <Arduino.h>

DashboardManager::DashboardManager(TimeManager* timeMgr, ConfigManager* configMgr, DiagnosticManager* diagMgr)
    : timeManager(timeMgr), configManager(configMgr), diagnosticManager(diagMgr) {}

void DashboardManager::begin() {
    if (diagnosticManager) {
        diagnosticManager->log(DiagnosticManager::LOG_INFO, "DashboardManager", "DashboardManager initialized");
    }
}

void DashboardManager::addConfigSettingsToJson(cJSON* root) {
    if (!configManager) return;
    cJSON* configRoot = configManager->getRoot();
    if (!configRoot) return;
    cJSON* configJson = cJSON_CreateObject();
    cJSON* item = nullptr;
    cJSON_ArrayForEach(item, configRoot) {
        // Only add primitive types (number, string, bool) for dashboard
        if (cJSON_IsNumber(item)) {
            cJSON_AddNumberToObject(configJson, item->string, item->valuedouble);
        } else if (cJSON_IsString(item)) {
            cJSON_AddStringToObject(configJson, item->string, item->valuestring);
        } else if (cJSON_IsBool(item)) {
            cJSON_AddBoolToObject(configJson, item->string, cJSON_IsTrue(item));
        }
        // For objects/arrays, you could add a summary or skip for now
    }
    cJSON_AddItemToObject(root, "config", configJson);
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
    // Add config settings
    addConfigSettingsToJson(root);
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
