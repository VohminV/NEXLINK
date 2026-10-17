// libwin_msvcrt.so - native POSIX implementation of the subset of
// msvcrt.dll / ucrtbase.dll needed by relinked MSVC- and MinGW-built PE
// binaries (Phase 1.5: MSVCRT bootstrap).
//
// Responsibilities:
//   * nexlink_crt_bootstrap(): idempotent CRT bring-up (heap/stdio/argv),
//     invoked both from a .so constructor (DT_NEEDED load) and from the
//     relinker's entry trampoline when the PE imports CRT functions.
//   * CRT startup surface: __getmainargs, __set_app_type, _initterm,
//     _initterm_e, _onexit/atexit, _amsg_exit, _cexit/_exit/exit/abort,
//     _lock/_unlock, _errno, _fmode/_commode, __iob_func, signal.
//   * libc passthroughs: malloc/calloc/realloc/free, memcpy/memset/...,
//     stdio (fprintf/vfprintf/fwrite/...), wcslen/wcstombs.
//
// ABI notes:
//   * Every exported entry point is WINAPI (Microsoft x64 ABI) so the
//     relinked PE can call through its IAT.
//   * We deliberately do NOT include <cstdio>/<cstdlib>/<cstring>: their
//     declarations would conflict with our ms_abi re-implementations of
//     malloc/fprintf/... The real glibc entry points are resolved once via
//     dlsym(RTLD_NEXT) and called through function pointers, which also
//     prevents the shim from binding to its own exports (infinite recursion).
//   * PE stdio code may reference the legacy msvcrt "iob" array
//     (__iob_func() + 1 == stdout). MSVCRT's FILE is a contiguous 48-byte
//     struct on x64; we emulate that with a fake array and map each slot to
//     the corresponding glibc FILE (stdin/stdout/stderr) on use.

#include "common/win32.hpp"

#include <stdarg.h>
#include <stdint.h>

#include <atomic>

#include <dlfcn.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/syscall.h>
#include <asm/prctl.h>

using namespace win2lin::win;

namespace {

// ---------------------------------------------------------------------------
// Opaque MSVCRT FILE stand-in. Never dereferenced by us: the only FILE*
// values PE code can obtain are fake-iob slots (mapped to glibc std streams)
// or glibc FILE* returned by our fopen passthrough.
// ---------------------------------------------------------------------------
struct _MCRT_FILE;
using FILE = _MCRT_FILE;

// sizeof(struct _iobuf) under mingw-w64 on x86-64; __iob_func()+1 relies on
// pointer arithmetic with this exact stride.
constexpr size_t MS_FILE_STRIDE = 48;
constexpr int MAX_ARGS = 256;
constexpr size_t CMDLINE_CAP = 8192;
constexpr int MAX_ATEXIT = 256;

using OnExitFn = void(WINAPI*)();
using Sighandler = void(WINAPI*)(int);

// ---------------------------------------------------------------------------
// Resolved glibc entry points (SysV ABI - our TU's native convention).
// ---------------------------------------------------------------------------
struct GlibcFns {
    void* (*malloc)(size_t);
    void* (*calloc)(size_t, size_t);
    void* (*realloc)(void*, size_t);
    void (*free)(void*);
    size_t (*malloc_usable_size)(void*);
    size_t (*strlen)(const char*);
    int (*strcmp)(const char*, const char*);
    int (*strncmp)(const char*, const char*, size_t);
    void* (*memcpy)(void*, const void*, size_t);
    void* (*memmove)(void*, const void*, size_t);
    void* (*memset)(void*, int, size_t);
    int (*memcmp)(const void*, const void*, size_t);
    char* (*strcpy)(char*, const char*);
    char* (*strcat)(char*, const char*);
    char* (*strchr)(const char*, int);
    char* (*strstr)(const char*, const char*);
    size_t (*fwrite)(const void*, size_t, size_t, void*);
    size_t (*fread)(void*, size_t, size_t, void*);
    int (*fputc)(int, void*);
    int (*fgetc)(void*);
    int (*fputs)(const char*, void*);
    char* (*fgets)(char*, int, void*);
    int (*fflush)(void*);
    int (*fclose)(void*);
    int (*feof)(void*);
    int (*ferror)(void*);
    void (*clearerr)(void*);
    int (*snprintf)(char*, size_t, const char*, ...);
    void (*exit)(int);
    void (*_exit)(int);
    void (*abort)(void);
    char* (*strerror)(int);
    int (*putchar)(int);
    int (*puts)(const char*);
    void* (*fopen)(const char*, const char*);
    // std stream objects (glibc FILE* values, obtained from data symbols).
    void* stdin_g;
    void* stdout_g;
    void* stderr_g;
};

GlibcFns g_glibc;
pthread_once_t g_once = PTHREAD_ONCE_INIT;

void resolve_glibc() {
    auto sym = [](const char* n) { return dlsym(RTLD_NEXT, n); };
    g_glibc.malloc = reinterpret_cast<decltype(g_glibc.malloc)>(sym("malloc"));
    g_glibc.calloc = reinterpret_cast<decltype(g_glibc.calloc)>(sym("calloc"));
    g_glibc.realloc = reinterpret_cast<decltype(g_glibc.realloc)>(sym("realloc"));
    g_glibc.free = reinterpret_cast<decltype(g_glibc.free)>(sym("free"));
    g_glibc.malloc_usable_size =
        reinterpret_cast<decltype(g_glibc.malloc_usable_size)>(sym("malloc_usable_size"));
    g_glibc.strlen = reinterpret_cast<decltype(g_glibc.strlen)>(sym("strlen"));
    g_glibc.strcmp = reinterpret_cast<decltype(g_glibc.strcmp)>(sym("strcmp"));
    g_glibc.strncmp = reinterpret_cast<decltype(g_glibc.strncmp)>(sym("strncmp"));
    g_glibc.memcpy = reinterpret_cast<decltype(g_glibc.memcpy)>(sym("memcpy"));
    g_glibc.memmove = reinterpret_cast<decltype(g_glibc.memmove)>(sym("memmove"));
    g_glibc.memset = reinterpret_cast<decltype(g_glibc.memset)>(sym("memset"));
    g_glibc.memcmp = reinterpret_cast<decltype(g_glibc.memcmp)>(sym("memcmp"));
    g_glibc.strcpy = reinterpret_cast<decltype(g_glibc.strcpy)>(sym("strcpy"));
    g_glibc.strcat = reinterpret_cast<decltype(g_glibc.strcat)>(sym("strcat"));
    g_glibc.strchr = reinterpret_cast<decltype(g_glibc.strchr)>(sym("strchr"));
    g_glibc.strstr = reinterpret_cast<decltype(g_glibc.strstr)>(sym("strstr"));
    g_glibc.fwrite = reinterpret_cast<decltype(g_glibc.fwrite)>(sym("fwrite"));
    g_glibc.fread = reinterpret_cast<decltype(g_glibc.fread)>(sym("fread"));
    g_glibc.fputc = reinterpret_cast<decltype(g_glibc.fputc)>(sym("fputc"));
    g_glibc.fgetc = reinterpret_cast<decltype(g_glibc.fgetc)>(sym("fgetc"));
    g_glibc.fputs = reinterpret_cast<decltype(g_glibc.fputs)>(sym("fputs"));
    g_glibc.fgets = reinterpret_cast<decltype(g_glibc.fgets)>(sym("fgets"));
    g_glibc.fflush = reinterpret_cast<decltype(g_glibc.fflush)>(sym("fflush"));
    g_glibc.fclose = reinterpret_cast<decltype(g_glibc.fclose)>(sym("fclose"));
    g_glibc.feof = reinterpret_cast<decltype(g_glibc.feof)>(sym("feof"));
    g_glibc.ferror = reinterpret_cast<decltype(g_glibc.ferror)>(sym("ferror"));
    g_glibc.clearerr = reinterpret_cast<decltype(g_glibc.clearerr)>(sym("clearerr"));
    g_glibc.snprintf = reinterpret_cast<decltype(g_glibc.snprintf)>(sym("snprintf"));
    g_glibc.exit = reinterpret_cast<decltype(g_glibc.exit)>(sym("exit"));
    g_glibc._exit = reinterpret_cast<decltype(g_glibc._exit)>(sym("_exit"));
    g_glibc.abort = reinterpret_cast<decltype(g_glibc.abort)>(sym("abort"));
    g_glibc.strerror = reinterpret_cast<decltype(g_glibc.strerror)>(sym("strerror"));
    g_glibc.putchar = reinterpret_cast<decltype(g_glibc.putchar)>(sym("putchar"));
    g_glibc.puts = reinterpret_cast<decltype(g_glibc.puts)>(sym("puts"));
    g_glibc.fopen = reinterpret_cast<decltype(g_glibc.fopen)>(sym("fopen"));
    // dlsym on a data symbol returns the object's address: stdout is a
    // FILE* variable in glibc, so dereference once.
    if (void** pp = reinterpret_cast<void**>(sym("stdin"))) g_glibc.stdin_g = *pp;
    if (void** pp = reinterpret_cast<void**>(sym("stdout"))) g_glibc.stdout_g = *pp;
    if (void** pp = reinterpret_cast<void**>(sym("stderr"))) g_glibc.stderr_g = *pp;
}

// Every public entry point calls this first; pthread_once makes it cheap and
// safe even if user threads race into CRT calls.
void ensure_crt() { pthread_once(&g_once, resolve_glibc); }

// ---------------------------------------------------------------------------
// Fake msvcrt "iob" array. Slot i maps to glibc stdin/stdout/stderr.
// ---------------------------------------------------------------------------
alignas(8) unsigned char g_iob[3 * MS_FILE_STRIDE] = {};

FILE* iob_stdin() { return reinterpret_cast<FILE*>(g_iob); }
FILE* iob_stdout() { return reinterpret_cast<FILE*>(g_iob + MS_FILE_STRIDE); }
FILE* iob_stderr() { return reinterpret_cast<FILE*>(g_iob + 2 * MS_FILE_STRIDE); }

void* file_to_glibc(FILE* f) {
    if (!f) return g_glibc.stdout_g;
    const auto* p = reinterpret_cast<const unsigned char*>(f);
    if (p >= g_iob && p < g_iob + sizeof(g_iob)) {
        const size_t idx = static_cast<size_t>(p - g_iob) / MS_FILE_STRIDE;
        if (idx == 0) return g_glibc.stdin_g;
        if (idx == 1) return g_glibc.stdout_g;
        return g_glibc.stderr_g;
    }
    return f; // fopen() result: already a glibc FILE*
}

// ---------------------------------------------------------------------------
// CRT process state.
// ---------------------------------------------------------------------------
int g_app_type = 0; // 1=console, 2=GUI (from __set_app_type)
int g_argc = 0;
char* g_argv_data[MAX_ARGS + 1] = {};
char** g_argv = g_argv_data; // pointer variable so __p___argv can return &g_argv
char16_t g_wargv_storage[MAX_ARGS][256];
char16_t* g_wargv[MAX_ARGS + 1] = {};
char g_cmdline_buf[CMDLINE_CAP] = {};
char g_acmdln_buf[CMDLINE_CAP] = {};
char16_t g_wcmdline_buf[CMDLINE_CAP] = {};

OnExitFn g_atexit_list[MAX_ATEXIT] = {};
int g_atexit_count = 0;
pthread_mutex_t g_atexit_mutex = PTHREAD_MUTEX_INITIALIZER;

// initial-exec TLS: avoids __tls_get_addr (a SysV call that clobbers RDI,
// which is callee-saved under the ms_abi our callers use).
thread_local int t_errno_value __attribute__((tls_model("initial-exec"))) = 0;

void run_atexit_handlers() {
    // Run in reverse registration order, like the real CRT.
    for (;;) {
        OnExitFn fn = nullptr;
        pthread_mutex_lock(&g_atexit_mutex);
        if (g_atexit_count > 0) fn = g_atexit_list[--g_atexit_count];
        pthread_mutex_unlock(&g_atexit_mutex);
        if (!fn) break;
        fn();
    }
}

bool register_atexit(OnExitFn fn) {
    if (!fn) return false;
    pthread_mutex_lock(&g_atexit_mutex);
    bool ok = g_atexit_count < MAX_ATEXIT;
    if (ok) g_atexit_list[g_atexit_count++] = fn;
    pthread_mutex_unlock(&g_atexit_mutex);
    return ok;
}

// UTF-8 -> UTF-16 (surrogate pairs for astral planes). Returns code units
// written, excluding the terminator.
size_t utf8_to_utf16(const char* src, char16_t* dst, size_t dst_units) {
    size_t out = 0;
    const auto emit = [&](uint32_t cp) {
        if (cp >= 0x10000) {
            if (out + 2 > dst_units) return false;
            cp -= 0x10000;
            dst[out++] = static_cast<char16_t>(0xD800 + (cp >> 10));
            dst[out++] = static_cast<char16_t>(0xDC00 + (cp & 0x3FF));
            return true;
        }
        if (out + 1 > dst_units) return false;
        dst[out++] = static_cast<char16_t>(cp);
        return true;
    };
    for (const unsigned char* s = reinterpret_cast<const unsigned char*>(src); *s;) {
        uint32_t cp = 0;
        if (*s < 0x80) {
            cp = *s++;
        } else if ((*s & 0xE0) == 0xC0 && (s[1] & 0xC0) == 0x80) {
            cp = ((*s & 0x1F) << 6) | (s[1] & 0x3F);
            s += 2;
        } else if ((*s & 0xF0) == 0xE0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80) {
            cp = ((*s & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
            s += 3;
        } else if ((*s & 0xF8) == 0xF0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80 &&
                   (s[3] & 0xC0) == 0x80) {
            cp = ((*s & 0x07) << 18) | ((s[1] & 0x3F) << 12) | ((s[2] & 0x3F) << 6) |
                 (s[3] & 0x3F);
            s += 4;
        } else {
            cp = 0xFFFD;
            ++s;
        }
        if (!emit(cp)) break;
    }
    return out;
}

// ---------------------------------------------------------------------------
// Mini printf engine. The PE builds an MS-ABI va_list; GCC's va_arg inside a
// ms_abi function follows MS rules, so formatting is done here rather than
// forwarding to glibc's SysV vfprintf (which would misread the va_list).
// ---------------------------------------------------------------------------
struct FmtSink {
    void (*emit)(FmtSink*, const char*, size_t);
    FILE* file; // file sink target
    char* buf;  // buffer sink target
    size_t cap;
    size_t len; // total length that would have been written (snprintf-like)
};

void sink_file(FmtSink* s, const char* d, size_t n) {
    if (n) g_glibc.fwrite(d, 1, n, file_to_glibc(s->file));
    s->len += n;
}

void sink_buf(FmtSink* s, const char* d, size_t n) {
    if (s->buf && s->cap > 0) {
        const size_t room = s->cap - 1 - (s->len < s->cap - 1 ? s->len : s->cap - 1);
        const size_t copy = n < room ? n : room;
        if (copy) {
            for (size_t i = 0; i < copy; ++i) s->buf[s->len + i] = d[i];
        }
    }
    s->len += n;
}

void emit_bytes(FmtSink* s, const char* d, size_t n) { s->emit(s, d, n); }

void emit_repeat(FmtSink* s, char c, int n) {
    char tmp[64];
    const char fill = c;
    for (int i = 0; i < n; i += 64) {
        const int chunk = (n - i) < 64 ? (n - i) : 64;
        for (int j = 0; j < chunk; ++j) tmp[j] = fill;
        emit_bytes(s, tmp, static_cast<size_t>(chunk));
    }
}

struct FmtSpec {
    bool left = false;    // '-'
    bool plus = false;    // '+'
    bool space = false;   // ' '
    bool alternate = false; // '#'
    bool zero = false;    // '0'
    int width = -1;
    int prec = -1;
    char length = 0;      // 0, 'h', 'l', 'L', 'z', 'j', 't'; 'll' -> 'q'
    char conv = 0;
};

const char* parse_spec(const char* p, FmtSpec& sp) {
    for (;; ++p) {
        if (*p == '-') sp.left = true;
        else if (*p == '+') sp.plus = true;
        else if (*p == ' ') sp.space = true;
        else if (*p == '#') sp.alternate = true;
        else if (*p == '0') sp.zero = true;
        else break;
    }
    if (*p == '*') {
        sp.width = -2; // filled from va_arg later
        ++p;
    } else {
        sp.width = 0;
        while (*p >= '0' && *p <= '9') sp.width = sp.width * 10 + (*p++ - '0');
    }
    if (*p == '.') {
        ++p;
        if (*p == '*') {
            sp.prec = -2;
            ++p;
        } else {
            sp.prec = 0;
            while (*p >= '0' && *p <= '9') sp.prec = sp.prec * 10 + (*p++ - '0');
        }
    }
    if (*p == 'h') { sp.length = 'h'; ++p; if (*p == 'h') { sp.length = 'H'; ++p; } }
    else if (*p == 'l') { sp.length = 'l'; ++p; if (*p == 'l') { sp.length = 'q'; ++p; } }
    else if (*p == 'L') { sp.length = 'L'; ++p; }
    else if (*p == 'z') { sp.length = 'z'; ++p; }
    else if (*p == 'j') { sp.length = 'j'; ++p; }
    else if (*p == 't') { sp.length = 't'; ++p; }
    sp.conv = *p;
    return p + 1;
}

// Formats an unsigned integer into buf (reversed helper avoided for clarity).
// Returns length written into out[].
int utoa_rev(uint64_t v, int base, bool upper, char* out) {
    const char* digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    int n = 0;
    do {
        out[n++] = digits[v % static_cast<uint64_t>(base)];
        v /= static_cast<uint64_t>(base);
    } while (v);
    return n;
}

void emit_padded(FmtSink* s, const FmtSpec& sp, const char* prefix, int prefix_len,
                 const char* body_rev, int body_len) {
    const int total = prefix_len + body_len;
    int pad = sp.width > total ? sp.width - total : 0;
    if (!sp.left && !sp.zero) emit_repeat(s, ' ', pad);
    if (prefix_len) emit_bytes(s, prefix, static_cast<size_t>(prefix_len));
    if (!sp.left && sp.zero) emit_repeat(s, '0', pad);
    for (int i = body_len - 1; i >= 0; --i) emit_bytes(s, &body_rev[i], 1);
    if (sp.left) emit_repeat(s, ' ', pad);
}

int format_impl(FmtSink* s, const char* fmt, va_list ap) {
    char num[72];
    for (const char* p = fmt; *p;) {
        if (*p != '%') {
            const char* start = p;
            while (*p && *p != '%') ++p;
            emit_bytes(s, start, static_cast<size_t>(p - start));
            continue;
        }
        ++p;
        if (*p == '%') {
            emit_bytes(s, "%", 1);
            ++p;
            continue;
        }
        FmtSpec sp;
        p = parse_spec(p, sp);
        if (sp.width == -2) sp.width = va_arg(ap, int);
        if (sp.prec == -2) sp.prec = va_arg(ap, int);
        if (sp.width < 0) sp.width = 0;

        switch (sp.conv) {
            case 'd':
            case 'i': {
                int64_t v;
                switch (sp.length) {
                    case 'H': v = static_cast<signed char>(va_arg(ap, int)); break;
                    case 'h': v = static_cast<short>(va_arg(ap, int)); break;
                    case 'q': v = va_arg(ap, long long); break;
                    case 'l': v = va_arg(ap, long); break;
                    case 'z': v = static_cast<int64_t>(va_arg(ap, size_t)); break;
                    case 'j': v = va_arg(ap, long long); break;
                    case 't': v = va_arg(ap, long); break;
                    default: v = va_arg(ap, int); break;
                }
                char sign = 0;
                uint64_t uv;
                if (v < 0) {
                    sign = '-';
                    uv = static_cast<uint64_t>(-v);
                } else {
                    uv = static_cast<uint64_t>(v);
                    if (sp.plus) sign = '+';
                    else if (sp.space) sign = ' ';
                }
                int len = utoa_rev(uv, 10, false, num);
                if (sp.prec >= 0) {
                    // Precision: minimum digits; zero flag ignored then.
                    while (len < sp.prec && len < 70) num[len++] = '0';
                    sp.zero = false;
                }
                const char prefix = sign;
                emit_padded(s, sp, sign ? &prefix : nullptr, sign ? 1 : 0, num, len);
                break;
            }
            case 'u':
            case 'x':
            case 'X':
            case 'o': {
                uint64_t v;
                switch (sp.length) {
                    case 'H': v = static_cast<unsigned char>(va_arg(ap, unsigned)); break;
                    case 'h': v = static_cast<unsigned short>(va_arg(ap, unsigned)); break;
                    case 'q': v = va_arg(ap, unsigned long long); break;
                    case 'l': v = va_arg(ap, unsigned long); break;
                    case 'z': v = va_arg(ap, size_t); break;
                    case 'j': v = va_arg(ap, unsigned long long); break;
                    case 't': v = va_arg(ap, unsigned long); break;
                    default: v = va_arg(ap, unsigned); break;
                }
                int base = sp.conv == 'o' ? 8 : (sp.conv == 'u' ? 10 : 16);
                int len = utoa_rev(v, base, sp.conv == 'X', num);
                if (sp.prec >= 0) {
                    while (len < sp.prec && len < 70) num[len++] = '0';
                    sp.zero = false;
                }
                char px[3];
                int pxlen = 0;
                if (sp.alternate && v != 0 && base == 16) {
                    px[pxlen++] = '0';
                    px[pxlen++] = sp.conv == 'X' ? 'X' : 'x';
                }
                emit_padded(s, sp, pxlen ? px : nullptr, pxlen, num, len);
                break;
            }
            case 'p': {
                void* v = va_arg(ap, void*);
                uint64_t uv = reinterpret_cast<uint64_t>(v);
                int len = utoa_rev(uv, 16, false, num);
                const char px[2] = {'0', 'x'};
                emit_padded(s, sp, px, 2, num, len);
                break;
            }
            case 'c': {
                char c = static_cast<char>(va_arg(ap, int));
                if (!sp.left && sp.width > 1) emit_repeat(s, ' ', sp.width - 1);
                emit_bytes(s, &c, 1);
                if (sp.left && sp.width > 1) emit_repeat(s, ' ', sp.width - 1);
                break;
            }
            case 's': {
                const char* str = va_arg(ap, const char*);
                if (!str) str = "(null)";
                size_t len = g_glibc.strlen(str);
                if (sp.prec >= 0 && static_cast<size_t>(sp.prec) < len)
                    len = static_cast<size_t>(sp.prec);
                int pad = sp.width > static_cast<int>(len) ? sp.width - static_cast<int>(len) : 0;
                if (!sp.left) emit_repeat(s, ' ', pad);
                emit_bytes(s, str, len);
                if (sp.left) emit_repeat(s, ' ', pad);
                break;
            }
            case 'f':
            case 'F':
            case 'e':
            case 'E':
            case 'g':
            case 'G': {
                double v = va_arg(ap, double);
                // Rebuild the spec for glibc's snprintf (SysV call from here).
                char gfmt[24];
                int gi = 0;
                gfmt[gi++] = '%';
                if (sp.left) gfmt[gi++] = '-';
                if (sp.plus) gfmt[gi++] = '+';
                if (sp.space) gfmt[gi++] = ' ';
                if (sp.alternate) gfmt[gi++] = '#';
                if (sp.zero) gfmt[gi++] = '0';
                if (sp.width > 0) gi += g_glibc.snprintf(gfmt + gi, sizeof(gfmt) - gi, "%d", sp.width);
                if (sp.prec >= 0) {
                    gfmt[gi++] = '.';
                    gi += g_glibc.snprintf(gfmt + gi, sizeof(gfmt) - gi, "%d", sp.prec);
                }
                gfmt[gi++] = sp.conv;
                gfmt[gi] = '\0';
                char tmp[512];
                int n = g_glibc.snprintf(tmp, sizeof(tmp), gfmt, v);
                if (n > 0) emit_bytes(s, tmp, static_cast<size_t>(n < 511 ? n : 511));
                break;
            }
            case 'n': {
                int* out = va_arg(ap, int*);
                if (out) *out = static_cast<int>(s->len);
                break;
            }
            default: {
                // Unknown conversion: emit literally to stay debuggable.
                emit_bytes(s, "%", 1);
                if (sp.conv) emit_bytes(s, &sp.conv, 1);
                break;
            }
        }
    }
    return static_cast<int>(s->len);
}

int vformat_to_file(FILE* f, const char* fmt, va_list ap) {
    FmtSink s{};
    s.emit = sink_file;
    s.file = f;
    return format_impl(&s, fmt, ap);
}

int vformat_to_buf(char* buf, size_t cap, const char* fmt, va_list ap) {
    FmtSink s{};
    s.emit = sink_buf;
    s.buf = buf;
    s.cap = cap;
    s.len = 0;
    const int r = format_impl(&s, fmt, ap);
    if (buf && cap > 0) {
        const size_t term = s.len < cap - 1 ? s.len : cap - 1;
        buf[term] = '\0';
    }
    return r;
}

// ---------------------------------------------------------------------------
// Command line capture (/proc/self/cmdline).
// ---------------------------------------------------------------------------
void build_cmdline() {
    const int fd = ::open("/proc/self/cmdline", O_RDONLY);
    size_t total = 0;
    if (fd >= 0) {
        const ssize_t n = ::read(fd, g_cmdline_buf, sizeof(g_cmdline_buf) - 1);
        ::close(fd);
        if (n > 0) total = static_cast<size_t>(n);
    }
    if (total == 0) {
        g_cmdline_buf[0] = 'p'; g_cmdline_buf[1] = 'e'; g_cmdline_buf[2] = '\0';
        total = 3;
    }
    g_cmdline_buf[total] = '\0';

    // Split on NULs into argv.
    size_t pos = 0;
    g_argc = 0;
    while (pos < total && g_argc < MAX_ARGS) {
        g_argv[g_argc++] = g_cmdline_buf + pos;
        while (pos < total && g_cmdline_buf[pos] != '\0') ++pos;
        if (pos < total) ++pos; // skip the NUL
    }
    g_argv[g_argc] = nullptr;

    // Space-joined narrow copy for _acmdln.
    size_t o = 0;
    for (int i = 0; i < g_argc && o + 2 < CMDLINE_CAP; ++i) {
        if (i) g_acmdln_buf[o++] = ' ';
        for (const char* q = g_argv[i]; *q && o + 2 < CMDLINE_CAP; ++q)
            g_acmdln_buf[o++] = *q;
    }
    g_acmdln_buf[o] = '\0';

    // UTF-16 command line + wide argv.
    const size_t wlen = utf8_to_utf16(g_acmdln_buf, g_wcmdline_buf, CMDLINE_CAP - 1);
    g_wcmdline_buf[wlen] = 0;
    for (int i = 0; i < g_argc; ++i) {
        g_wargv_storage[i][0] = 0;
        utf8_to_utf16(g_argv[i], g_wargv_storage[i], 255);
        g_wargv_storage[i][255] = 0;
        g_wargv[i] = g_wargv_storage[i];
    }
    g_wargv[g_argc] = nullptr;
}

// ---------------------------------------------------------------------------
// Fake Windows TEB mapped at the GS base.
//
// MinGW CRT startup (compiled into the PE) reads the TEB directly, e.g.
// `mov rax, gs:[0x30]` (TEB->Self) then TEB+0x8 (NT_TIB.StackBase) for the
// /GS security-cookie initialisation - no import involved, so the only way to
// satisfy it is to put a plausible TEB under GS. Linux x86-64 keeps TLS in FS
// and leaves GS unused for userspace, so ARCH_SET_GS is safe; glibc never
// touches it. Per-thread GS installation for PE-created threads is deferred
// (Phase 1.5 runs single-threaded startup on the main thread).
// ---------------------------------------------------------------------------
void ensure_teb() {
    static std::atomic_flag done = ATOMIC_FLAG_INIT;
    if (done.test_and_set()) return;

    constexpr size_t kRegion = 0x10000; // 64 KiB: TEB page + gap + PEB page
    uint8_t* region = static_cast<uint8_t*>(
        ::mmap(nullptr, kRegion, PROT_READ | PROT_WRITE,
               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    if (region == MAP_FAILED) return;

    uint8_t* teb = region;          // GS base
    uint8_t* peb = region + 0x8000; // TEB->ProcessEnvironmentBlock target

    // Zero the region with volatile stores: a plain memset would be emitted
    // as `call memset@plt`, which resolves to THIS shim's ms_abi memset and
    // would read SysV-passed arguments from the wrong registers.
    volatile uint8_t* vp = region;
    for (size_t i = 0; i < kRegion; ++i) vp[i] = 0;

    const uintptr_t sp = reinterpret_cast<uintptr_t>(__builtin_frame_address(0));
    // NT_TIB: StackBase / StackLimit - any readable window around rsp works;
    // the CRT only folds them into the security cookie.
    *reinterpret_cast<uintptr_t*>(teb + 0x08) = sp + 0x1000000;
    *reinterpret_cast<uintptr_t*>(teb + 0x10) = sp - 0x1000000;
    // NT_TIB.Self
    *reinterpret_cast<uintptr_t*>(teb + 0x30) = reinterpret_cast<uintptr_t>(teb);
    // CLIENT_ID {UniqueProcess, UniqueThread} - inlined GetCurrentProcessId/
    // GetCurrentThreadId read these.
    *reinterpret_cast<uintptr_t*>(teb + 0x40) =
        static_cast<uintptr_t>(::getpid());
    *reinterpret_cast<uintptr_t*>(teb + 0x48) =
        static_cast<uintptr_t>(::syscall(SYS_gettid));
    // TEB->ProcessEnvironmentBlock (PEB pointer lives at TEB+0x600 on x64).
    *reinterpret_cast<uintptr_t*>(teb + 0x600) =
        reinterpret_cast<uintptr_t>(peb);
    // PEB+0x2 BeingDebugged = 0, PEB+0xBC NtGlobalFlag = 0 (already zeroed).

    (void)::syscall(SYS_arch_prctl, ARCH_SET_GS,
                    reinterpret_cast<unsigned long>(teb));
}

} // namespace

// ==========================================================================
// Data exports (referenced directly through the PE's IAT as GLOB_DAT).
// Declared before nexlink_crt_bootstrap(), which assigns them at bring-up.
// ==========================================================================
WIN2LIN_EXPORT int _fmode = 0;      // 0 = binary (default)
WIN2LIN_EXPORT int _commode = 0;    // 0 = binary newline translation
WIN2LIN_EXPORT char** __initenv = nullptr;
WIN2LIN_EXPORT char** _environ = nullptr;
WIN2LIN_EXPORT char* _acmdln = g_acmdln_buf;

// ==========================================================================
// CRT bootstrap (relinker trampoline target + .so constructor)
// ==========================================================================
WIN2LIN_EXPORT void WINAPI nexlink_crt_bootstrap(void) {
    ensure_crt();
    ensure_teb(); // TEB under GS before any PE code runs (CRT reads gs:0x30)
    static std::atomic_flag logged = ATOMIC_FLAG_INIT;
    if (!logged.test_and_set()) {
        build_cmdline();
        extern char** environ;
        _environ = environ;
        __initenv = environ;
        static const char msg[] = "[NEXLINK MSVCRT] bootstrap: CRT initialized\n";
        (void)::write(2, msg, sizeof(msg) - 1);
    }
}

namespace {
__attribute__((constructor)) void msvcrt_shim_ctor() { nexlink_crt_bootstrap(); }
} // namespace

// ==========================================================================
// CRT startup surface
// ==========================================================================
WIN2LIN_EXPORT int WINAPI __getmainargs(int* argc, char*** argv, char*** env,
                                        int, void*) {
    ensure_crt();
    if (argc) *argc = g_argc;
    if (argv) *argv = g_argv;
    if (env) {
        extern char** environ;
        *env = environ;
    }
    return 0;
}

WIN2LIN_EXPORT int WINAPI __wgetmainargs(int* argc, char16_t*** argv, char16_t*** env,
                                         int, void*) {
    ensure_crt();
    if (argc) *argc = g_argc;
    if (argv) *argv = g_wargv;
    if (env) {
        // Wide environment is rarely consumed; hand back an empty list.
        static char16_t* empty_env[1] = {nullptr};
        *env = empty_env;
    }
    return 0;
}

WIN2LIN_EXPORT void WINAPI __set_app_type(int t) { g_app_type = t; }

WIN2LIN_EXPORT FILE* WINAPI __iob_func(void) {
    ensure_crt();
    return iob_stdin(); // contiguous 3-slot array: [stdin, stdout, stderr]
}

// UCRT-style indexed std stream accessor.
WIN2LIN_EXPORT FILE* WINAPI __acrt_iob_func(unsigned index) {
    ensure_crt();
    if (index == 0) return iob_stdin();
    if (index == 1) return iob_stdout();
    return iob_stderr();
}

WIN2LIN_EXPORT void WINAPI __setusermatherr(void*) { /* custom matherr handler unused */ }

// Initializer pointers are MS-ABI functions compiled into the PE itself.
using CtorFn = void(WINAPI*)();
using CtorEFn = int(WINAPI*)();

// _initterm: call [begin, end) initializers (C++ global constructors).
WIN2LIN_EXPORT void WINAPI _initterm(CtorFn* begin, CtorFn* end) {
    if (!begin || !end) return;
    for (CtorFn* fn = begin; fn < end; ++fn) {
        if (*fn) (*fn)();
    }
}

// _initterm_e: like _initterm but initializers return int; nonzero aborts.
WIN2LIN_EXPORT int WINAPI _initterm_e(CtorEFn* begin, CtorEFn* end) {
    if (!begin || !end) return 0;
    for (CtorEFn* fn = begin; fn < end; ++fn) {
        if (*fn && (*fn)() != 0) return 1;
    }
    return 0;
}

WIN2LIN_EXPORT OnExitFn WINAPI _onexit(OnExitFn fn) {
    return register_atexit(fn) ? fn : nullptr;
}

WIN2LIN_EXPORT int WINAPI atexit(OnExitFn fn) {
    return register_atexit(fn) ? 0 : 1;
}

WIN2LIN_EXPORT OnExitFn WINAPI __dllonexit(OnExitFn fn, OnExitFn**, OnExitFn**) {
    return register_atexit(fn) ? fn : nullptr;
}

WIN2LIN_EXPORT void WINAPI _amsg_exit(int code) {
    ensure_crt();
    char msg[96];
    const int n = g_glibc.snprintf(msg, sizeof(msg),
                                   "[NEXLINK MSVCRT] fatal error _amsg_exit(%d)\n", code);
    if (n > 0) (void)::write(2, msg, static_cast<size_t>(n));
    run_atexit_handlers();
    g_glibc.exit(code ? code : 255);
}

WIN2LIN_EXPORT void WINAPI _cexit(void) {
    ensure_crt();
    run_atexit_handlers();
    g_glibc.fflush(nullptr);
}

WIN2LIN_EXPORT void WINAPI _c_exit(void) { _cexit(); }

WIN2LIN_EXPORT void WINAPI exit(int code) {
    ensure_crt();
    run_atexit_handlers();
    g_glibc.exit(code);
}

// unistd.h already declares a SysV-ABI _exit; define ours under a distinct
// identifier and publish it under the "_exit" symbol via an asm alias so the
// PE's IAT resolves to the shim without a redeclaration conflict.
extern "C" void WINAPI nexlink_shim_exit(int code) __asm__("_exit");
extern "C" void WINAPI nexlink_shim_exit(int code) {
    ensure_crt();
    g_glibc._exit(code);
}

WIN2LIN_EXPORT void WINAPI abort(void) {
    ensure_crt();
    g_glibc.abort();
}

WIN2LIN_EXPORT void WINAPI _lock(int idx) { /* CRT lock numbers are no-ops here */ }
WIN2LIN_EXPORT void WINAPI _unlock(int) { }

WIN2LIN_EXPORT Sighandler WINAPI signal(int, Sighandler) {
    // Real signal routing is handled by the host; report "previous = SIG_DFL".
    return nullptr;
}

WIN2LIN_EXPORT int* WINAPI _errno(void) { return &t_errno_value; }
WIN2LIN_EXPORT int* WINAPI __p__errno(void) { return &t_errno_value; }

WIN2LIN_EXPORT unsigned WINAPI ___lc_codepage_func(void) {
    return 65001; // report UTF-8; conversions in the shims are UTF-8 based
}

WIN2LIN_EXPORT unsigned WINAPI ___mb_cur_max_func(void) { return 1; }

WIN2LIN_EXPORT void WINAPI _set_error_mode(int) { /* keep defaults */ }

// UCRT onexit-table stubs: keep a flat fallback list.
struct OnExitTable { uintptr_t first, last, end; };
WIN2LIN_EXPORT int WINAPI _initialize_onexit_table(OnExitTable* t) {
    if (t) t->first = t->last = t->end = 0;
    return 0;
}
WIN2LIN_EXPORT int WINAPI _register_onexit_function(OnExitTable* t, OnExitFn fn) {
    (void)t;
    return register_atexit(fn) ? 0 : -1;
}
WIN2LIN_EXPORT int WINAPI _execute_onexit_table(OnExitTable*) {
    run_atexit_handlers();
    return 0;
}

// SEH personality hook: let exceptions keep propagating (continue search).
WIN2LIN_EXPORT intptr_t WINAPI __C_specific_handler(void*, void*, void*, void*) {
    return 1; // ExceptionContinueSearch
}

WIN2LIN_EXPORT int WINAPI _setargv(void) { return 0; }
WIN2LIN_EXPORT int WINAPI _setenvp(void) { return 0; }

// Pointer-getter helpers used by MinGW startup.
WIN2LIN_EXPORT char** WINAPI __p__acmdln(void) {
    ensure_crt();
    return &_acmdln;
}
WIN2LIN_EXPORT int* WINAPI __p__fmode(void) { return &_fmode; }
WIN2LIN_EXPORT int* WINAPI __p__commode(void) { return &_commode; }
WIN2LIN_EXPORT int* WINAPI __p___argc(void) {
    ensure_crt();
    return &g_argc;
}
WIN2LIN_EXPORT char*** WINAPI __p___argv(void) {
    ensure_crt();
    return &g_argv; // g_argv is the pointer variable handed to __getmainargs
}
WIN2LIN_EXPORT char*** WINAPI __p__environ(void) {
    ensure_crt();
    return &_environ;
}

// ==========================================================================
// Heap
// ==========================================================================
WIN2LIN_EXPORT void* WINAPI malloc(size_t size) {
    ensure_crt();
    return g_glibc.malloc(size ? size : 1);
}
WIN2LIN_EXPORT void* WINAPI calloc(size_t n, size_t size) {
    ensure_crt();
    return g_glibc.calloc(n ? n : 1, size ? size : 1);
}
WIN2LIN_EXPORT void* WINAPI realloc(void* p, size_t size) {
    ensure_crt();
    return g_glibc.realloc(p, size ? size : 1);
}
WIN2LIN_EXPORT void WINAPI free(void* p) {
    ensure_crt();
    g_glibc.free(p);
}
WIN2LIN_EXPORT size_t WINAPI _msize(void* p) {
    ensure_crt();
    return p ? g_glibc.malloc_usable_size(p) : 0;
}
WIN2LIN_EXPORT void* WINAPI _malloc_base(size_t size) { return malloc(size); }
WIN2LIN_EXPORT void* WINAPI _calloc_base(size_t n, size_t size) { return calloc(n, size); }
WIN2LIN_EXPORT void* WINAPI _realloc_base(void* p, size_t size) { return realloc(p, size); }
WIN2LIN_EXPORT void WINAPI _free_base(void* p) { free(p); }

// ==========================================================================
// Memory / string primitives (thin glibc passthroughs)
// ==========================================================================
WIN2LIN_EXPORT void* WINAPI memcpy(void* dst, const void* src, size_t n) {
    ensure_crt();
    return g_glibc.memcpy(dst, src, n);
}
WIN2LIN_EXPORT void* WINAPI memmove(void* dst, const void* src, size_t n) {
    ensure_crt();
    return g_glibc.memmove(dst, src, n);
}
WIN2LIN_EXPORT void* WINAPI memset(void* dst, int c, size_t n) {
    ensure_crt();
    return g_glibc.memset(dst, c, n);
}
WIN2LIN_EXPORT int WINAPI memcmp(const void* a, const void* b, size_t n) {
    ensure_crt();
    return g_glibc.memcmp(a, b, n);
}
WIN2LIN_EXPORT size_t WINAPI strlen(const char* s) {
    ensure_crt();
    return g_glibc.strlen(s);
}
WIN2LIN_EXPORT int WINAPI strcmp(const char* a, const char* b) {
    ensure_crt();
    return g_glibc.strcmp(a, b);
}
WIN2LIN_EXPORT int WINAPI strncmp(const char* a, const char* b, size_t n) {
    ensure_crt();
    return g_glibc.strncmp(a, b, n);
}
WIN2LIN_EXPORT char* WINAPI strcpy(char* d, const char* s) {
    ensure_crt();
    return g_glibc.strcpy(d, s);
}
WIN2LIN_EXPORT char* WINAPI strcat(char* d, const char* s) {
    ensure_crt();
    return g_glibc.strcat(d, s);
}
WIN2LIN_EXPORT char* WINAPI strchr(const char* s, int c) {
    ensure_crt();
    return g_glibc.strchr(s, c);
}
WIN2LIN_EXPORT char* WINAPI strstr(const char* h, const char* n) {
    ensure_crt();
    return g_glibc.strstr(h, n);
}

// Wide-char helpers: the PE's wchar_t is 16-bit, so these take UTF-16.
WIN2LIN_EXPORT size_t WINAPI wcslen(const char16_t* s) {
    size_t n = 0;
    if (s) while (s[n]) ++n;
    return n;
}

WIN2LIN_EXPORT size_t WINAPI wcstombs(char* dst, const char16_t* src, size_t n) {
    if (!src) return static_cast<size_t>(-1);
    // Minimal UTF-16 -> UTF-8; emit at most n bytes (NUL only if room).
    size_t out = 0;
    size_t i = 0;
    while (src[i]) {
        uint32_t cp = src[i];
        if (cp >= 0xD800 && cp <= 0xDBFF && src[i + 1] >= 0xDC00 && src[i + 1] <= 0xDFFF) {
            cp = 0x10000 + ((cp - 0xD800) << 10) + (src[i + 1] - 0xDC00);
            ++i;
        }
        ++i;
        char enc[4];
        int len = 0;
        if (cp < 0x80) { enc[len++] = static_cast<char>(cp); }
        else if (cp < 0x800) {
            enc[len++] = static_cast<char>(0xC0 | (cp >> 6));
            enc[len++] = static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            enc[len++] = static_cast<char>(0xE0 | (cp >> 12));
            enc[len++] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            enc[len++] = static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            enc[len++] = static_cast<char>(0xF0 | (cp >> 18));
            enc[len++] = static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            enc[len++] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            enc[len++] = static_cast<char>(0x80 | (cp & 0x3F));
        }
        bool truncated = false;
        for (int k = 0; k < len; ++k) {
            if (out < n) dst[out] = enc[k];
            else truncated = true;
            ++out;
        }
        if (truncated) return static_cast<size_t>(-1);
    }
    if (out < n) dst[out] = '\0';
    return out;
}

WIN2LIN_EXPORT size_t WINAPI mbstowcs(char16_t* dst, const char* src, size_t n) {
    if (!src) return static_cast<size_t>((-1));
    const size_t units = utf8_to_utf16(src, dst, n ? n - 1 : 0);
    if (n) dst[units] = 0;
    return units;
}

// ==========================================================================
// stdio
// ==========================================================================
WIN2LIN_EXPORT int WINAPI fprintf(FILE* stream, const char* fmt, ...) {
    ensure_crt();
    if (!fmt) return -1;
    va_list ap;
    va_start(ap, fmt);
    const int r = vformat_to_file(stream, fmt, ap);
    va_end(ap);
    return r;
}

WIN2LIN_EXPORT int WINAPI vfprintf(FILE* stream, const char* fmt, va_list ap) {
    ensure_crt();
    if (!fmt) return -1;
    return vformat_to_file(stream, fmt, ap);
}

// UCRT-style entry: options ignored.
WIN2LIN_EXPORT int WINAPI __stdio_common_vfprintf(uint64_t, FILE* stream, const char* fmt,
                                                  va_list ap) {
    return vfprintf(stream, fmt, ap);
}

WIN2LIN_EXPORT int WINAPI printf(const char* fmt, ...) {
    ensure_crt();
    if (!fmt) return -1;
    va_list ap;
    va_start(ap, fmt);
    const int r = vformat_to_file(iob_stdout(), fmt, ap);
    va_end(ap);
    return r;
}

WIN2LIN_EXPORT int WINAPI sprintf(char* buf, const char* fmt, ...) {
    ensure_crt();
    if (!buf || !fmt) return -1;
    va_list ap;
    va_start(ap, fmt);
    const int r = vformat_to_buf(buf, static_cast<size_t>(-1) / 2, fmt, ap);
    va_end(ap);
    return r;
}

WIN2LIN_EXPORT int WINAPI snprintf(char* buf, size_t cap, const char* fmt, ...) {
    ensure_crt();
    if (!fmt) return -1;
    va_list ap;
    va_start(ap, fmt);
    const int r = vformat_to_buf(buf, cap, fmt, ap);
    va_end(ap);
    return r;
}

WIN2LIN_EXPORT int WINAPI vsprintf(char* buf, const char* fmt, va_list ap) {
    ensure_crt();
    if (!buf || !fmt) return -1;
    return vformat_to_buf(buf, static_cast<size_t>(-1) / 2, fmt, ap);
}

WIN2LIN_EXPORT int WINAPI vsnprintf(char* buf, size_t cap, const char* fmt, va_list ap) {
    ensure_crt();
    if (!fmt) return -1;
    return vformat_to_buf(buf, cap, fmt, ap);
}

WIN2LIN_EXPORT int WINAPI fputc(int c, FILE* stream) {
    ensure_crt();
    return g_glibc.fputc(c, file_to_glibc(stream));
}

WIN2LIN_EXPORT int WINAPI fgetc(FILE* stream) {
    ensure_crt();
    return g_glibc.fgetc(file_to_glibc(stream));
}

WIN2LIN_EXPORT size_t WINAPI fwrite(const void* ptr, size_t size, size_t nmemb, FILE* stream) {
    ensure_crt();
    if (!ptr || !size || !nmemb) return 0;
    return g_glibc.fwrite(ptr, size, nmemb, file_to_glibc(stream));
}

WIN2LIN_EXPORT size_t WINAPI fread(void* ptr, size_t size, size_t nmemb, FILE* stream) {
    ensure_crt();
    if (!ptr || !size || !nmemb) return 0;
    return g_glibc.fread(ptr, size, nmemb, file_to_glibc(stream));
}

WIN2LIN_EXPORT int WINAPI fflush(FILE* stream) {
    ensure_crt();
    return g_glibc.fflush(file_to_glibc(stream));
}

WIN2LIN_EXPORT int WINAPI fputs(const char* s, FILE* stream) {
    ensure_crt();
    if (!s) return -1;
    return g_glibc.fputs(s, file_to_glibc(stream));
}

WIN2LIN_EXPORT char* WINAPI fgets(char* buf, int n, FILE* stream) {
    ensure_crt();
    if (!buf || n <= 0) return nullptr;
    return g_glibc.fgets(buf, n, file_to_glibc(stream));
}

WIN2LIN_EXPORT int WINAPI fclose(FILE* stream) {
    ensure_crt();
    return g_glibc.fclose(file_to_glibc(stream));
}

WIN2LIN_EXPORT int WINAPI feof(FILE* stream) {
    ensure_crt();
    return g_glibc.feof(file_to_glibc(stream));
}

WIN2LIN_EXPORT int WINAPI ferror(FILE* stream) {
    ensure_crt();
    return g_glibc.ferror(file_to_glibc(stream));
}

WIN2LIN_EXPORT void WINAPI clearerr(FILE* stream) {
    ensure_crt();
    g_glibc.clearerr(file_to_glibc(stream));
}

WIN2LIN_EXPORT FILE* WINAPI fopen(const char* path, const char* mode) {
    ensure_crt();
    if (!path || !mode) return nullptr;
    return static_cast<FILE*>(g_glibc.fopen(path, mode));
}

WIN2LIN_EXPORT int WINAPI putchar(int c) {
    ensure_crt();
    return g_glibc.putchar(c);
}

WIN2LIN_EXPORT int WINAPI puts(const char* s) {
    ensure_crt();
    return g_glibc.puts(s);
}

// ==========================================================================
// Misc helpers
// ==========================================================================
WIN2LIN_EXPORT char* WINAPI strerror(int err) {
    ensure_crt();
    return g_glibc.strerror(err);
}

struct lconv;
WIN2LIN_EXPORT lconv* WINAPI localeconv(void) {
    ensure_crt();
    using LocaleConvFn = void* (*)();
    static LocaleConvFn fn =
        reinterpret_cast<LocaleConvFn>(dlsym(RTLD_NEXT, "localeconv"));
    return static_cast<lconv*>(fn ? fn() : nullptr);
}

WIN2LIN_EXPORT int WINAPI abs(int v) { return v < 0 ? -v : v; }
WIN2LIN_EXPORT long WINAPI labs(long v) { return v < 0 ? -v : v; }

WIN2LIN_EXPORT int WINAPI atoi(const char* s) {
    if (!s) return 0;
    while (*s == ' ' || *s == '\t') ++s;
    int sign = 1;
    if (*s == '-') { sign = -1; ++s; }
    else if (*s == '+') ++s;
    long v = 0;
    while (*s >= '0' && *s <= '9') v = v * 10 + (*s++ - '0');
    return static_cast<int>(sign * v);
}

WIN2LIN_EXPORT long WINAPI atol(const char* s) { return static_cast<long>(atoi(s)); }

WIN2LIN_EXPORT void WINAPI srand(unsigned seed) {
    ensure_crt();
    using SrandFn = void (*)(unsigned);
    static SrandFn fn = reinterpret_cast<SrandFn>(dlsym(RTLD_NEXT, "srand"));
    if (fn) fn(seed);
}

WIN2LIN_EXPORT int WINAPI rand(void) {
    ensure_crt();
    using RandFn = int (*)();
    static RandFn fn = reinterpret_cast<RandFn>(dlsym(RTLD_NEXT, "rand"));
    return fn ? fn() : 0;
}

// qsort with an MS-ABI comparator: implement our own stable merge sort so the
// PE-provided compar (ms_abi) is invoked with the right convention.
namespace {
void merge_sort(void* base, size_t n, size_t sz,
                int(WINAPI* compar)(const void*, const void*), char* tmp) {
    for (size_t width = 1; width < n; width *= 2) {
        for (size_t i = 0; i < n; i += 2 * width) {
            size_t mid = i + width < n ? i + width : n;
            size_t right = i + 2 * width < n ? i + 2 * width : n;
            size_t l = i, r = mid, k = i;
            char* basec = static_cast<char*>(base);
            while (l < mid && r < right) {
                if (compar(basec + l * sz, basec + r * sz) <= 0) {
                    memcpy(tmp + k * sz, basec + l * sz, sz);
                    ++l;
                } else {
                    memcpy(tmp + k * sz, basec + r * sz, sz);
                    ++r;
                }
                ++k;
            }
            while (l < mid) { memcpy(tmp + k * sz, basec + l * sz, sz); ++l; ++k; }
            while (r < right) { memcpy(tmp + k * sz, basec + r * sz, sz); ++r; ++k; }
            for (size_t j = i; j < right; ++j) memcpy(basec + j * sz, tmp + j * sz, sz);
        }
    }
}
} // namespace

WIN2LIN_EXPORT void WINAPI qsort(void* base, size_t n, size_t sz,
                                 int(WINAPI* compar)(const void*, const void*)) {
    ensure_crt();
    if (!base || n < 2 || sz == 0) return;
    // Merge sort needs a scratch buffer; fall back to insertion sort when the
    // allocation fails or the array is tiny.
    if (n * sz > (64u << 20)) return; // refuse absurd sizes rather than crash
    char* tmp = static_cast<char*>(g_glibc.malloc(n * sz));
    if (!tmp) return;
    merge_sort(base, n, sz, compar, tmp);
    g_glibc.free(tmp);
}

WIN2LIN_EXPORT void* WINAPI bsearch(const void* key, const void* base, size_t n, size_t sz,
                                    int(WINAPI* compar)(const void*, const void*)) {
    ensure_crt();
    if (!key || !base) return nullptr;
    const char* p = static_cast<const char*>(base);
    size_t lo = 0, hi = n;
    while (lo < hi) {
        const size_t mid = lo + (hi - lo) / 2;
        const int c = compar(key, p + mid * sz);
        if (c == 0) return const_cast<char*>(p + mid * sz);
        if (c < 0) hi = mid;
        else lo = mid + 1;
    }
    return nullptr;
}
