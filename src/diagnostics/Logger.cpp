#include "diagnostics/Logger.h"

void Logger::log(const char* level, const char* tag, const char* fmt, ...) {
    Serial.printf("[%s] %s: ", level, tag);
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    Serial.print(buf);
    Serial.println();
}
