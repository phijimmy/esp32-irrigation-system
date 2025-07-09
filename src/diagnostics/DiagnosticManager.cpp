#include "diagnostics/DiagnosticManager.h"
#include <stdarg.h>

void DiagnosticManager::begin(ConfigManager& cfgMgr) {
    configManager = &cfgMgr;
    loadLogLevelFromConfig();
}

void DiagnosticManager::setLogLevel(LogLevel level) {
    logLevel = level;
    if (configManager) configManager->set("debug_level", String((int)level).c_str());
}

DiagnosticManager::LogLevel DiagnosticManager::getLogLevel() const {
    return logLevel;
}

void DiagnosticManager::loadLogLevelFromConfig() {
    if (!configManager) return;
    const char* lvl = configManager->get("debug_level");
    int val = lvl && *lvl ? atoi(lvl) : (int)LOG_INFO;
    logLevel = (LogLevel)val;
}

void DiagnosticManager::log(LogLevel level, const char* tag, const char* fmt, ...) {
    if (level > logLevel || level == LOG_NONE) return;
    static const char* levelStr[] = {"NONE", "ERROR", "WARN", "INFO", "DEBUG"};
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    logger.log(levelStr[level], tag, "%s", buf);
}
