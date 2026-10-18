// libwin_msvcp71/80/90.so - stub shims for Microsoft C++ runtime (msvcp*.dll).
// MinGW-linked games often depend on these; stub the commonly called symbols.

#include "common/logging.hpp"

#include <cstdint>
#include <cstddef>

#define WINAPI __attribute__((ms_abi))

using HRESULT = int32_t;
using DWORD = uint32_t;
using UINT = uint32_t;
using SIZE_T = uint64_t;
using intptr_t = int64_t;
using uintptr_t = uint64_t;
using LPVOID = void*;
using LPCVOID = const void*;

namespace win2lin {

#define WINAPI __attribute__((ms_abi))

// Common C++ standard library stubs
// RTTI / typeinfo stubs
extern "C" WINAPI void* __gxx_personality_v0(void* a, void* b, void* c, void* d, void* e) { return nullptr; }
extern "C" WINAPI void __cxa_throw(void* a, void* b, void* c) { (void)a; (void)b; (void)c; }
extern "C" WINAPI void* __cxa_allocate_exception(SIZE_T size) { (void)size; return nullptr; }
extern "C" WINAPI void __cxa_free_exception(void* ptr) { (void)ptr; }
extern "C" WINAPI void* __cxa_begin_catch(void* ptr) { (void)ptr; return nullptr; }
extern "C" WINAPI void __cxa_end_catch(void) {}

// String / stream stubs (commonly referenced)
extern "C" WINAPI int strcmp(const char* a, const char* b) { (void)a; (void)b; return 0; }
extern "C" WINAPI int strcasecmp(const char* a, const char* b) { (void)a; (void)b; return 0; }
extern "C" WINAPI int strncmp(const char* a, const char* b, SIZE_T n) { (void)a; (void)b; (void)n; return 0; }
extern "C" WINAPI SIZE_T strlen(const char* s) { (void)s; return 0; }
extern "C" WINAPI char* strcpy(char* d, const char* s) { (void)d; (void)s; return d; }
extern "C" WINAPI char* strncpy(char* d, const char* s, SIZE_T n) { (void)d; (void)s; (void)n; return d; }
extern "C" WINAPI char* strcat(char* d, const char* s) { (void)d; (void)s; return d; }
extern "C" WINAPI char* strchr(const char* s, int c) { (void)s; (void)c; return nullptr; }
extern "C" WINAPI char* strstr(const char* a, const char* b) { (void)a; (void)b; return nullptr; }
extern "C" WINAPI void* memset(void* d, int c, SIZE_T n) { (void)d; (void)c; (void)n; return d; }
extern "C" WINAPI void* memcpy(void* d, const void* s, SIZE_T n) { (void)d; (void)s; (void)n; return d; }
extern "C" WINAPI void* memmove(void* d, const void* s, SIZE_T n) { (void)d; (void)s; (void)n; return d; }
extern "C" WINAPI int memcmp(const void* a, const void* b, SIZE_T n) { (void)a; (void)b; (void)n; return 0; }

} // namespace win2lin
