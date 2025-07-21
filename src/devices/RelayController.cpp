#include "devices/RelayController.h"
#include <Arduino.h>
#include "system/MqttManager.h"

RelayController::RelayController() : Device(DeviceType::RELAY_CONTROLLER) {
    // Only print essential info
    Serial.println("[RelayController] Constructed");
}

void RelayController::begin() {
    Serial.println("[RelayController] begin() called");
    if (diagnosticManager) {
        diagnosticManager->log(DiagnosticManager::LOG_INFO, "RelayController", "RelayController::begin() called");
    }
    loadConfig();
    // Load relay2_control_gpio from config
    if (configManager) {
        relay2ControlGpio = configManager->getInt("relay2_control_gpio", 18);
    }
    setupRelay2ControlGpio();
    for (int i = 0; i < RELAY_COUNT; ++i) {
        bool activeHigh = true;
        char key[24];
        snprintf(key, sizeof(key), "relay_active_high_%d", i);
        if (configManager) {
            activeHigh = configManager->getBool(key, true);
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
    // Only print essential info
    if (diagnosticManager) {
        diagnosticManager->log(DiagnosticManager::LOG_INFO, "RelayController", "Loading relay GPIO config");
    }
    for (int i = 0; i < RELAY_COUNT; ++i) {
        char key[16];
        snprintf(key, sizeof(key), "relay_gpio_%d", i);
        if (configManager) {
            int gpio = configManager->getInt(key, relayGpios[i]);
            relayGpios[i] = gpio;
        }
    }
}

void RelayController::setupRelay2ControlGpio() {
    pinMode(relay2ControlGpio, INPUT_PULLUP);
}

// Track last state for edge detection
static int lastRelay2ControlState = HIGH;

void RelayController::handleRelay2ControlInterrupt() {
    int state = digitalRead(relay2ControlGpio);
    if (state != lastRelay2ControlState) {
        lastRelay2ControlState = state;
        if (state == LOW) {
            // GPIO 18 LOW: activate relay 2
            activateRelay(2);
        } else {
            // GPIO 18 HIGH: deactivate relay 2
            deactivateRelay(2);
        }
    }
}

void RelayController::update() {
    // Check relay2 control GPIO each update
    handleRelay2ControlInterrupt();
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
    // Publish relay state to MQTT/Home Assistant only if mqttManager is set and initialized
    if (mqttManager && mqttManager->isInitialized()) {
        mqttManager->publishRelayState(index, mode == Relay::Mode::ON);
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
