#include "system/HealthManager.h"

void HealthManager::begin(ConfigManager* config, DiagnosticManager* diag) {
    configManager = config;
    diagnosticManager = diag;
    subsystemCount = 0;
    overallHealth = true;
}

void HealthManager::update() {
    runAllChecks();
}

void HealthManager::runAllChecks() {
    overallHealth = true;
    checkConfig();
    checkFileSystem();
    // ...add more checks as needed
}

void HealthManager::checkConfig() {
    bool ok = configManager != nullptr;
    setSubsystemHealth("Config", ok);
    if (!ok) overallHealth = false;
}

void HealthManager::checkFileSystem() {
    // For demo, just check pointer
    bool ok = configManager != nullptr; // Replace with real FS check
    setSubsystemHealth("FileSystem", ok);
    if (!ok) overallHealth = false;
}

bool HealthManager::isHealthy() const {
    return overallHealth;
}

bool HealthManager::isSubsystemHealthy(const char* name) const {
    for (int i = 0; i < subsystemCount; ++i) {
        if (subsystems[i].name == name) return subsystems[i].healthy;
    }
    return false;
}

void HealthManager::setSubsystemHealth(const char* name, bool healthy) {
    for (int i = 0; i < subsystemCount; ++i) {
        if (subsystems[i].name == name) {
            if (subsystems[i].healthy != healthy) {
                subsystems[i].healthy = healthy;
                if (diagnosticManager) diagnosticManager->log(healthy ? DiagnosticManager::LOG_INFO : DiagnosticManager::LOG_ERROR, "Health", "%s health: %s", name, healthy ? "OK" : "FAIL");
            }
            return;
        }
    }
    if (subsystemCount < MAX_SUBSYSTEMS) {
        subsystems[subsystemCount].name = name;
        subsystems[subsystemCount].healthy = healthy;
        subsystems[subsystemCount].lastLoggedHealthy = !healthy; // force log on first set
        if (diagnosticManager) diagnosticManager->log(healthy ? DiagnosticManager::LOG_INFO : DiagnosticManager::LOG_ERROR, "Health", "%s health: %s", name, healthy ? "OK" : "FAIL");
        ++subsystemCount;
    }
}
