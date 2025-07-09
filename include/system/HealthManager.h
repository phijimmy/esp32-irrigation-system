#ifndef HEALTH_MANAGER_H
#define HEALTH_MANAGER_H

#include <Arduino.h>
#include "diagnostics/DiagnosticManager.h"
#include "config/ConfigManager.h"

class HealthManager {
public:
    void begin(ConfigManager* config, DiagnosticManager* diag);
    void update();
    bool isHealthy() const;
    bool isSubsystemHealthy(const char* name) const;
    void setSubsystemHealth(const char* name, bool healthy);
    void runAllChecks();
private:
    DiagnosticManager* diagnosticManager = nullptr;
    ConfigManager* configManager = nullptr;
    bool overallHealth = true;
    struct SubsystemHealth {
        String name;
        bool healthy;
        bool lastLoggedHealthy;
    };
    static const int MAX_SUBSYSTEMS = 8;
    SubsystemHealth subsystems[MAX_SUBSYSTEMS];
    int subsystemCount = 0;
    void checkConfig();
    void checkFileSystem();
    // Add more checks as needed
};

#endif // HEALTH_MANAGER_H
