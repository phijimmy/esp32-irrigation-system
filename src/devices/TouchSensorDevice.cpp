#include "diagnostics/DiagnosticManager.h"
#include "devices/TouchSensorDevice.h"
#include <Arduino.h>

TouchSensorDevice::TouchSensorDevice() : Device(DeviceType::TOUCH_SENSOR) {}

void TouchSensorDevice::setDiagnosticManager(DiagnosticManager* diag) {
    diagnosticManager = diag;
}

void TouchSensorDevice::setConfigManager(ConfigManager* config) {
    configManager = config;
}

void TouchSensorDevice::setGpio(uint8_t gpioNum) {
    gpio = gpioNum;
    if (configManager) configManager->set("touch_gpio", String(gpioNum).c_str());
}

void TouchSensorDevice::setLongPressDuration(uint32_t durationMs) {
    longPressDuration = durationMs;
    if (configManager) configManager->set("touch_long_press", String(durationMs).c_str());
}

void TouchSensorDevice::setThreshold(uint16_t thresholdVal) {
    threshold = thresholdVal;
    if (configManager) configManager->set("touch_threshold", String(thresholdVal).c_str());
}

uint8_t TouchSensorDevice::getGpio() const { return gpio; }
uint32_t TouchSensorDevice::getLongPressDuration() const { return longPressDuration; }
uint16_t TouchSensorDevice::getThreshold() const { return threshold; }

void TouchSensorDevice::begin() {
    if (configManager) {
        cJSON* root = configManager->getRoot();
        cJSON* gpioItem = cJSON_GetObjectItem(root, "touch_gpio");
        cJSON* longPressItem = cJSON_GetObjectItem(root, "touch_long_press");
        cJSON* thresholdItem = cJSON_GetObjectItem(root, "touch_threshold");
        gpio = (gpioItem && cJSON_IsNumber(gpioItem)) ? gpioItem->valueint : 4;
        longPressDuration = (longPressItem && cJSON_IsNumber(longPressItem)) ? longPressItem->valueint : 5000;
        threshold = (thresholdItem && cJSON_IsNumber(thresholdItem)) ? thresholdItem->valueint : 40;
    }
    // No pinMode for touchRead pins
    if (diagnosticManager) diagnosticManager->log(DiagnosticManager::LOG_INFO, "Touch", "Touch sensor initialized on GPIO %d, long press %d ms, threshold %d", gpio, longPressDuration, threshold);
}

void TouchSensorDevice::update() {
    uint16_t value = touchRead(gpio);
    if (diagnosticManager) diagnosticManager->log(DiagnosticManager::LOG_DEBUG, "Touch", "Raw value: %d, threshold: %d", value, threshold);
    handleTouch(value);
}

void TouchSensorDevice::handleTouch(uint16_t value) {
    uint32_t now = millis();
    if (value < threshold) {
        if (!touched) {
            touched = true;
            touchStartTime = now;
            if (diagnosticManager) diagnosticManager->log(DiagnosticManager::LOG_INFO, "Touch", "Touch detected (value: %d, threshold: %d)", value, threshold);
        } else if (!longPressed && (now - touchStartTime >= longPressDuration)) {
            longPressed = true;
            if (diagnosticManager) diagnosticManager->log(DiagnosticManager::LOG_INFO, "Touch", "Long press detected");
        }
    } else {
        if (touched && diagnosticManager) diagnosticManager->log(DiagnosticManager::LOG_INFO, "Touch", "Touch released (value: %d, threshold: %d)", value, threshold);
        touched = false;
        longPressed = false;
    }
}

bool TouchSensorDevice::isTouched() const { return touched; }
bool TouchSensorDevice::isLongPressed() const { return longPressed; }
