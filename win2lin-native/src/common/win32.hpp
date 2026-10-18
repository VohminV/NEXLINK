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
using ATOM = uint16_t;
using HRESULT = int32_t;
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
using HICON = void*;
using HCURSOR = void*;
using HBRUSH = void*;
using HMENU = void*;
using LPVOID = void*;
using LPCVOID = const void*;
using PVOID = void*;
using LPSTR = char*;
using LPCSTR = const char*;
using LPWSTR = WCHAR*;
using LPCWSTR = const WCHAR*;
using LPDWORD = DWORD*;
using LPBYTE = BYTE*;

using WPARAM = uint64_t;
using LPARAM = int64_t;
using LRESULT = int64_t;
using UINT_PTR = uint64_t;
using INT_PTR = int64_t;
using LONG_PTR = int64_t;
using ULONG_PTR = uint64_t;
using DWORD_PTR = uint64_t;

struct POINT {
    LONG x;
    LONG y;
};

struct RECT {
    LONG left;
    LONG top;
    LONG right;
    LONG bottom;
};

struct MSG {
    HWND hwnd;
    UINT message;
    WPARAM wParam;
    LPARAM lParam;
    DWORD time;
    POINT pt;
};

using WNDPROC = LRESULT(WINAPI*)(HWND, UINT, WPARAM, LPARAM);

struct WNDCLASSEXW {
    UINT cbSize;
    UINT style;
    WNDPROC lpfnWndProc;
    int cbClsExtra;
    int cbWndExtra;
    HINSTANCE hInstance;
    HICON hIcon;
    HCURSOR hCursor;
    HBRUSH hbrBackground;
    LPCWSTR lpszMenuName;
    LPCWSTR lpszClassName;
    HICON hIconSm;
};

struct STARTUPINFOW {
    DWORD cb;
    LPWSTR lpReserved;
    LPWSTR lpDesktop;
    LPWSTR lpTitle;
    DWORD dwX;
    DWORD dwY;
    DWORD dwXSize;
    DWORD dwYSize;
    DWORD dwXCountChars;
    DWORD dwYCountChars;
    DWORD dwFillAttribute;
    DWORD dwFlags;
    WORD wShowWindow;
    WORD cbReserved2;
    LPBYTE lpReserved2;
    HANDLE hStdInput;
    HANDLE hStdOutput;
    HANDLE hStdError;
};

struct MEMORY_BASIC_INFORMATION {
    PVOID BaseAddress;
    PVOID AllocationBase;
    DWORD AllocationProtect;
    WORD PartitionId;
    SIZE_T RegionSize;
    DWORD State;
    DWORD Protect;
    DWORD Type;
};

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

// BOOL
constexpr int TRUE = 1;
constexpr int FALSE = 0;

// File type / attributes
constexpr DWORD FILE_TYPE_UNKNOWN = 0x0000;
constexpr DWORD FILE_TYPE_DISK = 0x0001;
constexpr DWORD INVALID_FILE_ATTRIBUTES = 0xFFFFFFFF;

// Code pages
constexpr UINT CP_UTF8 = 65001;
constexpr UINT CP_ACP = 0;

// Invalid handle
const HANDLE INVALID_HANDLE_VALUE = (HANDLE)(void*)-1;

// CPINFO structure (for GetCPInfo)
struct CPINFO {
    UINT MaxCharSize;
    BYTE DefaultChar[2];
    BYTE LeadByte[12];
};
using LPCPINFO = CPINFO*;

using LPWORD = WORD*;
using LARGE_INTEGER = int64_t;
using PLONG = LONG*;

using LCID = DWORD;
using HRSRC = void*;
using HGLOBAL = void*;

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
