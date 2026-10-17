#include "utils.hpp"
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>

#ifdef __x86_64__
#include <cpuid.h>
#endif

namespace win2lin {
namespace utils {

std::string format_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buf[2048];
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    return std::string(buf);
}

bool is_x86_64_feature_supported() {
#ifdef __x86_64__
    unsigned int eax, ebx, ecx, edx;
    // Check basic features
    if (!__get_cpuid(1, &eax, &ebx, &ecx, &edx)) {
        return false;
    }
    // Require at least SSE/SSE2 etc - most modern CPUs have them
    return true;
#else
    return false;
#endif
}

std::string get_last_error_string() {
    return std::string("not implemented");
}

} // namespace utils
} // namespace win2lin
