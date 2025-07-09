#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include <stdarg.h>

class Logger {
public:
    void log(const char* level, const char* tag, const char* fmt, ...);
};

#endif // LOGGER_H
