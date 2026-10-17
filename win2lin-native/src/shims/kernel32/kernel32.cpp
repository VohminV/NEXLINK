// libwin_kernel32.so - native POSIX implementations of the subset of
// kernel32.dll needed by relinked Windows binaries.
//
// Every exported function uses the Microsoft x64 calling convention (WINAPI)
// and is `extern "C"` so the ELF dynamic linker can resolve the undefined
// symbols baked into the relinked binary by name.
//
// Mapping rationale (Windows API -> Linux primitive):
//   VirtualAlloc  -> mmap(MAP_PRIVATE|MAP_ANONYMOUS), 64 KiB granularity
//   VirtualFree   -> munmap (size recovered from an allocation registry)
//   HeapAlloc     -> malloc / calloc  (Windows heap -> libc heap)
//   CreateThread  -> pthread_create
//   CreateFileW   -> open(2) after UTF-16 -> UTF-8 conversion
//   ReadFile      -> read(2)
//   WriteFile     -> write(2)
//   GetTickCount  -> clock_gettime(CLOCK_MONOTONIC)
//   GetLastError  -> thread_local DWORD, set by failing calls

#ifndef _GNU_SOURCE
#define _GNU_SOURCE // pthread_tryjoin_np, MAP_FIXED_NOREPLACE
#endif

#include "common/win32.hpp"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>

#include <dlfcn.h>
#include <fcntl.h>
#include <malloc.h>
#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <functional>
#include <map>
#include <mutex>
#include <string>

using namespace win2lin::win;

namespace {

// --------------------------------------------------------------------------
// Error state. GetLastError() is per-thread on Windows, so mirror that with
// a thread_local. Every shim entry point that can fail calls set_last_error().
// --------------------------------------------------------------------------
// initial-exec TLS: avoids __tls_get_addr (a SysV call that clobbers RDI/RSI,
// which are callee-saved under the ms_abi our callers use).
thread_local DWORD t_last_error __attribute__((tls_model("initial-exec"))) = ERROR_SUCCESS;

void set_last_error(DWORD e) { t_last_error = e; }

DWORD errno_to_win(int e) {
    switch (e) {
        case ENOENT: return ERROR_FILE_NOT_FOUND;
        case ENOTDIR: return ERROR_PATH_NOT_FOUND;
        case EACCES:
        case EPERM: return ERROR_ACCESS_DENIED;
        case EBADF: return ERROR_INVALID_HANDLE;
        case EINVAL: return ERROR_INVALID_PARAMETER;
        case ENOMEM: return 8; // ERROR_NOT_ENOUGH_MEMORY
        default: return static_cast<DWORD>(e);
    }
}

// --------------------------------------------------------------------------
// Handle table. Windows HANDLEs are opaque; we map them to a small struct so
// ReadFile/WriteFile/CloseHandle can dispatch to the right backend.
// --------------------------------------------------------------------------
struct Handle {
    enum class Kind { File, Thread } kind = Kind::File;
    int fd = -1;
    pthread_t thread = 0;
    bool joinable = false;
};

std::mutex g_handle_mutex;
std::map<HANDLE, Handle*> g_handles;
uintptr_t g_next_handle = 0x1000;

HANDLE alloc_handle(Handle h) {
    std::lock_guard<std::mutex> lk(g_handle_mutex);
    auto* hp = new Handle(h);
    const HANDLE key = reinterpret_cast<HANDLE>(g_next_handle += 0x10);
    g_handles[key] = hp;
    return key;
}

Handle* lookup_handle(HANDLE key) {
    std::lock_guard<std::mutex> lk(g_handle_mutex);
    auto it = g_handles.find(key);
    return it == g_handles.end() ? nullptr : it->second;
}

HANDLE std_handle(DWORD id) {
    if (id == STD_INPUT_HANDLE)  return reinterpret_cast<HANDLE>(static_cast<intptr_t>(0));
    if (id == STD_OUTPUT_HANDLE) return reinterpret_cast<HANDLE>(static_cast<intptr_t>(1));
    if (id == STD_ERROR_HANDLE)  return reinterpret_cast<HANDLE>(static_cast<intptr_t>(2));
    return nullptr;
}

int handle_to_fd(HANDLE handle) {
    const intptr_t as_int = reinterpret_cast<intptr_t>(handle);
    if (as_int >= 0 && as_int <= 2) return static_cast<int>(as_int); // std streams
    if (Handle* h = lookup_handle(handle)) return h->fd;
    return -1;
}

// --------------------------------------------------------------------------
// VirtualAlloc bookkeeping. MEM_RELEASE requires the original reservation
// base and munmap needs the length, so remember every live reservation.
// --------------------------------------------------------------------------
struct Reservation {
    uint64_t size;
    DWORD protect;
};
std::mutex g_valloc_mutex;
std::map<void*, Reservation> g_reservations;

int prot_from_win(DWORD protect) {
    switch (protect & 0xFF) {
        case PAGE_NOACCESS: return PROT_NONE;
        case PAGE_READONLY: return PROT_READ;
        case PAGE_READWRITE:
        case PAGE_WRITECOPY: return PROT_READ | PROT_WRITE;
        case PAGE_EXECUTE: return PROT_EXEC;
        case PAGE_EXECUTE_READ: return PROT_READ | PROT_EXEC;
        case PAGE_EXECUTE_READWRITE:
        case PAGE_EXECUTE_WRITECOPY: return PROT_READ | PROT_WRITE | PROT_EXEC;
        default: return PROT_READ | PROT_WRITE;
    }
}

// --------------------------------------------------------------------------
// UTF-16 -> UTF-8 (handles surrogate pairs).
// --------------------------------------------------------------------------
std::string utf16_to_utf8(const WCHAR* w) {
    std::string out;
    if (!w) return out;
    for (size_t i = 0; w[i] != 0; ++i) {
        uint32_t cp = w[i];
        if (cp >= 0xD800 && cp <= 0xDBFF && w[i + 1] >= 0xDC00 && w[i + 1] <= 0xDFFF) {
            cp = 0x10000 + ((cp - 0xD800) << 10) + (w[i + 1] - 0xDC00);
            ++i;
        }
        if (cp < 0x80) {
            out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }
    return out;
}

std::u16string latin1_to_utf16(const char* s) {
    std::u16string w;
    for (; s && *s; ++s) w.push_back(static_cast<char16_t>(static_cast<unsigned char>(*s)));
    return w;
}

// --------------------------------------------------------------------------
// DLL stem -> shim stem aliasing. The relinker applies the same mapping at
// static-link time; runtime LoadLibrary/GetModuleHandle must agree so
// delay-loaded CRT/api-set DLLs resolve to the shims that actually exist.
// --------------------------------------------------------------------------
std::string shim_stem_for_dll(std::string stem) {
    for (char& c : stem) c = static_cast<char>(tolower((unsigned char)c));
    // Universal CRT + vcruntime + MSVC C++ runtime all fold into the one
    // msvcrt shim (Phase 1.5 covers the shared bootstrap surface).
    if (stem == "msvcrt" || stem == "msvcrt_os" || stem == "ucrtbase" ||
        stem == "ucrtbase_app" || stem.rfind("vcruntime", 0) == 0 ||
        stem.rfind("msvcr", 0) == 0 || stem.rfind("msvcp", 0) == 0 ||
        stem.rfind("api-ms-win-crt-", 0) == 0) {
        return "msvcrt";
    }
    // api-set forwarders for core Win32 APIs land in kernel32.
    if (stem.rfind("api-ms-win-core-", 0) == 0 || stem.rfind("api-ms-win-dev-", 0) == 0) {
        return "kernel32";
    }
    return stem;
}

// Splits "path\foo.dll" -> lowercased stem, strips directories and extension.
std::string dll_stem_of(LPCSTR name) {
    std::string n(name ? name : "");
    for (char& c : n) c = static_cast<char>(tolower((unsigned char)c));
    const auto slash = n.find_last_of("/\\");
    if (slash != std::string::npos) n = n.substr(slash + 1);
    const auto dot = n.find_last_of('.');
    return (dot == std::string::npos) ? n : n.substr(0, dot);
}

// dlopen shim for a Windows DLL name with aliasing; nullptr on failure.
void* dlopen_shim_for_dll(LPCSTR name) {
    if (!name) return nullptr;
    const std::string stem = shim_stem_for_dll(dll_stem_of(name));
    const std::string soname = "libwin_" + stem + ".so";
    void* h = dlopen(soname.c_str(), RTLD_NOW | RTLD_GLOBAL);
    if (!h) h = dlopen(dll_stem_of(name).c_str(), RTLD_NOW | RTLD_GLOBAL);
    return h;
}

} // namespace

// ==========================================================================
// Error handling
// ==========================================================================
WIN2LIN_EXPORT DWORD WINAPI GetLastError(void) { return t_last_error; }
WIN2LIN_EXPORT void WINAPI SetLastError(DWORD e) { t_last_error = e; }

// ==========================================================================
// Process / module
// ==========================================================================
WIN2LIN_EXPORT void WINAPI ExitProcess(UINT code) {
    fflush(nullptr);
    std::exit(static_cast<int>(code));
}

WIN2LIN_EXPORT void WINAPI TerminateProcess(HANDLE, UINT code) {
    fflush(nullptr);
    std::exit(static_cast<int>(code));
}

WIN2LIN_EXPORT HANDLE WINAPI GetCurrentProcess(void) {
    return reinterpret_cast<HANDLE>(static_cast<intptr_t>(-1));
}
WIN2LIN_EXPORT DWORD WINAPI GetCurrentProcessId(void) { return ::getpid(); }
WIN2LIN_EXPORT HANDLE WINAPI GetCurrentThread(void) {
    return reinterpret_cast<HANDLE>(pthread_self());
}

WIN2LIN_EXPORT HMODULE WINAPI GetModuleHandleA(LPCSTR name) {
    // Main image or an already-mapped shim. dlopen(nullptr) hands back the
    // main program handle, which GetProcAddress can query via dlsym.
    if (!name) return dlopen(nullptr, RTLD_NOW);
    // Games commonly do GetModuleHandleW(L"user32.dll") + GetProcAddress to
    // resolve APIs dynamically; map the Windows name onto our shim soname.
    if (void* h = dlopen_shim_for_dll(name)) return h;
    return dlopen(name, RTLD_NOW);
}
WIN2LIN_EXPORT HMODULE WINAPI GetModuleHandleW(LPCWSTR name) {
    if (!name) return dlopen(nullptr, RTLD_NOW);
    return GetModuleHandleA(utf16_to_utf8(name).c_str());
}

WIN2LIN_EXPORT HMODULE WINAPI LoadLibraryA(LPCSTR name) {
    if (!name) { set_last_error(ERROR_INVALID_PARAMETER); return nullptr; }
    // Map foo.dll -> libwin_foo.so (with CRT/api-set aliasing). Delay-loaded
    // imports call this at runtime.
    void* h = dlopen_shim_for_dll(name);
    if (!h) h = dlopen(dll_stem_of(name).c_str(), RTLD_NOW | RTLD_GLOBAL);
    if (!h) { set_last_error(ERROR_FILE_NOT_FOUND); return nullptr; }
    return h;
}
WIN2LIN_EXPORT HMODULE WINAPI LoadLibraryW(LPCWSTR name) {
    return LoadLibraryA(utf16_to_utf8(name).c_str());
}
WIN2LIN_EXPORT HMODULE WINAPI LoadLibraryExA(LPCSTR n, HANDLE, DWORD) {
    return LoadLibraryA(n);
}
WIN2LIN_EXPORT HMODULE WINAPI LoadLibraryExW(LPCWSTR n, HANDLE, DWORD) {
    return LoadLibraryW(n);
}

WIN2LIN_EXPORT void* WINAPI GetProcAddress(HMODULE mod, LPCSTR name) {
    if (!mod || !name) { set_last_error(ERROR_INVALID_PARAMETER); return nullptr; }
    void* sym = dlsym(mod, name);
    if (!sym) set_last_error(ERROR_FILE_NOT_FOUND);
    return sym;
}

WIN2LIN_EXPORT BOOL WINAPI FreeLibrary(HMODULE) {
    // Do not actually unload: PE code may retain pointers into the module.
    return 1;
}

WIN2LIN_EXPORT BOOL WINAPI IsDebuggerPresent(void) { return 0; }

using LPTOP_LEVEL_EXCEPTION_FILTER = LONG(WINAPI*)(void*);
WIN2LIN_EXPORT LPTOP_LEVEL_EXCEPTION_FILTER WINAPI SetUnhandledExceptionFilter(
    LPTOP_LEVEL_EXCEPTION_FILTER filter) {
    static LPTOP_LEVEL_EXCEPTION_FILTER t_previous = nullptr;
    LPTOP_LEVEL_EXCEPTION_FILTER old = t_previous;
    t_previous = filter;
    return old;
}

WIN2LIN_EXPORT void WINAPI GetStartupInfoW(STARTUPINFOW* si) {
    if (!si) return;
    std::memset(si, 0, sizeof(*si));
    si->cb = sizeof(STARTUPINFOW);
    si->wShowWindow = 1; // SW_SHOWNORMAL
    si->hStdInput = reinterpret_cast<HANDLE>(static_cast<intptr_t>(0));
    si->hStdOutput = reinterpret_cast<HANDLE>(static_cast<intptr_t>(1));
    si->hStdError = reinterpret_cast<HANDLE>(static_cast<intptr_t>(2));
}

WIN2LIN_EXPORT void WINAPI GetStartupInfoA(STARTUPINFOW* si) {
    // STARTUPINFOA and STARTUPINFOW are layout-identical on x64 (pointers are
    // the same width); fill once and let the caller interpret the strings.
    GetStartupInfoW(si);
}

WIN2LIN_EXPORT void WINAPI OutputDebugStringA(LPCSTR s) {
    if (s) fprintf(stderr, "[win2lin:debug] %s", s);
}
WIN2LIN_EXPORT void WINAPI OutputDebugStringW(LPCWSTR s) {
    OutputDebugStringA(utf16_to_utf8(s).c_str());
}
WIN2LIN_EXPORT BOOL WINAPI FlushInstructionCache(HANDLE, LPCVOID, SIZE_T) {
    // x86 has coherent I-cache; nothing to do.
    return 1;
}
WIN2LIN_EXPORT void WINAPI SwitchToThread(void) { sched_yield(); }

// ==========================================================================
// Memory
// ==========================================================================
WIN2LIN_EXPORT LPVOID WINAPI VirtualAlloc(LPVOID addr, SIZE_T size, DWORD type,
                                          DWORD protect) {
    constexpr uint64_t GRAN = 0x10000; // Windows allocation granularity
    if (size == 0) { set_last_error(ERROR_INVALID_PARAMETER); return nullptr; }

    const bool reserve = (type & MEM_RESERVE) != 0;
    const bool commit = (type & MEM_COMMIT) != 0;

    // Commit-only on a returned reservation: just change protection.
    if (commit && !reserve) {
        std::lock_guard<std::mutex> lk(g_valloc_mutex);
        auto it = g_reservations.find(addr);
        if (it == g_reservations.end()) { set_last_error(ERROR_INVALID_PARAMETER); return nullptr; }
        if (mprotect(addr, size, prot_from_win(protect)) != 0) {
            set_last_error(errno_to_win(errno));
            return nullptr;
        }
        it->second.protect = protect;
        return addr;
    }

    // Reserve (optionally also commit) a fresh region.
    int prot = commit ? prot_from_win(protect) : PROT_NONE;
    const uint64_t aligned_size = (size + GRAN - 1) & ~(GRAN - 1);

    void* want = addr;
    if (want) {
        // Round the hint down to allocation granularity, like Windows does.
        want = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(want) & ~(GRAN - 1));
    }
    void* got = mmap(want, aligned_size, prot,
                     MAP_PRIVATE | MAP_ANONYMOUS | (want ? MAP_FIXED_NOREPLACE : 0),
                     -1, 0);
    if (got == MAP_FAILED && want) {
        // Hint occupied; retry anywhere so engines passing hints still work.
        got = mmap(nullptr, aligned_size, prot, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    }
    if (got == MAP_FAILED) { set_last_error(errno_to_win(errno)); return nullptr; }

    std::lock_guard<std::mutex> lk(g_valloc_mutex);
    g_reservations[got] = {aligned_size, protect};
    return got;
}

WIN2LIN_EXPORT BOOL WINAPI VirtualFree(LPVOID addr, SIZE_T size, DWORD type) {
    if (!addr) { set_last_error(ERROR_INVALID_PARAMETER); return 0; }
    if (type & MEM_RELEASE) {
        std::lock_guard<std::mutex> lk(g_valloc_mutex);
        auto it = g_reservations.find(addr);
        if (it == g_reservations.end()) {
            // Not one of ours (or already released). Fake success: PE code
            // frequently frees CRT-internal regions we never tracked.
            return 1;
        }
        const uint64_t len = it->second.size;
        g_reservations.erase(it);
        if (munmap(addr, len) != 0) { set_last_error(errno_to_win(errno)); return 0; }
        return 1;
    }
    if (type & MEM_DECOMMIT) {
        if (mprotect(addr, size, PROT_NONE) != 0) {
            set_last_error(errno_to_win(errno));
            return 0;
        }
    }
    return 1;
}

WIN2LIN_EXPORT BOOL WINAPI VirtualProtect(LPVOID addr, SIZE_T size, DWORD new_protect,
                                          DWORD* old_protect) {
    if (!addr || size == 0) { set_last_error(ERROR_INVALID_PARAMETER); return 0; }
    // Report the previous protection as the most permissive plausible value:
    // PE code (e.g. CRT relocations, JIT-style patching) only checks that the
    // call succeeds and usually restores via the returned value.
    if (old_protect) *old_protect = PAGE_EXECUTE_READWRITE;

    const uintptr_t page = reinterpret_cast<uintptr_t>(addr) & ~uintptr_t(0xFFF);
    const uintptr_t end = (reinterpret_cast<uintptr_t>(addr) + size + 0xFFF) & ~uintptr_t(0xFFF);
    if (mprotect(reinterpret_cast<void*>(page), end - page, prot_from_win(new_protect)) != 0) {
        set_last_error(errno_to_win(errno));
        return 0;
    }
    return 1;
}

WIN2LIN_EXPORT SIZE_T WINAPI VirtualQuery(LPCVOID addr, MEMORY_BASIC_INFORMATION* mbi,
                                          SIZE_T len) {
    if (!addr || !mbi || len < sizeof(MEMORY_BASIC_INFORMATION)) {
        set_last_error(ERROR_INVALID_PARAMETER);
        return 0;
    }
    // Locate the mapping that owns `addr` via /proc/self/maps.
    const uintptr_t target = reinterpret_cast<uintptr_t>(addr);
    uintptr_t region_lo = target & ~uintptr_t(0xFFF);
    uintptr_t region_hi = region_lo + 0x1000;
    DWORD protect = PAGE_READONLY;
    bool found = false;

    FILE* f = std::fopen("/proc/self/maps", "r");
    if (f) {
        char line[512];
        while (std::fgets(line, sizeof(line), f)) {
            unsigned long long lo = 0, hi = 0;
            char perms[8] = {};
            if (std::sscanf(line, "%llx-%llx %7s", &lo, &hi, perms) != 3) continue;
            if (target >= lo && target < hi) {
                region_lo = target & ~uintptr_t(0xFFF);
                region_hi = static_cast<uintptr_t>(hi);
                protect = PAGE_NOACCESS;
                if (perms[0] == 'r') protect = PAGE_READONLY;
                if (perms[1] == 'w') protect = PAGE_READWRITE;
                if (perms[2] == 'x') protect = PAGE_EXECUTE_READWRITE;
                found = true;
                break;
            }
        }
        std::fclose(f);
    }
    if (!found) { set_last_error(ERROR_INVALID_PARAMETER); return 0; }

    std::memset(mbi, 0, sizeof(*mbi));
    mbi->BaseAddress = reinterpret_cast<PVOID>(region_lo);
    mbi->AllocationBase = reinterpret_cast<PVOID>(region_lo);
    mbi->AllocationProtect = protect;
    mbi->RegionSize = region_hi - region_lo;
    mbi->State = MEM_COMMIT;
    mbi->Protect = protect;
    mbi->Type = MEM_PRIVATE;
    return sizeof(MEMORY_BASIC_INFORMATION);
}

WIN2LIN_EXPORT HANDLE WINAPI GetProcessHeap(void) {
    return reinterpret_cast<HANDLE>(0x5150'4a00ULL); // sentinel, never dereferenced
}
WIN2LIN_EXPORT HANDLE WINAPI HeapCreate(DWORD, SIZE_T, SIZE_T) { return GetProcessHeap(); }
WIN2LIN_EXPORT BOOL WINAPI HeapDestroy(HANDLE) { return 1; }

WIN2LIN_EXPORT LPVOID WINAPI HeapAlloc(HANDLE, DWORD flags, SIZE_T size) {
    if (size == 0) size = 1;
    void* p = (flags & 0x8) ? std::calloc(1, size) // HEAP_ZERO_MEMORY
                            : std::malloc(size);
    if (!p) set_last_error(8 /*ERROR_NOT_ENOUGH_MEMORY*/);
    return p;
}
WIN2LIN_EXPORT LPVOID WINAPI HeapReAlloc(HANDLE, DWORD flags, LPVOID mem, SIZE_T size) {
    void* p;
    if (flags & 0x8) {
        // HEAP_ZERO_MEMORY on realloc: glibc realloc does not zero the tail.
        size_t old = mem ? malloc_usable_size(mem) : 0;
        p = std::realloc(mem, size ? size : 1);
        if (p && size > old) std::memset(static_cast<uint8_t*>(p) + old, 0, size - old);
    } else {
        p = std::realloc(mem, size ? size : 1);
    }
    if (!p) set_last_error(8);
    return p;
}
WIN2LIN_EXPORT BOOL WINAPI HeapFree(HANDLE, DWORD, LPVOID mem) {
    std::free(mem);
    return 1;
}
WIN2LIN_EXPORT SIZE_T WINAPI HeapSize(HANDLE, DWORD, LPCVOID mem) {
    return mem ? static_cast<SIZE_T>(malloc_usable_size(const_cast<void*>(mem))) : 0;
}

// ==========================================================================
// Threads
// ==========================================================================
WIN2LIN_EXPORT HANDLE WINAPI CreateThread(void*, SIZE_T, THREAD_START_ROUTINE start,
                                          LPVOID param, DWORD, DWORD* tid) {
    if (!start) { set_last_error(ERROR_INVALID_PARAMETER); return nullptr; }
    Handle h;
    h.kind = Handle::Kind::Thread;
    h.joinable = true;
    const HANDLE key = alloc_handle(h);

    // start is an MS-ABI function pointer; calling it from this SysV frame is
    // fine because GCC emits the ABI transition from the declared type.
    auto* body = new std::pair<THREAD_START_ROUTINE, LPVOID>(start, param);
    auto trampoline = [](void* arg) -> void* {
        auto* p = static_cast<std::pair<THREAD_START_ROUTINE, LPVOID>*>(arg);
        p->first(p->second);
        delete p;
        return nullptr;
    };
    pthread_t th;
    if (pthread_create(&th, nullptr, trampoline, body) != 0) {
        delete body;
        set_last_error(errno_to_win(errno));
        return nullptr;
    }
    if (Handle* hp = lookup_handle(key)) hp->thread = th;
    if (tid) *tid = static_cast<DWORD>(reinterpret_cast<uintptr_t>(th));
    return key;
}

WIN2LIN_EXPORT DWORD WINAPI WaitForSingleObject(HANDLE handle, DWORD ms) {
    if (!handle) { set_last_error(ERROR_INVALID_HANDLE); return WAIT_FAILED; }
    Handle* h = lookup_handle(handle);
    if (!h) return WAIT_OBJECT_0; // unknown handle resolves immediately
    if (h->kind == Handle::Kind::Thread && h->joinable) {
        if (ms == 0xFFFFFFFF) { // INFINITE
            pthread_join(h->thread, nullptr);
            h->joinable = false;
            return WAIT_OBJECT_0;
        }
        struct timespec ts{};
        clock_gettime(CLOCK_MONOTONIC, &ts);
        const uint64_t deadline =
            uint64_t(ts.tv_sec) * 1000000000ULL + ts.tv_nsec + uint64_t(ms) * 1000000ULL;
        for (;;) {
            if (pthread_tryjoin_np(h->thread, nullptr) == 0) {
                h->joinable = false;
                return WAIT_OBJECT_0;
            }
            clock_gettime(CLOCK_MONOTONIC, &ts);
            if (uint64_t(ts.tv_sec) * 1000000000ULL + ts.tv_nsec >= deadline) {
                return WAIT_TIMEOUT;
            }
            struct timespec nap{0, 1000000};
            nanosleep(&nap, nullptr);
        }
    }
    return WAIT_OBJECT_0;
}

WIN2LIN_EXPORT DWORD WINAPI WaitForMultipleObjects(DWORD count, const HANDLE* handles,
                                                   BOOL wait_all, DWORD ms) {
    if (!handles || count == 0 || count > 64) {
        set_last_error(ERROR_INVALID_PARAMETER);
        return WAIT_FAILED;
    }
    if (!wait_all) {
        for (;;) {
            for (DWORD i = 0; i < count; ++i) {
                if (WaitForSingleObject(handles[i], 0) == WAIT_OBJECT_0) return i;
            }
            if (ms != 0xFFFFFFFF && ms == 0) return WAIT_TIMEOUT;
            struct timespec nap{0, 1000000};
            nanosleep(&nap, nullptr);
            if (ms != 0xFFFFFFFF && ms > 0) --ms;
        }
    }
    for (DWORD i = 0; i < count; ++i) {
        if (WaitForSingleObject(handles[i], ms) != WAIT_OBJECT_0) return WAIT_TIMEOUT;
    }
    return WAIT_OBJECT_0;
}

WIN2LIN_EXPORT DWORD WINAPI GetCurrentThreadId(void) {
    return static_cast<DWORD>(reinterpret_cast<uintptr_t>(pthread_self()));
}
WIN2LIN_EXPORT void WINAPI Sleep(DWORD ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, nullptr);
}

// ==========================================================================
// Thread Local Storage (dynamic TlsAlloc slots) + critical sections
// ==========================================================================
namespace {
constexpr DWORD MAX_TLS_SLOTS = 1088; // Windows Server 2003+ dynamic TLS limit
constexpr DWORD TLS_OUT_OF_INDEXES = 0xFFFFFFFF;

std::mutex g_tls_alloc_mutex;
bool g_tls_slot_used[MAX_TLS_SLOTS] = {};
// Per-thread value cells; slot allocation itself is process-wide.
// initial-exec TLS: avoids __tls_get_addr (clobbers RDI/RSI under ms_abi).
thread_local void* t_tls_values[MAX_TLS_SLOTS] __attribute__((tls_model("initial-exec"))) = {};

// CRITICAL_SECTION emulation: the PE owns the (opaque) struct memory; we map
// each initialized struct address to a pthread mutex held by the shim.
std::mutex g_cs_registry_mutex;
std::map<void*, pthread_mutex_t*> g_cs_registry;
} // namespace

WIN2LIN_EXPORT DWORD WINAPI TlsAlloc(void) {
    std::lock_guard<std::mutex> lk(g_tls_alloc_mutex);
    for (DWORD i = 0; i < MAX_TLS_SLOTS; ++i) {
        if (!g_tls_slot_used[i]) {
            g_tls_slot_used[i] = true;
            t_tls_values[i] = nullptr;
            return i;
        }
    }
    set_last_error(8 /*ERROR_NOT_ENOUGH_MEMORY*/);
    return TLS_OUT_OF_INDEXES;
}

WIN2LIN_EXPORT LPVOID WINAPI TlsGetValue(DWORD index) {
    if (index >= MAX_TLS_SLOTS) {
        set_last_error(ERROR_INVALID_PARAMETER);
        return nullptr;
    }
    t_last_error = ERROR_SUCCESS;
    return t_tls_values[index];
}

WIN2LIN_EXPORT BOOL WINAPI TlsSetValue(DWORD index, LPVOID value) {
    if (index >= MAX_TLS_SLOTS) {
        set_last_error(ERROR_INVALID_PARAMETER);
        return 0;
    }
    t_tls_values[index] = value;
    return 1;
}

WIN2LIN_EXPORT BOOL WINAPI TlsFree(DWORD index) {
    std::lock_guard<std::mutex> lk(g_tls_alloc_mutex);
    if (index >= MAX_TLS_SLOTS || !g_tls_slot_used[index]) {
        set_last_error(ERROR_INVALID_PARAMETER);
        return 0;
    }
    g_tls_slot_used[index] = false;
    return 1;
}

WIN2LIN_EXPORT void WINAPI InitializeCriticalSection(void* cs) {
    if (!cs) return;
    auto* m = new pthread_mutex_t;
    pthread_mutex_init(m, nullptr);
    std::lock_guard<std::mutex> lk(g_cs_registry_mutex);
    g_cs_registry[cs] = m;
}

WIN2LIN_EXPORT BOOL WINAPI InitializeCriticalSectionAndSpinCount(void* cs, DWORD) {
    InitializeCriticalSection(cs);
    return 1;
}

WIN2LIN_EXPORT void WINAPI EnterCriticalSection(void* cs) {
    pthread_mutex_t* m = nullptr;
    {
        std::lock_guard<std::mutex> lk(g_cs_registry_mutex);
        auto it = g_cs_registry.find(cs);
        if (it != g_cs_registry.end()) m = it->second;
    }
    if (m) pthread_mutex_lock(m); // unknown CS: treat as uncontended no-op
}

WIN2LIN_EXPORT BOOL WINAPI TryEnterCriticalSection(void* cs) {
    pthread_mutex_t* m = nullptr;
    {
        std::lock_guard<std::mutex> lk(g_cs_registry_mutex);
        auto it = g_cs_registry.find(cs);
        if (it != g_cs_registry.end()) m = it->second;
    }
    if (!m) return 1;
    return pthread_mutex_trylock(m) == 0 ? 1 : 0;
}

WIN2LIN_EXPORT void WINAPI LeaveCriticalSection(void* cs) {
    pthread_mutex_t* m = nullptr;
    {
        std::lock_guard<std::mutex> lk(g_cs_registry_mutex);
        auto it = g_cs_registry.find(cs);
        if (it != g_cs_registry.end()) m = it->second;
    }
    if (m) pthread_mutex_unlock(m);
}

WIN2LIN_EXPORT void WINAPI DeleteCriticalSection(void* cs) {
    pthread_mutex_t* m = nullptr;
    {
        std::lock_guard<std::mutex> lk(g_cs_registry_mutex);
        auto it = g_cs_registry.find(cs);
        if (it != g_cs_registry.end()) {
            m = it->second;
            g_cs_registry.erase(it);
        }
    }
    if (m) {
        pthread_mutex_destroy(m);
        delete m;
    }
}

// ==========================================================================
// Character set conversion (Phase 1.5: UTF-8 assumed for ACP)
// ==========================================================================
WIN2LIN_EXPORT int WINAPI IsDBCSLeadByteEx(UINT code_page, BYTE) {
    // UTF-8 (our ACP stand-in) has no single-byte lead bytes in the SBCS
    // sense; DBCS-era code pages are not modelled yet.
    (void)code_page;
    return 0;
}

namespace {
// Decodes one UTF-8 sequence; advances *i. Returns U+FFFD on malformed input.
uint32_t utf8_next(const char* s, size_t len, size_t* i) {
    const auto b = [&](size_t k) -> unsigned char {
        return (*i + k < len) ? static_cast<unsigned char>(s[*i + k]) : 0;
    };
    const unsigned char c = b(0);
    if (c < 0x80) { ++*i; return c; }
    if ((c & 0xE0) == 0xC0 && (b(1) & 0xC0) == 0x80) {
        *i += 2;
        return ((c & 0x1Fu) << 6) | (b(1) & 0x3Fu);
    }
    if ((c & 0xF0) == 0xE0 && (b(1) & 0xC0) == 0x80 && (b(2) & 0xC0) == 0x80) {
        *i += 3;
        return ((c & 0x0Fu) << 12) | ((b(1) & 0x3Fu) << 6) | (b(2) & 0x3Fu);
    }
    if ((c & 0xF8) == 0xF0 && (b(1) & 0xC0) == 0x80 && (b(2) & 0xC0) == 0x80 &&
        (b(3) & 0xC0) == 0x80) {
        *i += 4;
        return ((c & 0x07u) << 18) | ((b(1) & 0x3Fu) << 12) | ((b(2) & 0x3Fu) << 6) |
               (b(3) & 0x3Fu);
    }
    ++*i;
    return 0xFFFD;
}
} // namespace

WIN2LIN_EXPORT int WINAPI MultiByteToWideChar(UINT code_page, DWORD, LPCSTR src,
                                              int src_len, LPWSTR dst, int dst_len) {
    if (!src || src_len == 0) return 0;
    const bool utf8 = (code_page == 65001 || code_page == 0 || code_page == 3 ||
                       code_page == 65000);
    const size_t in_len = (src_len < 0) ? std::strlen(src) : static_cast<size_t>(src_len);

    size_t needed = 0;
    size_t i = 0;
    while (i < in_len) {
        uint32_t cp;
        if (utf8) {
            cp = utf8_next(src, in_len, &i);
        } else {
            cp = static_cast<unsigned char>(src[i++]); // SBCS stand-in
        }
        needed += (cp >= 0x10000) ? 2 : 1;
    }
    if (!dst || dst_len == 0) return static_cast<int>(needed);
    if (static_cast<size_t>(dst_len) < needed) {
        set_last_error(ERROR_INVALID_PARAMETER);
        return 0;
    }

    size_t out = 0;
    i = 0;
    while (i < in_len) {
        uint32_t cp;
        if (utf8) {
            cp = utf8_next(src, in_len, &i);
        } else {
            cp = static_cast<unsigned char>(src[i++]);
        }
        if (cp >= 0x10000) {
            cp -= 0x10000;
            dst[out++] = static_cast<WCHAR>(0xD800 + (cp >> 10));
            dst[out++] = static_cast<WCHAR>(0xDC00 + (cp & 0x3FF));
        } else {
            dst[out++] = static_cast<WCHAR>(cp);
        }
    }
    return static_cast<int>(out);
}

WIN2LIN_EXPORT int WINAPI WideCharToMultiByte(UINT code_page, DWORD, LPCWSTR src,
                                              int src_len, LPSTR dst, int dst_len,
                                              LPCSTR default_char, BOOL* used_default) {
    if (used_default) *used_default = 0;
    if (!src || src_len == 0) return 0;
    const bool utf8 = (code_page == 65001 || code_page == 0 || code_page == 3 ||
                       code_page == 65000);
    size_t in_len;
    if (src_len < 0) {
        in_len = 0;
        while (src[in_len]) ++in_len; // src is WCHAR* (char16_t), not wchar_t
    } else {
        in_len = static_cast<size_t>(src_len);
    }

    size_t needed = 0;
    for (size_t i = 0; i < in_len;) {
        uint32_t cp = src[i];
        if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < in_len && src[i + 1] >= 0xDC00 &&
            src[i + 1] <= 0xDFFF) {
            cp = 0x10000 + ((cp - 0xD800) << 10) + (src[i + 1] - 0xDC00);
            ++i;
        }
        ++i;
        if (!utf8) {
            needed += 1;
        } else if (cp < 0x80) {
            needed += 1;
        } else if (cp < 0x800) {
            needed += 2;
        } else if (cp < 0x10000) {
            needed += 3;
        } else {
            needed += 4;
        }
    }
    if (!dst || dst_len == 0) return static_cast<int>(needed);
    if (static_cast<size_t>(dst_len) < needed) {
        set_last_error(ERROR_INVALID_PARAMETER);
        return 0;
    }

    size_t out = 0;
    for (size_t i = 0; i < in_len;) {
        uint32_t cp = src[i];
        if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < in_len && src[i + 1] >= 0xDC00 &&
            src[i + 1] <= 0xDFFF) {
            cp = 0x10000 + ((cp - 0xD800) << 10) + (src[i + 1] - 0xDC00);
            ++i;
        }
        ++i;
        if (!utf8) {
            dst[out++] = static_cast<char>(cp < 0x100 ? cp
                                                      : (default_char ? *default_char : '?'));
        } else if (cp < 0x80) {
            dst[out++] = static_cast<char>(cp);
        } else if (cp < 0x800) {
            dst[out++] = static_cast<char>(0xC0 | (cp >> 6));
            dst[out++] = static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            dst[out++] = static_cast<char>(0xE0 | (cp >> 12));
            dst[out++] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            dst[out++] = static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            dst[out++] = static_cast<char>(0xF0 | (cp >> 18));
            dst[out++] = static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            dst[out++] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            dst[out++] = static_cast<char>(0x80 | (cp & 0x3F));
        }
    }
    return static_cast<int>(out);
}

// ==========================================================================
// Files
// ==========================================================================
namespace {
int desired_access_to_flags(DWORD access) {
    const bool rd = (access & GENERIC_READ) != 0;
    const bool wr = (access & GENERIC_WRITE) != 0;
    if (rd && wr) return O_RDWR;
    if (wr) return O_WRONLY;
    return O_RDONLY;
}
} // namespace

WIN2LIN_EXPORT HANDLE WINAPI CreateFileW(LPCWSTR path, DWORD access, DWORD share,
                                         void*, DWORD creation, DWORD, HANDLE) {
    if (!path) { set_last_error(ERROR_INVALID_PARAMETER); return reinterpret_cast<HANDLE>(-1); }
    const std::string p = utf16_to_utf8(path);
    int flags = desired_access_to_flags(access);
    switch (creation) {
        case CREATE_NEW: flags |= O_CREAT | O_EXCL; break;
        case CREATE_ALWAYS: flags |= O_CREAT | O_TRUNC; break;
        case OPEN_EXISTING: break;
        case OPEN_ALWAYS: flags |= O_CREAT; break;
        case TRUNCATE_EXISTING: flags |= O_TRUNC; break;
        default: break;
    }
    (void)share; // Win share modes are advisory; POSIX locking is opt-in
    int fd = ::open(p.c_str(), flags, 0644);
    if (fd < 0) {
        set_last_error(errno_to_win(errno));
        return reinterpret_cast<HANDLE>(-1); // INVALID_HANDLE_VALUE
    }
    Handle h;
    h.kind = Handle::Kind::File;
    h.fd = fd;
    return alloc_handle(h);
}

WIN2LIN_EXPORT HANDLE WINAPI CreateFileA(LPCSTR path, DWORD access, DWORD share,
                                         void* sa, DWORD creation, DWORD flags,
                                         HANDLE tmpl) {
    const std::u16string w = latin1_to_utf16(path);
    return CreateFileW(reinterpret_cast<LPCWSTR>(w.c_str()), access, share, sa,
                       creation, flags, tmpl);
}

WIN2LIN_EXPORT BOOL WINAPI ReadFile(HANDLE handle, LPVOID buf, DWORD to_read,
                                    LPDWORD read_count, void*) {
    if (read_count) *read_count = 0;
    if (!buf) { set_last_error(ERROR_INVALID_PARAMETER); return 0; }
    const int fd = handle_to_fd(handle);
    if (fd < 0) { set_last_error(ERROR_INVALID_HANDLE); return 0; }
    ssize_t n = ::read(fd, buf, to_read);
    if (n < 0) { set_last_error(errno_to_win(errno)); return 0; }
    if (read_count) *read_count = static_cast<DWORD>(n);
    return 1;
}

WIN2LIN_EXPORT BOOL WINAPI WriteFile(HANDLE handle, LPCVOID buf, DWORD to_write,
                                     LPDWORD written, void*) {
    if (written) *written = 0;
    if (!buf) { set_last_error(ERROR_INVALID_PARAMETER); return 0; }
    const int fd = handle_to_fd(handle);
    if (fd < 0) { set_last_error(ERROR_INVALID_HANDLE); return 0; }
    const auto* p = static_cast<const uint8_t*>(buf);
    DWORD total = 0;
    while (total < to_write) {
        ssize_t n = ::write(fd, p + total, to_write - total);
        if (n < 0) {
            if (errno == EINTR) continue;
            set_last_error(errno_to_win(errno));
            return 0;
        }
        total += static_cast<DWORD>(n);
    }
    if (written) *written = total;
    return 1;
}

WIN2LIN_EXPORT BOOL WINAPI CloseHandle(HANDLE handle) {
    if (!handle) { set_last_error(ERROR_INVALID_HANDLE); return 0; }
    std::lock_guard<std::mutex> lk(g_handle_mutex);
    auto it = g_handles.find(handle);
    if (it == g_handles.end()) return 1; // std stream or unknown: treat as success
    Handle* h = it->second;
    if (h->kind == Handle::Kind::File && h->fd >= 0) ::close(h->fd);
    if (h->kind == Handle::Kind::Thread && h->joinable) pthread_detach(h->thread);
    g_handles.erase(it);
    delete h;
    return 1;
}

WIN2LIN_EXPORT BOOL WINAPI DeleteFileW(LPCWSTR path) {
    const std::string p = utf16_to_utf8(path);
    if (::unlink(p.c_str()) != 0) { set_last_error(errno_to_win(errno)); return 0; }
    return 1;
}

WIN2LIN_EXPORT DWORD WINAPI GetFileSize(HANDLE handle, LPDWORD high) {
    const int fd = handle_to_fd(handle);
    if (fd < 0) { set_last_error(ERROR_INVALID_HANDLE); return INVALID_SET_FILE_POINTER; }
    struct stat st{};
    if (::fstat(fd, &st) != 0) {
        set_last_error(errno_to_win(errno));
        return INVALID_SET_FILE_POINTER;
    }
    const uint64_t size = static_cast<uint64_t>(st.st_size);
    if (high) *high = static_cast<DWORD>(size >> 32);
    return static_cast<DWORD>(size & 0xFFFFFFFF);
}

WIN2LIN_EXPORT DWORD WINAPI SetFilePointer(HANDLE handle, LONG distance, LONG* high,
                                           DWORD method) {
    const int fd = handle_to_fd(handle);
    if (fd < 0) { set_last_error(ERROR_INVALID_HANDLE); return INVALID_SET_FILE_POINTER; }
    int64_t off = distance;
    if (high) off |= (int64_t(*high) << 32);
    int whence = SEEK_SET;
    if (method == 1) whence = SEEK_CUR;      // FILE_CURRENT
    else if (method == 2) whence = SEEK_END; // FILE_END
    off_t pos = ::lseek(fd, off, whence);
    if (pos < 0) { set_last_error(errno_to_win(errno)); return INVALID_SET_FILE_POINTER; }
    if (high) *high = static_cast<LONG>(pos >> 32);
    return static_cast<DWORD>(pos & 0xFFFFFFFF);
}

// ==========================================================================
// Timing
// ==========================================================================
WIN2LIN_EXPORT DWORD WINAPI GetTickCount(void) {
    struct timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<DWORD>((uint64_t(ts.tv_sec) * 1000ULL) + (ts.tv_nsec / 1000000));
}
WIN2LIN_EXPORT ULONGLONG WINAPI GetTickCount64(void) {
    struct timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return uint64_t(ts.tv_sec) * 1000ULL + (ts.tv_nsec / 1000000);
}
WIN2LIN_EXPORT void WINAPI GetSystemTimeAsFileTime(void* ft) {
    constexpr uint64_t EPOCH_DIFF = 11644473600ULL; // 1601 -> 1970 seconds
    struct timespec ts{};
    clock_gettime(CLOCK_REALTIME, &ts);
    const uint64_t intervals =
        (uint64_t(ts.tv_sec) + EPOCH_DIFF) * 10000000ULL + uint64_t(ts.tv_nsec / 100);
    if (ft) {
        struct FILETIME { DWORD lo, hi; } f;
        f.lo = static_cast<DWORD>(intervals & 0xFFFFFFFF);
        f.hi = static_cast<DWORD>(intervals >> 32);
        std::memcpy(ft, &f, sizeof(f));
    }
}
WIN2LIN_EXPORT void WINAPI QueryPerformanceCounter(LONGLONG* out) {
    struct timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    if (out) *out = int64_t(ts.tv_sec) * 1000000000LL + ts.tv_nsec;
}
WIN2LIN_EXPORT BOOL WINAPI QueryPerformanceFrequency(LONGLONG* out) {
    if (out) *out = 1000000000LL;
    return 1;
}

// ==========================================================================
// Console
// ==========================================================================
WIN2LIN_EXPORT BOOL WINAPI AllocConsole(void) { return 1; }
WIN2LIN_EXPORT BOOL WINAPI AttachConsole(DWORD) { return 1; }
WIN2LIN_EXPORT BOOL WINAPI FreeConsole(void) { return 1; }
WIN2LIN_EXPORT HANDLE WINAPI GetStdHandle(DWORD which) {
    HANDLE h = std_handle(which);
    if (!h) set_last_error(ERROR_INVALID_PARAMETER);
    return h;
}
WIN2LIN_EXPORT BOOL WINAPI SetStdHandle(DWORD, HANDLE) { return 1; }
WIN2LIN_EXPORT BOOL WINAPI GetConsoleMode(HANDLE, DWORD* mode) {
    if (mode) *mode = 0;
    return 1;
}
WIN2LIN_EXPORT BOOL WINAPI SetConsoleMode(HANDLE, DWORD) { return 1; }

// ---------------------------------------------------------------------------
// Thread / condition-variable stubs pulled in by static libstdc++.
// ---------------------------------------------------------------------------
WIN2LIN_EXPORT BOOL WINAPI DuplicateHandle(HANDLE, HANDLE src, HANDLE, HANDLE* dst,
                                           DWORD, BOOL, DWORD) {
    if (dst) *dst = src;
    return 1;
}
WIN2LIN_EXPORT void WINAPI ExitThread(DWORD) { pthread_exit(nullptr); }
WIN2LIN_EXPORT BOOL WINAPI GetExitCodeThread(HANDLE, DWORD* code) {
    if (code) *code = 259; // STILL_ACTIVE
    return 1;
}
WIN2LIN_EXPORT DWORD WINAPI GetThreadId(HANDLE) { return (DWORD)(uintptr_t)pthread_self(); }
WIN2LIN_EXPORT DWORD WINAPI ResumeThread(HANDLE) { return 1; }
WIN2LIN_EXPORT void WINAPI RtlVirtualUnwind(DWORD, uintptr_t, uintptr_t, void*, void*,
                                            void*, void*, void*) {}
WIN2LIN_EXPORT BOOL WINAPI SleepConditionVariableCS(void* cond, void* cs, DWORD ms) {
    // Minimal: treat as a timed wait; wake after ms (or 1 ms if 0/infinite).
    DWORD wait_ms = (ms == 0xFFFFFFFF) ? 1 : ms;
    Sleep(wait_ms);
    return 1;
}
WIN2LIN_EXPORT BOOL WINAPI SleepConditionVariableSR(void* cond, void* sr, DWORD ms) {
    DWORD wait_ms = (ms == 0xFFFFFFFF) ? 1 : ms;
    Sleep(wait_ms);
    return 1;
}
WIN2LIN_EXPORT void WINAPI InitializeConditionVariable(void*) {}
WIN2LIN_EXPORT void WINAPI WakeConditionVariable(void*) {}
WIN2LIN_EXPORT void WINAPI WakeAllConditionVariable(void*) {}
WIN2LIN_EXPORT DWORD WINAPI FormatMessageA(DWORD flags, LPCVOID source, DWORD msg_id,
                                           DWORD lang, LPSTR buf, DWORD size,
                                           void* args) {
    if (buf && size > 0) buf[0] = '\0';
    (void)flags; (void)source; (void)msg_id; (void)lang; (void)args;
    return 0;
}
WIN2LIN_EXPORT void WINAPI GetSystemInfo(void* si) {
    if (si) std::memset(si, 0, 48); // SYSTEM_INFO
}
WIN2LIN_EXPORT DWORD WINAPI GetTimeZoneInformation(void* tz) {
    if (tz) std::memset(tz, 0, 172); // TIME_ZONE_INFORMATION
    return 0; // TIME_ZONE_ID_UNKNOWN
}
WIN2LIN_EXPORT void* WINAPI LocalFree(void* p) { std::free(p); return nullptr; }
WIN2LIN_EXPORT void WINAPI RaiseException(DWORD code, DWORD flags, DWORD n,
                                          const uintptr_t* args) {
    (void)code; (void)flags; (void)n; (void)args;
    std::abort();
}
WIN2LIN_EXPORT void WINAPI RtlCaptureContext(void* ctx) { if (ctx) std::memset(ctx, 0, 0x4D8); }
WIN2LIN_EXPORT void* WINAPI RtlLookupFunctionEntry(uint64_t, uint64_t*, void*) { return nullptr; }
WIN2LIN_EXPORT void WINAPI RtlUnwindEx(void*, void*, void*, void*, void*, void*) {}
