#include "devices/LedDevice.h"
#include "diagnostics/DiagnosticManager.h"
#include <Arduino.h>

LedDevice::LedDevice() : Device(DeviceType::LED) {
    mode = OFF;
    ledState = false;
}

void LedDevice::begin() {
    // Use config or default to GPIO 23, active-high logic
    if (configManager) {
        cJSON* root = configManager->getRoot();
        cJSON* gpioItem = cJSON_GetObjectItem(root, "led_gpio");
        cJSON* blinkItem = cJSON_GetObjectItem(root, "led_blink_rate");
        if (gpioItem) {
            if (cJSON_IsNumber(gpioItem)) {
                gpio = gpioItem->valueint;
            } else if (cJSON_IsString(gpioItem) && strlen(gpioItem->valuestring) > 0) {
                gpio = atoi(gpioItem->valuestring);
            } else {
                gpio = 23;
            }
        } else {
            gpio = 23;
        }
        if (blinkItem) {
            if (cJSON_IsNumber(blinkItem)) {
                blinkRate = blinkItem->valueint;
            } else if (cJSON_IsString(blinkItem) && strlen(blinkItem->valuestring) > 0) {
                blinkRate = atoi(blinkItem->valuestring);
            } else {
                blinkRate = 500;
            }
        } else {
            blinkRate = 500;
        }
    } else {
        gpio = 23;
        blinkRate = 500;
    }
    pinMode(gpio, OUTPUT);
    mode = OFF;
    ledState = false;
    digitalWrite(gpio, LOW); // Set LOW to turn LED OFF for active-high wiring
    if (diagnosticManager) diagnosticManager->log(DiagnosticManager::LOG_INFO, "LED", "LED initialized on GPIO %d, blink rate %d", gpio, blinkRate);
    setMode(OFF); // Set LED OFF after initialization
}

void LedDevice::update() {
    if (mode == BLINK) {
        blink();
    }
}

void LedDevice::setMode(Mode m) {
    mode = m;
    if (mode == OFF) {
        ledState = false;
        digitalWrite(gpio, LOW); // Set LOW to turn LED OFF for active-high wiring
        Serial.printf("[LED] digitalWrite(%d, LOW) (OFF)\n", gpio);
    } else {
        applyMode();
    }
    int pinState = digitalRead(gpio);
    Serial.printf("[LED] Mode set to %d, pinState=%d (LOW=off, HIGH=on)\n", mode, pinState);
    if (diagnosticManager) diagnosticManager->log(DiagnosticManager::LOG_DEBUG, "LED", "LED mode set to %d, pinState=%d (LOW=off, HIGH=on)", mode, pinState);
}

void LedDevice::setBlinkRate(uint32_t rateMs) {
    blinkRate = rateMs;
    if (configManager) configManager->set("led_blink_rate", String(rateMs).c_str());
}

void LedDevice::setGpio(uint8_t gpioNum) {
    gpio = gpioNum;
    if (configManager) configManager->set("led_gpio", String(gpioNum).c_str());
    pinMode(gpio, OUTPUT);
}

LedDevice::Mode LedDevice::getMode() const { return mode; }
uint32_t LedDevice::getBlinkRate() const { return blinkRate; }
uint8_t LedDevice::getGpio() const { return gpio; }

void LedDevice::applyMode() {
    switch (mode) {
        case ON:
            ledState = true;
            digitalWrite(gpio, HIGH); // Set HIGH to turn LED ON for active-high wiring
            Serial.printf("[LED] digitalWrite(%d, HIGH) (ON)\n", gpio);
            break;
        case OFF:
            ledState = false;
            digitalWrite(gpio, LOW); // Set LOW to turn LED OFF for active-high wiring
            Serial.printf("[LED] digitalWrite(%d, LOW) (OFF)\n", gpio);
            break;
        case TOGGLE:
            ledState = !ledState;
            digitalWrite(gpio, ledState ? HIGH : LOW); // HIGH=ON, LOW=OFF
            Serial.printf("[LED] digitalWrite(%d, %s) (TOGGLE)\n", gpio, ledState ? "HIGH" : "LOW");
            break;
        case BLINK:
            // handled in update()
            break;
    }
}

void LedDevice::blink() {
    uint32_t now = millis();
    if (now - lastToggle >= blinkRate) {
        lastToggle = now;
        ledState = !ledState;
        digitalWrite(gpio, ledState ? HIGH : LOW); // HIGH=ON, LOW=OFF
        Serial.printf("[LED] digitalWrite(%d, %s) (BLINK)\n", gpio, ledState ? "HIGH" : "LOW");
    }
}

void LedDevice::setDiagnosticManager(DiagnosticManager* diag) {
    diagnosticManager = diag;
}
