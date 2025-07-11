#ifndef WEBSERVER_MANAGER_H
#define WEBSERVER_MANAGER_H

#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include "system/DashboardManager.h"
#include "diagnostics/DiagnosticManager.h"

class WebServerManager {
public:
    WebServerManager(DashboardManager* dashMgr, DiagnosticManager* diagMgr);
    void begin();
    void handleClient();
    bool isRunning() const { return running; }

private:
    AsyncWebServer* server;
    DashboardManager* dashboardManager;
    DiagnosticManager* diagnosticManager;
    bool running = false;
    
    // Route handlers
    void handleRoot(AsyncWebServerRequest* request);
    void handleAPI(AsyncWebServerRequest* request);
    void handleNotFound(AsyncWebServerRequest* request);
    void handleStaticFile(AsyncWebServerRequest* request);
    
    // Helper methods
    void initializeLittleFS();
};

#endif // WEBSERVER_MANAGER_H
