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
    // Only print error if invalid GPIO
    if (gpio > 40) {
        Serial.printf("[Relay] ERROR: Relay %d trying to use invalid GPIO %d!\n", index, gpio);
        return;
    }
    if (diagnosticManager) diagnosticManager->log(DiagnosticManager::LOG_DEBUG, "Relay", "Relay %d applyMode() called: mode=%d, gpio=%d, activeHigh=%s", index, (int)mode, gpio, activeHigh ? "true" : "false");
    switch (mode) {
        case ON:
            digitalWrite(gpio, activeHigh ? HIGH : LOW);
            state = true;
            break;
        case OFF:
            digitalWrite(gpio, activeHigh ? LOW : HIGH);
            state = false;
            break;
        case TOGGLE:
            toggle();
            break;
    }
    if (diagnosticManager) diagnosticManager->log(DiagnosticManager::LOG_DEBUG, "Relay", "Relay %d set to %s", index, state ? "ON" : "OFF");
}
