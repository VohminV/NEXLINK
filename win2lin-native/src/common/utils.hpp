#pragma once

#include <cstddef>
#include <string>

namespace win2lin {
namespace utils {

std::string format_error(const char* fmt, ...);
bool is_x86_64_feature_supported();
std::string get_last_error_string();

} // namespace utils
} // namespace win2lin
