#include "system/PowerManager.h"
#include <esp_system.h>

void PowerManager::begin(ConfigManager* config, DiagnosticManager* diag) {
    configManager = config;
    diagnosticManager = diag;
    pinMode(voltagePin, INPUT);
    // Always get brownoutThreshold from config manager (central config)
    const char* thresholdStr = configManager ? configManager->get("brownout_threshold") : nullptr;
    if (thresholdStr && strlen(thresholdStr) > 0) {
        brownoutThreshold = atof(thresholdStr);
    } else {
        brownoutThreshold = 2.5f; // fallback default
        if (diagnosticManager) diagnosticManager->log(DiagnosticManager::LOG_WARN, "Power", "No brownout_threshold in config, using fallback: %.2f V", brownoutThreshold);
    }
    if (diagnosticManager) diagnosticManager->log(DiagnosticManager::LOG_INFO, "Power", "Brownout threshold in use: %.2f V", brownoutThreshold);
    // Load max CPU speed from config
    const char* cpuStr = configManager ? configManager->get("cpu_speed") : nullptr;
    int cpuCfg = cpuStr && strlen(cpuStr) > 0 ? atoi(cpuStr) : 160;
    if (cpuCfg == 80 || cpuCfg == 160 || cpuCfg == 240) {
        maxCpuSpeedMhz = cpuCfg;
    } else {
        maxCpuSpeedMhz = 160;
    }
    currentCpuSpeedMhz = maxCpuSpeedMhz;
    applyCpuSpeed(currentCpuSpeedMhz);
    currentMode = NORMAL;
    brownout = false;
}

void PowerManager::update() {
    checkVoltage();
    // Add more power management logic if needed
}

void PowerManager::setPowerMode(PowerMode mode) {
    if (currentMode != mode) {
        currentMode = mode;
        if (diagnosticManager) diagnosticManager->log(DiagnosticManager::LOG_INFO, "Power", "Power mode set to %s", mode == NORMAL ? "NORMAL" : "LOW_POWER");
        if (mode == LOW_POWER) {
            stepDownCpuSpeed();
        } else {
            stepUpCpuSpeed();
        }
    }
}

PowerManager::PowerMode PowerManager::getPowerMode() const {
    return currentMode;
}

float PowerManager::getSupplyVoltage() const {
    return lastVoltage;
}

bool PowerManager::isBrownout() const {
    return brownout;
}

int PowerManager::getCpuSpeedMhz() const {
    return currentCpuSpeedMhz;
}

int PowerManager::getMaxCpuSpeedMhz() const {
    return maxCpuSpeedMhz;
}

void PowerManager::stepDownCpuSpeed() {
    int newSpeed = currentCpuSpeedMhz;
    if (currentCpuSpeedMhz > 160 && maxCpuSpeedMhz >= 160) {
        newSpeed = 160;
    } else if (currentCpuSpeedMhz > 80 && maxCpuSpeedMhz >= 80) {
        newSpeed = 80;
    }
    if (newSpeed != currentCpuSpeedMhz) {
        applyCpuSpeed(newSpeed);
        if (diagnosticManager) diagnosticManager->log(DiagnosticManager::LOG_INFO, "Power", "CPU speed stepped down to %d MHz", newSpeed);
    }
}

void PowerManager::stepUpCpuSpeed() {
    int target = maxCpuSpeedMhz;
    if (currentCpuSpeedMhz < target) {
        applyCpuSpeed(target);
        if (diagnosticManager) diagnosticManager->log(DiagnosticManager::LOG_INFO, "Power", "CPU speed stepped up to %d MHz", target);
    }
}

void PowerManager::applyCpuSpeed(int mhz) {
    if (mhz == 80 || mhz == 160 || mhz == 240) {
        setCpuFrequencyMhz(mhz);
        currentCpuSpeedMhz = mhz;
    }
}

void PowerManager::checkVoltage() {
    int raw = analogRead(voltagePin);
    float actual = (raw / adcMax) * vRef; // Voltage at GPIO 34
    float estimated_supply = actual * voltageDividerRatio; // Estimate supply voltage
    lastVoltage = estimated_supply;
    bool wasBrownout = brownout;
    brownout = (estimated_supply < brownoutThreshold);
    if (brownout && !wasBrownout && diagnosticManager) {
        diagnosticManager->log(DiagnosticManager::LOG_WARN, "Power", "Brownout detected! Supply voltage: %.2f V (GPIO34: %.2f V)", estimated_supply, actual);
    } else if (!brownout && wasBrownout && diagnosticManager) {
        diagnosticManager->log(DiagnosticManager::LOG_INFO, "Power", "Voltage recovered: %.2f V (GPIO34: %.2f V)", estimated_supply, actual);
    }
}
