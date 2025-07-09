#include "devices/Relay.h"

Relay::Relay() {}

void Relay::begin(uint8_t gpioNum, ConfigManager* config, DiagnosticManager* diag, int relayIndex, bool activeHighParam) {
    gpio = gpioNum;
    configManager = config;
    diagnosticManager = diag;
    index = relayIndex;
    activeHigh = activeHighParam;
    if (diagnosticManager) diagnosticManager->log(DiagnosticManager::LOG_INFO, "Relay", "Relay %d begin() called with GPIO %d (active %s)", index, gpio, activeHigh ? "HIGH" : "LOW");
    pinMode(gpio, OUTPUT);
    setOff();
    if (diagnosticManager) diagnosticManager->log(DiagnosticManager::LOG_INFO, "Relay", "Relay %d initialized on GPIO %d (active %s)", index, gpio, activeHigh ? "HIGH" : "LOW");
}

void Relay::setMode(Mode m) {
    mode = m;
    applyMode();
}

void Relay::toggle() {
    setMode(mode == ON ? OFF : ON);
}

void Relay::setOn() {
    setMode(ON);
}

void Relay::setOff() {
    setMode(OFF);
}

Relay::Mode Relay::getMode() const { return mode; }
bool Relay::getState() const { return state; }
uint8_t Relay::getGpio() const { return gpio; }

void Relay::applyMode() {
    Serial.printf("[DIRECT] Relay %d applyMode: gpio=%d, mode=%d, activeHigh=%s\n", index, gpio, (int)mode, activeHigh ? "true" : "false");
    if (gpio > 40) {
        Serial.printf("[DIRECT] ERROR: Relay %d trying to use invalid GPIO %d!\n", index, gpio);
        return; // Don't attempt to use invalid GPIO
    }
    if (diagnosticManager) diagnosticManager->log(DiagnosticManager::LOG_DEBUG, "Relay", "Relay %d applyMode() called: mode=%d, gpio=%d, activeHigh=%s", index, (int)mode, gpio, activeHigh ? "true" : "false");
    
    switch (mode) {
        case ON:
            if (diagnosticManager) diagnosticManager->log(DiagnosticManager::LOG_DEBUG, "Relay", "Relay %d: Setting GPIO %d to %s", index, gpio, activeHigh ? "HIGH" : "LOW");
            digitalWrite(gpio, activeHigh ? HIGH : LOW);
            state = true;
            break;
        case OFF:
            if (diagnosticManager) diagnosticManager->log(DiagnosticManager::LOG_DEBUG, "Relay", "Relay %d: Setting GPIO %d to %s", index, gpio, activeHigh ? "LOW" : "HIGH");
            digitalWrite(gpio, activeHigh ? LOW : HIGH);
            state = false;
            break;
        case TOGGLE:
            toggle();
            break;
    }
    if (diagnosticManager) diagnosticManager->log(DiagnosticManager::LOG_DEBUG, "Relay", "Relay %d set to %s", index, state ? "ON" : "OFF");
}
