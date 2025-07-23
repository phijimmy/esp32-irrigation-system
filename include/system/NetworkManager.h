#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <Arduino.h>
#include "config/ConfigManager.h"
#include "diagnostics/DiagnosticManager.h"
#include "system/PowerManager.h"

class TimeManager; // Forward declaration

class NetworkManager {
public:

    cJSON* getNetworkInfoJson() const;
public:
    void begin(ConfigManager* config, DiagnosticManager* diag, PowerManager* power);
    void setTimeManager(TimeManager* timeMgr) { timeManager = timeMgr; }
    void update();
    void shutdownAP();
    bool isAPActive() const;
    bool isWiFiConnected() const;
    void startWiFiClient();
    void forceReconnect(); // Force WiFi reconnection
    void startAP();
private:
    ConfigManager* configManager = nullptr;
    DiagnosticManager* diagnosticManager = nullptr;
    PowerManager* powerManager = nullptr;
    TimeManager* timeManager = nullptr;
    String apSsid;
    String apPassword;
    String wifiSsid;
    String wifiPassword;
    String wifiMode; // "ap" or "client"
    unsigned long apTimeout = 1800; // seconds
    unsigned long lastClientTime = 0;
    unsigned long wifiConnectTimeout = 30; // seconds
    unsigned long wifiConnectStart = 0;
    unsigned long reconnectInterval = 60; // seconds between reconnection attempts
    unsigned long lastReconnectAttempt = 0;
    unsigned long connectionLostTime = 0;
    bool apActive = false;
    bool wifiConnecting = false;
    bool wasConnected = false; // Track previous connection state
    bool reconnectEnabled = true; // Enable automatic reconnection
    int reconnectAttempts = 0;
    int maxReconnectAttempts = 5; // Max attempts before falling back to AP
    int getConnectedClients();
    void handleWiFiDisconnection();
    void attemptReconnect();
};

#endif // NETWORK_MANAGER_H
