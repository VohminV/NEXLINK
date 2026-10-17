#pragma once

#include <cstdint>

namespace win2lin {
namespace abi {

// Windows type definitions for ABI compatibility
using DWORD = uint32_t;
using BOOL = int32_t;
using BYTE = uint8_t;
using WORD = uint16_t;
using UINT = uint32_t;
using LONG = int32_t;
using ULONG = uint32_t;
using LONGLONG = int64_t;
using ULONGLONG = uint64_t;
using HANDLE = void*;
using LPVOID = void*;
using LPCSTR = const char*;
using LPCWSTR = const wchar_t*;
using LPSTR = char*;
using LPWSTR = wchar_t*;

#define WINAPI __attribute__((stdcall))

} // namespace abi
} // namespace win2lin
