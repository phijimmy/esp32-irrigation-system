#ifndef DASHBOARD_MANAGER_H
#define DASHBOARD_MANAGER_H

#include "system/TimeManager.h"
#include "config/ConfigManager.h"
#include "diagnostics/DiagnosticManager.h"
#include <cJSON.h>

class DashboardManager {
public:
    DashboardManager(TimeManager* timeMgr, ConfigManager* configMgr, DiagnosticManager* diagMgr = nullptr);
    void begin();
    cJSON* getStatusJson(); // Returns a cJSON object with current datetime info
    String getStatusString(); // Returns JSON as string
private:
    TimeManager* timeManager;
    ConfigManager* configManager;
    DiagnosticManager* diagnosticManager;

    // Helper to add config settings to JSON
    void addConfigSettingsToJson(cJSON* root);
};

#endif // DASHBOARD_MANAGER_H
