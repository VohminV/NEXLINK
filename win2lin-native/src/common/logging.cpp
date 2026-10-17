#include "logging.hpp"
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <iostream>

namespace win2lin {
namespace logging {

static Level current_level = Level::Info;

void set_level(Level level) {
    current_level = level;
}

static const char* level_to_str(Level level) {
    switch (level) {
        case Level::Trace: return "TRACE";
        case Level::Debug: return "DEBUG";
        case Level::Info:  return "INFO";
        case Level::Warn:  return "WARN";
        case Level::Error: return "ERROR";
        default: return "UNKNOWN";
    }
}

void log(Level level, const char* file, int line, const char* fmt, ...) {
    if (level < current_level) return;

    va_list args;
    va_start(args, fmt);

    std::time_t t = std::time(nullptr);
    char timebuf[32];
    std::strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));

    fprintf(stderr, "[%s] [%s] %s:%d: ", timebuf, level_to_str(level), file, line);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");

    va_end(args);
}

} // namespace logging
} // namespace win2lin
