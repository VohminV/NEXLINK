#pragma once

#include <string>

namespace win2lin {
namespace logging {

enum class Level {
    Trace,
    Debug,
    Info,
    Warn,
    Error
};

void set_level(Level level);
void log(Level level, const char* file, int line, const char* fmt, ...);

#define LOG_TRACE(...) log(::win2lin::logging::Level::Trace, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_DEBUG(...) log(::win2lin::logging::Level::Debug, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...)  log(::win2lin::logging::Level::Info,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN(...)  log(::win2lin::logging::Level::Warn,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...) log(::win2lin::logging::Level::Error, __FILE__, __LINE__, __VA_ARGS__)

} // namespace logging
} // namespace win2lin
