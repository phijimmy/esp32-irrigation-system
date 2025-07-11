#ifndef DASHBOARD_MANAGER_H
#define DASHBOARD_MANAGER_H

#include "system/TimeManager.h"
#include "config/ConfigManager.h"
#include "diagnostics/DiagnosticManager.h"
#include "system/SystemManager.h"
#include "devices/LedDevice.h"
#include "devices/RelayController.h"
#include "devices/TouchSensorDevice.h"
#include "devices/BME280Device.h"
#include "devices/SoilMoistureSensor.h"
#include <cJSON.h>

class DashboardManager {
public:
    enum State {
        UNINITIALIZED,
        INITIALIZED,
        ERROR,
        UPDATING
    };
    static const char* stateToString(State s) {
        switch (s) {
            case UNINITIALIZED: return "uninitialized";
            case INITIALIZED: return "initialized";
            case ERROR: return "error";
            case UPDATING: return "updating";
            default: return "unknown";
        }
    }

    DashboardManager(TimeManager* timeMgr, ConfigManager* configMgr, SystemManager* sysMgr = nullptr, DiagnosticManager* diagMgr = nullptr, LedDevice* ledDev = nullptr, RelayController* relayCtrl = nullptr, TouchSensorDevice* touchDev = nullptr, BME280Device* bme280Dev = nullptr);
    void begin();
    cJSON* getStatusJson(); // Returns a cJSON object with current datetime info
    String getStatusString(); // Returns JSON as string
    void setLedDevice(LedDevice* ledDev);
    void setRelayController(RelayController* relayCtrl);
    void setTouchSensorDevice(TouchSensorDevice* touchDev);
    void setBME280Device(BME280Device* bme280Dev);
    void setSoilMoistureSensor(SoilMoistureSensor* soilSensor);
private:
    TimeManager* timeManager;
    ConfigManager* configManager;
    SystemManager* systemManager;
    DiagnosticManager* diagnosticManager;
    LedDevice* ledDevice = nullptr;
    RelayController* relayController = nullptr;
    TouchSensorDevice* touchSensorDevice = nullptr;
    BME280Device* bme280Device = nullptr;
    SoilMoistureSensor* soilMoistureSensor = nullptr;
    State state = UNINITIALIZED;

    // Helper to add config settings to JSON
    void addConfigSettingsToJson(cJSON* root);
};

#endif // DASHBOARD_MANAGER_H
