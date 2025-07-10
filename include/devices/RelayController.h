#ifndef RELAY_CONTROLLER_H
#define RELAY_CONTROLLER_H

#include "devices/Device.h"
#include "devices/Relay.h"
#include "config/ConfigManager.h"
#include "diagnostics/DiagnosticManager.h"

class RelayController : public Device {
public:
    RelayController();
    void begin() override; // For DeviceManager compatibility
    void begin(ConfigManager* config, DiagnosticManager* diag);
    void update() override;
    Relay* getRelay(int index);
    void setRelayMode(int index, Relay::Mode mode);
    void activateRelay(int index); // Turn relay ON
    void deactivateRelay(int index); // Turn relay OFF
    void toggleRelay(int index);
    bool getRelayState(int index) const;
    uint8_t getRelayGpio(int index) const;
    void setConfigManager(ConfigManager* config) { configManager = config; }
    void setDiagnosticManager(DiagnosticManager* diag) { diagnosticManager = diag; }
    void handleRelay2ControlInterrupt(); // New: handle GPIO 18 interrupt
    void setupRelay2ControlGpio(); // New: setup GPIO 18 as input with pullup
private:
    static const int RELAY_COUNT = 4;
    Relay relays[RELAY_COUNT];
    uint8_t relayGpios[RELAY_COUNT] = {32, 33, 25, 26};
    ConfigManager* configManager = nullptr;
    DiagnosticManager* diagnosticManager = nullptr;
    void loadConfig();
    int relay2ControlGpio = 18; // Default, will be loaded from config
};

#endif // RELAY_CONTROLLER_H
