#include "devices/RelayController.h"
#include <Arduino.h>

RelayController::RelayController() : Device(DeviceType::RELAY_CONTROLLER) {
    // Print to Serial directly to bypass any diagnostic manager issues
    Serial.println("[DIRECT] RelayController constructor called");
    Serial.printf("[DIRECT] Initial relayGpios: [%d, %d, %d, %d]\n", relayGpios[0], relayGpios[1], relayGpios[2], relayGpios[3]);
}

void RelayController::begin() {
    // Print to Serial directly to bypass any diagnostic manager issues
    Serial.println("[DIRECT] RelayController::begin() called");
    Serial.printf("[DIRECT] configManager=%p, diagnosticManager=%p\n", (void*)configManager, (void*)diagnosticManager);
    
    // Use configManager and diagnosticManager already set via setConfigManager/setDiagnosticManager if needed
    if (diagnosticManager) {
        diagnosticManager->log(DiagnosticManager::LOG_ERROR, "RelayController", ">>> RelayController::begin() ENTRY POINT <<<");
        diagnosticManager->log(DiagnosticManager::LOG_INFO, "RelayController", "RelayController::begin() called, configManager=%p", (void*)configManager);
    }
    loadConfig();
    Serial.printf("[DIRECT] After loadConfig, relayGpios: [%d, %d, %d, %d]\n", relayGpios[0], relayGpios[1], relayGpios[2], relayGpios[3]);
    for (int i = 0; i < RELAY_COUNT; ++i) {
        bool activeHigh = true;
        char key[24];
        snprintf(key, sizeof(key), "relay_active_high_%d", i);
        if (configManager) {
            activeHigh = configManager->getBool(key, true); // Default to true (active high)
        }
        relays[i].begin(relayGpios[i], configManager, diagnosticManager, i, activeHigh);
        if (diagnosticManager) {
            diagnosticManager->log(DiagnosticManager::LOG_INFO, "RelayController", "Relay %d initialized on GPIO %d (active %s)", i, relayGpios[i], activeHigh ? "HIGH" : "LOW");
        }
    }
}

void RelayController::begin(ConfigManager* config, DiagnosticManager* diag) {
    configManager = config;
    diagnosticManager = diag;
    begin();
}

void RelayController::loadConfig() {
    Serial.println("[DIRECT] RelayController::loadConfig() called");
    Serial.printf("[DIRECT] Default relayGpios before config load: [%d, %d, %d, %d]\n", relayGpios[0], relayGpios[1], relayGpios[2], relayGpios[3]);
    
    if (diagnosticManager) {
        diagnosticManager->log(DiagnosticManager::LOG_ERROR, "RelayController", ">>> loadConfig() ENTRY POINT <<<");
        diagnosticManager->log(DiagnosticManager::LOG_ERROR, "RelayController", "Default relayGpios before config load: [%d, %d, %d, %d]", relayGpios[0], relayGpios[1], relayGpios[2], relayGpios[3]);
    }
    for (int i = 0; i < RELAY_COUNT; ++i) {
        char key[16];
        snprintf(key, sizeof(key), "relay_gpio_%d", i);
        if (configManager) {
            int gpio = configManager->getInt(key, relayGpios[i]); // Use default if not found
            Serial.printf("[DIRECT] Config lookup for %s: %d (default was %d)\n", key, gpio, relayGpios[i]);
            if (diagnosticManager) {
                diagnosticManager->log(DiagnosticManager::LOG_ERROR, "RelayController", "Before assignment: relayGpios[%d]=%d, config value=%d", i, relayGpios[i], gpio);
            }
            relayGpios[i] = gpio;
            if (diagnosticManager) {
                diagnosticManager->log(DiagnosticManager::LOG_ERROR, "RelayController", "After assignment: relayGpios[%d]=%d", i, relayGpios[i]);
            }
        } else {
            if (diagnosticManager) {
                diagnosticManager->log(DiagnosticManager::LOG_WARN, "RelayController", "No configManager, using default GPIO %d for relay %d", relayGpios[i], i);
            }
        }
    }
    if (diagnosticManager) {
        diagnosticManager->log(DiagnosticManager::LOG_ERROR, "RelayController", "Final relayGpios after config load: [%d, %d, %d, %d]", relayGpios[0], relayGpios[1], relayGpios[2], relayGpios[3]);
    }
    Serial.printf("[DIRECT] Final relayGpios after config load: [%d, %d, %d, %d]\n", relayGpios[0], relayGpios[1], relayGpios[2], relayGpios[3]);
}

void RelayController::update() {
    // No periodic logic for relays by default
}

Relay* RelayController::getRelay(int index) {
    if (index < 0 || index >= RELAY_COUNT) return nullptr;
    return &relays[index];
}

void RelayController::setRelayMode(int index, Relay::Mode mode) {
    if (index < 0 || index >= RELAY_COUNT) return;
    Serial.printf("[DIRECT] setRelayMode called: relay %d, GPIO=%d, mode=%d\n", index, relays[index].getGpio(), (int)mode);
    if (relays[index].getGpio() > 40 || relays[index].getGpio() == 0) {
        Serial.printf("[DIRECT] ERROR: Relay %d has invalid GPIO %d! Not initialized properly!\n", index, relays[index].getGpio());
    }
    relays[index].setMode(mode);
    if (diagnosticManager) {
        diagnosticManager->log(DiagnosticManager::LOG_INFO, "RelayController", "Relay %d set to %s", index, mode == Relay::Mode::ON ? "ON" : (mode == Relay::Mode::OFF ? "OFF" : "TOGGLE"));
    }
}

void RelayController::activateRelay(int index) {
    setRelayMode(index, Relay::Mode::ON);
}

void RelayController::deactivateRelay(int index) {
    setRelayMode(index, Relay::Mode::OFF);
}

void RelayController::toggleRelay(int index) {
    if (index < 0 || index >= RELAY_COUNT) return;
    relays[index].toggle();
    if (diagnosticManager) {
        diagnosticManager->log(DiagnosticManager::LOG_INFO, "RelayController", "Relay %d toggled to %s", index, relays[index].getState() ? "ON" : "OFF");
    }
}

bool RelayController::getRelayState(int index) const {
    if (index < 0 || index >= RELAY_COUNT) return false;
    return relays[index].getState();
}

uint8_t RelayController::getRelayGpio(int index) const {
    if (index < 0 || index >= RELAY_COUNT) return 0;
    return relayGpios[index];
}
