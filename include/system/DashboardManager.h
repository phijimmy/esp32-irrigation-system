#ifndef DASHBOARD_MANAGER_H
#define DASHBOARD_MANAGER_H

#include "system/TimeManager.h"
#include "config/ConfigManager.h"
#include "diagnostics/DiagnosticManager.h"
#include "system/SystemManager.h"
#include "devices/LedDevice.h"
#include "devices/RelayController.h"
#include "devices/TouchSensorDevice.h"
#include <cJSON.h>

class DashboardManager {
public:
    DashboardManager(TimeManager* timeMgr, ConfigManager* configMgr, SystemManager* sysMgr = nullptr, DiagnosticManager* diagMgr = nullptr, LedDevice* ledDev = nullptr, RelayController* relayCtrl = nullptr, TouchSensorDevice* touchDev = nullptr);
    void begin();
    cJSON* getStatusJson(); // Returns a cJSON object with current datetime info
    String getStatusString(); // Returns JSON as string
    void setLedDevice(LedDevice* ledDev);
    void setRelayController(RelayController* relayCtrl);
    void setTouchSensorDevice(TouchSensorDevice* touchDev);
private:
    TimeManager* timeManager;
    ConfigManager* configManager;
    SystemManager* systemManager;
    DiagnosticManager* diagnosticManager;
    LedDevice* ledDevice = nullptr;
    RelayController* relayController = nullptr;
    TouchSensorDevice* touchSensorDevice = nullptr;

    // Helper to add config settings to JSON
    void addConfigSettingsToJson(cJSON* root);
};

#endif // DASHBOARD_MANAGER_H
