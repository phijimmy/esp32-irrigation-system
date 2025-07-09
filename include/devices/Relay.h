#ifndef RELAY_H
#define RELAY_H

#include <Arduino.h>
#include "config/ConfigManager.h"
#include "diagnostics/DiagnosticManager.h"

class Relay {
public:
    enum Mode { OFF = 0, ON = 1, TOGGLE = 2 };
    Relay();
    void begin(uint8_t gpio, ConfigManager* config, DiagnosticManager* diag, int relayIndex, bool activeHigh = true);
    void setMode(Mode mode);
    void toggle();
    void setOn();
    void setOff();
    Mode getMode() const;
    bool getState() const;
    uint8_t getGpio() const;
    bool isActiveHigh() const { return activeHigh; }
private:
    uint8_t gpio = 255;
    Mode mode = OFF;
    bool state = false;
    int index = 0;
    bool activeHigh = true;
    ConfigManager* configManager = nullptr;
    DiagnosticManager* diagnosticManager = nullptr;
    void applyMode();
};

#endif // RELAY_H
