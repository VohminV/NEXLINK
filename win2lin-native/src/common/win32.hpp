#pragma once

// Minimal Windows ABI type definitions and calling-convention helpers.
//
// Why this file exists: a relinked PE binary calls its imports using the
// Microsoft x64 calling convention (args in RCX/RDX/R8/R9, 32-byte shadow
// space, non-volatile RBX/RBP/RDI/RSI/R12-R15). On x86-64 Linux, GCC/Clang
// default to the System V ABI (args in RDI/RSI/RDX/RCX/R8/R9 and no shadow
// space). A shim compiled with the default ABI would read garbage arguments.
//
// GCC supports __attribute__((ms_abi)) on x86-64, which makes the compiler
// emit MS-ABI prologues/epilogues for the annotated function. Every function
// exported by a libwin_*.so MUST use WINAPI so the callee matches the caller.

#include <cstddef>
#include <cstdint>

#if defined(_WIN32)
#define WINAPI __stdcall
#elif defined(__x86_64__)
// Microsoft x64 calling convention.
#define WINAPI __attribute__((ms_abi))
#else
#define WINAPI
#endif

namespace win2lin::win {

using BOOL = int32_t;
using BYTE = uint8_t;
using WORD = uint16_t;
using DWORD = uint32_t;
using QWORD = uint64_t;
using LONG = int32_t;
using ULONG = uint32_t;
using LONGLONG = int64_t;
using ULONGLONG = uint64_t;
using UINT = uint32_t;
using WCHAR = char16_t; // UTF-16 code unit (Windows wchar_t is 16-bit)
using SIZE_T = uint64_t;

using HANDLE = void*;
using HMODULE = void*;
using HINSTANCE = void*;
using HWND = void*;
using HDC = void*;
using HGLRC = void*;
using LPVOID = void*;
using LPCVOID = const void*;
using LPSTR = char*;
using LPCSTR = const char*;
using LPWSTR = WCHAR*;
using LPCWSTR = const WCHAR*;
using LPDWORD = DWORD*;

using THREAD_START_ROUTINE = DWORD(WINAPI*)(LPVOID);

// Sentinel handles for the standard streams.
constexpr DWORD STD_INPUT_HANDLE = static_cast<DWORD>(-10);
constexpr DWORD STD_OUTPUT_HANDLE = static_cast<DWORD>(-11);
constexpr DWORD STD_ERROR_HANDLE = static_cast<DWORD>(-12);

// Wait results.
constexpr DWORD WAIT_OBJECT_0 = 0;
constexpr DWORD WAIT_TIMEOUT = 258;
constexpr DWORD WAIT_FAILED = 0xFFFFFFFF;

// VirtualAlloc flags.
constexpr DWORD MEM_COMMIT = 0x1000;
constexpr DWORD MEM_RESERVE = 0x2000;
constexpr DWORD MEM_RELEASE = 0x8000;
constexpr DWORD MEM_DECOMMIT = 0x4000;
constexpr DWORD MEM_RESET = 0x80000;
constexpr DWORD MEM_FREE = 0x10000;
constexpr DWORD MEM_PRIVATE = 0x20000;
constexpr DWORD PAGE_NOACCESS = 0x01;
constexpr DWORD PAGE_READONLY = 0x02;
constexpr DWORD PAGE_READWRITE = 0x04;
constexpr DWORD PAGE_WRITECOPY = 0x08;
constexpr DWORD PAGE_EXECUTE = 0x10;
constexpr DWORD PAGE_EXECUTE_READ = 0x20;
constexpr DWORD PAGE_EXECUTE_READWRITE = 0x40;
constexpr DWORD PAGE_EXECUTE_WRITECOPY = 0x80;
constexpr DWORD PAGE_GUARD = 0x100;
constexpr DWORD PAGE_NOCACHE = 0x200;

// File access / creation.
constexpr DWORD GENERIC_READ = 0x80000000;
constexpr DWORD GENERIC_WRITE = 0x40000000;
constexpr DWORD FILE_SHARE_READ = 0x00000001;
constexpr DWORD FILE_SHARE_WRITE = 0x00000002;
constexpr DWORD CREATE_NEW = 1;
constexpr DWORD CREATE_ALWAYS = 2;
constexpr DWORD OPEN_EXISTING = 3;
constexpr DWORD OPEN_ALWAYS = 4;
constexpr DWORD TRUNCATE_EXISTING = 5;
constexpr DWORD INVALID_SET_FILE_POINTER = 0xFFFFFFFF;

// Error codes (subset).
constexpr DWORD ERROR_SUCCESS = 0;
constexpr DWORD ERROR_FILE_NOT_FOUND = 2;
constexpr DWORD ERROR_PATH_NOT_FOUND = 3;
constexpr DWORD ERROR_ACCESS_DENIED = 5;
constexpr DWORD ERROR_INVALID_HANDLE = 6;
constexpr DWORD ERROR_INVALID_PARAMETER = 87;
constexpr DWORD ERROR_CALL_NOT_IMPLEMENTED = 120;

} // namespace win2lin::win

// Mark a function as exported from a shim shared library.
#define WIN2LIN_EXPORT extern "C" __attribute__((visibility("default")))

// Fail loudly on unimplemented entry points. We deliberately do NOT throw a
// C++ exception across the PE boundary: the PE was not compiled with the
// Itanium unwinder and letting it unwind through foreign frames would corrupt
// state. Abort with a diagnostic instead ("never silently fail").
#define WIN2LIN_UNIMPL(name)                                                   \
    do {                                                                       \
        fprintf(stderr,                                                        \
                "[win2lin] FATAL: unimplemented shim entry point: %s\n", name);\
        abort();                                                               \
    } while (0)
