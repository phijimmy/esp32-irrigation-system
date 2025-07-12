#include "system/NetworkManager.h"
#include "system/TimeManager.h"
#include <WiFi.h>
#include <cJSON.h>

void NetworkManager::begin(ConfigManager* config, DiagnosticManager* diag, PowerManager* power) {
    configManager = config;
    diagnosticManager = diag;
    powerManager = power;
    
    // Load WiFi configuration
    wifiMode = configManager->get("wifi_mode");
    wifiSsid = configManager->get("wifi_ssid");
    wifiPassword = configManager->get("wifi_pass");
    apSsid = configManager->get("ap_ssid");
    apPassword = configManager->get("ap_password");
    const char* timeoutStr = configManager->get("ap_timeout");
    apTimeout = timeoutStr && *timeoutStr ? atol(timeoutStr) : 1800;
    
    // Load reconnection settings from config
    const char* reconnectIntervalStr = configManager->get("wifi_reconnect_interval");
    reconnectInterval = reconnectIntervalStr && *reconnectIntervalStr ? atol(reconnectIntervalStr) : 60;
    const char* maxAttemptsStr = configManager->get("wifi_max_reconnect_attempts");
    maxReconnectAttempts = maxAttemptsStr && *maxAttemptsStr ? atoi(maxAttemptsStr) : 5;
    
    if (diagnosticManager) {
        diagnosticManager->log(DiagnosticManager::LOG_INFO, "Network", "WiFi mode: %s, reconnect interval: %lus, max attempts: %d", 
                              wifiMode.c_str(), reconnectInterval, maxReconnectAttempts);
    }
    
    // Set up WiFi event handlers for automatic reconnection
    static NetworkManager* self = this;
    WiFi.onEvent([](arduino_event_t* event) {
        if (self && event) {
            switch (event->event_id) {
                case ARDUINO_EVENT_WIFI_STA_CONNECTED:
                    if (self->diagnosticManager) {
                        self->diagnosticManager->log(DiagnosticManager::LOG_INFO, "Network", "WiFi station connected");
                    }
                    break;
                case ARDUINO_EVENT_WIFI_STA_GOT_IP:
                    self->wasConnected = true;
                    self->reconnectAttempts = 0; // Reset reconnect attempts on successful connection
                    if (self->diagnosticManager) {
                        self->diagnosticManager->log(DiagnosticManager::LOG_INFO, "Network", "WiFi got IP: %s", WiFi.localIP().toString().c_str());
                    }
                    // Trigger initial NTP sync when WiFi connects
                    if (self->timeManager) {
                        self->timeManager->onWiFiConnected();
                    }
                    break;
                case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
                    if (self->diagnosticManager) {
                        self->diagnosticManager->log(DiagnosticManager::LOG_WARN, "Network", "WiFi station disconnected");
                    }
                    self->handleWiFiDisconnection();
                    break;
                default:
                    break;
            }
        }
    });
    
    // Start network based on configured mode
    if (wifiMode == "client" && wifiSsid.length() > 0) {
        startWiFiClient();
    } else {
        if (diagnosticManager) {
            diagnosticManager->log(DiagnosticManager::LOG_INFO, "Network", "Starting in Access Point mode (mode=%s, ssid_len=%d)", wifiMode.c_str(), wifiSsid.length());
        }
        startAP();
    }
}

void NetworkManager::startAP() {
    if (powerManager) powerManager->setPowerMode(PowerManager::LOW_POWER);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(apSsid.c_str(), apPassword.c_str());
    static NetworkManager* self = this;
    WiFi.onEvent([](arduino_event_t* event) {
        if (event && event->event_id == ARDUINO_EVENT_WIFI_AP_STACONNECTED && self && self->diagnosticManager) {
            const uint8_t* raw = (const uint8_t*)&event->event_info.wifi_sta_connected;
            char macStr[18];
            snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
                raw[0], raw[1], raw[2], raw[3], raw[4], raw[5]);
            int aid = raw[6] | (raw[7] << 8); // little-endian
            self->diagnosticManager->log(DiagnosticManager::LOG_INFO, "Network", "Client connected: %s, AID=%d", macStr, aid);
        }
    });
    apActive = true;
    lastClientTime = millis();
    if (diagnosticManager) diagnosticManager->log(DiagnosticManager::LOG_INFO, "Network", "Access Point started: %s, timeout: %lus", apSsid.c_str(), apTimeout);
    if (powerManager) powerManager->setPowerMode(PowerManager::NORMAL);
}

void NetworkManager::update() {
    // Handle Access Point mode
    if (apActive) {
        int clients = getConnectedClients();
        if (clients > 0) {
            lastClientTime = millis();
        } else {
            unsigned long elapsed = (millis() - lastClientTime) / 1000;
            if (elapsed >= apTimeout) {
                shutdownAP();
            }
        }
    }
    
    // Handle WiFi Client connection attempts and reconnection
    if (wifiMode == "client" && wifiSsid.length() > 0 && !apActive) {
        if (wifiConnecting) {
            unsigned long elapsed = (millis() - wifiConnectStart) / 1000;
            if (WiFi.status() == WL_CONNECTED) {
                wifiConnecting = false;
                if (diagnosticManager) {
                    diagnosticManager->log(DiagnosticManager::LOG_INFO, "Network", "WiFi connected! IP: %s", WiFi.localIP().toString().c_str());
                }
                if (powerManager) powerManager->setPowerMode(PowerManager::NORMAL);
            } else if (elapsed >= wifiConnectTimeout) {
                wifiConnecting = false;
                reconnectAttempts++;
                if (diagnosticManager) {
                    diagnosticManager->log(DiagnosticManager::LOG_WARN, "Network", "WiFi connection timeout (attempt %d/%d)", 
                                          reconnectAttempts, maxReconnectAttempts);
                }
                
                if (reconnectAttempts >= maxReconnectAttempts) {
                    if (diagnosticManager) {
                        diagnosticManager->log(DiagnosticManager::LOG_WARN, "Network", "Max reconnection attempts reached, falling back to AP mode");
                    }
                    startAP();
                } else {
                    lastReconnectAttempt = millis();
                }
            }
        } else {
            // Check if we need to attempt reconnection
            if (WiFi.status() != WL_CONNECTED && reconnectEnabled && 
                reconnectAttempts < maxReconnectAttempts &&
                (millis() - lastReconnectAttempt) >= (reconnectInterval * 1000)) {
                attemptReconnect();
            }
        }
    }
}

void NetworkManager::shutdownAP() {
    if (apActive) {
        WiFi.softAPdisconnect(true);
        apActive = false;
        if (diagnosticManager) diagnosticManager->log(DiagnosticManager::LOG_INFO, "Network", "Access Point shutdown after timeout");
    }
}

bool NetworkManager::isAPActive() const {
    return apActive;
}

bool NetworkManager::isWiFiConnected() const {
    return WiFi.status() == WL_CONNECTED && !apActive;
}

void NetworkManager::startWiFiClient() {
    if (powerManager) powerManager->setPowerMode(PowerManager::LOW_POWER);
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifiSsid.c_str(), wifiPassword.c_str());
    
    wifiConnecting = true;
    wifiConnectStart = millis();
    
    if (diagnosticManager) {
        diagnosticManager->log(DiagnosticManager::LOG_INFO, "Network", "Connecting to WiFi: %s", wifiSsid.c_str());
    }
}

int NetworkManager::getConnectedClients() {
    return WiFi.softAPgetStationNum();
}

void NetworkManager::handleWiFiDisconnection() {
    if (wasConnected && !apActive) {
        connectionLostTime = millis();
        wasConnected = false;
        if (diagnosticManager) {
            diagnosticManager->log(DiagnosticManager::LOG_WARN, "Network", "WiFi connection lost, will attempt reconnection");
        }
        
        // Start reconnection attempts if enabled
        if (reconnectEnabled && reconnectAttempts < maxReconnectAttempts) {
            lastReconnectAttempt = millis();
        }
    }
}

void NetworkManager::attemptReconnect() {
    if (wifiMode == "client" && wifiSsid.length() > 0 && !apActive && !wifiConnecting) {
        reconnectAttempts++;
        if (diagnosticManager) {
            diagnosticManager->log(DiagnosticManager::LOG_INFO, "Network", "Attempting WiFi reconnection (%d/%d)", 
                                  reconnectAttempts, maxReconnectAttempts);
        }
        
        // Disconnect and reconnect
        WiFi.disconnect(true);
        delay(1000);
        WiFi.mode(WIFI_STA);
        WiFi.begin(wifiSsid.c_str(), wifiPassword.c_str());
        
        wifiConnecting = true;
        wifiConnectStart = millis();
        lastReconnectAttempt = millis();
        
        if (powerManager) powerManager->setPowerMode(PowerManager::LOW_POWER);
    }
}

void NetworkManager::forceReconnect() {
    if (wifiMode == "client" && wifiSsid.length() > 0) {
        if (diagnosticManager) {
            diagnosticManager->log(DiagnosticManager::LOG_INFO, "Network", "Forcing WiFi reconnection");
        }
        
        // Reset reconnection state
        reconnectAttempts = 0;
        wifiConnecting = false;
        
        // If we're in AP mode, shut it down first
        if (apActive) {
            shutdownAP();
        }
        
        // Force reconnection
        attemptReconnect();
    }
}

cJSON* NetworkManager::getNetworkInfoJson() const {
    cJSON* net = cJSON_CreateObject();
    cJSON_AddStringToObject(net, "mode", wifiMode.c_str());
    cJSON_AddStringToObject(net, "ssid", wifiMode == "ap" ? apSsid.c_str() : wifiSsid.c_str());
    cJSON_AddStringToObject(net, "ip", WiFi.isConnected() ? WiFi.localIP().toString().c_str() : "0.0.0.0");
    cJSON_AddStringToObject(net, "mac", WiFi.macAddress().c_str());
    cJSON_AddBoolToObject(net, "connected", WiFi.isConnected());
    cJSON_AddBoolToObject(net, "ap_active", apActive);
    cJSON_AddNumberToObject(net, "ap_clients", apActive ? WiFi.softAPgetStationNum() : 0);
    return net;
}
