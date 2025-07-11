#ifndef LED_DEVICE_H
#define LED_DEVICE_H

#include "Device.h"
#include "config/ConfigManager.h"

class LedDevice : public Device {
public:
    enum Mode { OFF, ON, TOGGLE, BLINK };

    LedDevice();
    void begin() override;
    void update() override;
    void setMode(Mode mode);
    void setBlinkRate(uint32_t rateMs);
    void setGpio(uint8_t gpioNum);
    void setDiagnosticManager(DiagnosticManager* diag);
    Mode getMode() const;
    uint32_t getBlinkRate() const;
    uint8_t getGpio() const;
    bool isOn() const { return ledState; }

private:
    void applyMode();
    void blink();
    Mode mode = OFF;
    uint8_t gpio = 23;
    uint32_t blinkRate = 500;
    uint32_t lastToggle = 0;
    bool ledState = false;
    ConfigManager* configManager = nullptr;
    DiagnosticManager* diagnosticManager = nullptr;
};

#endif // LED_DEVICE_H
