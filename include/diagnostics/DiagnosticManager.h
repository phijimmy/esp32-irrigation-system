#ifndef DIAGNOSTIC_MANAGER_H
#define DIAGNOSTIC_MANAGER_H

#include <Arduino.h>
#include "config/ConfigManager.h"
#include "diagnostics/Logger.h"

class DiagnosticManager {
public:
    enum LogLevel {
        LOG_NONE = 0,
        LOG_ERROR = 1,
        LOG_WARN = 2,
        LOG_INFO = 3,
        LOG_DEBUG = 4
    };

    void begin(ConfigManager& configManager);
    void setLogLevel(LogLevel level);
    LogLevel getLogLevel() const;
    void log(LogLevel level, const char* tag, const char* fmt, ...);

private:
    LogLevel logLevel = LOG_INFO;
    ConfigManager* configManager = nullptr;
    Logger logger;
    void loadLogLevelFromConfig();
};

#endif // DIAGNOSTIC_MANAGER_H
