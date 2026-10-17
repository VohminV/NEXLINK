// libwin_user32.so - mock windowing shim (Phase 1.5).
//
// Real window creation (X11/Wayland) is deferred to a later phase: right now
// the goal is to let a standard Win32 application run past window setup and
// reach graphics initialization. Every function therefore returns success
// with plausible mock values:
//
//   RegisterClassExW -> monotonically increasing ATOM
//   CreateWindowExW  -> shared mock HWND (0xDEADBEEF) backed by a window
//                       record (size, style, WndProc) so GetClientRect /
//                       GetWindowRect / DispatchMessageW behave sanely
//   ShowWindow/UpdateWindow/SetForegroundWindow -> TRUE
//   GetClientRect/GetWindowRect -> requested size, clamped to 1920x1080
//
// Anything not yet modelled logs "[NEXLINK USER32 UNIMPL] <name>" and returns
// a safe default (0 / FALSE / S_OK-equivalent) so the app keeps running.

#include "common/win32.hpp"

#include <cstdio>
#include <cstring>

#include <map>
#include <mutex>
#include <string>

using namespace win2lin::win;

namespace {

// --------------------------------------------------------------------------
// Logging / mock constants
// --------------------------------------------------------------------------
const HWND MOCK_HWND = reinterpret_cast<HWND>(static_cast<uintptr_t>(0xDEADBEEF));
const HWND MOCK_DESKTOP_HWND = reinterpret_cast<HWND>(static_cast<uintptr_t>(0xDEAD0000));
const HDC MOCK_HDC = reinterpret_cast<HDC>(static_cast<uintptr_t>(0xD1));
const HCURSOR MOCK_HCURSOR = reinterpret_cast<HCURSOR>(static_cast<uintptr_t>(0xC1));
const HICON MOCK_HICON = reinterpret_cast<HICON>(static_cast<uintptr_t>(0xC2));
constexpr LONG MOCK_SCREEN_W = 1920;
constexpr LONG MOCK_SCREEN_H = 1080;

#define USER32_UNIMPL(name)                                                    \
    fprintf(stderr, "[NEXLINK USER32 UNIMPL] %s\n", name)

#define USER32_INFO(fmt, ...) fprintf(stderr, "[NEXLINK USER32] " fmt "\n", __VA_ARGS__)

// --------------------------------------------------------------------------
// Window / class registries. The mock world has one HWND shared by all
// CreateWindowExW calls; per-window records still track size and the WndProc
// so DispatchMessageW can invoke application code.
// --------------------------------------------------------------------------
struct WindowClass {
    WNDPROC wndproc = nullptr;
    std::u16string name;
};

struct WindowInfo {
    LONG x = 0, y = 0;
    LONG width = MOCK_SCREEN_W, height = MOCK_SCREEN_H;
    UINT style = 0;
    bool visible = false;
    WNDPROC wndproc = nullptr;
};

std::mutex g_mutex;
std::map<ATOM, WindowClass> g_classes;
std::map<std::u16string, ATOM> g_class_names;
WindowInfo g_window;
unsigned g_next_atom = 1;

std::u16string utf8_to_utf16(const char* s) {
    std::u16string w;
    if (!s) return w;
    // Minimal UTF-8 -> UTF-16 (ASCII + 2/3-byte sequences suffice for mock
    // class names; unknown bytes become '?' rather than failing).
    for (size_t i = 0; s[i];) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        if (c < 0x80) {
            w.push_back(static_cast<char16_t>(c));
            ++i;
        } else if ((c & 0xE0) == 0xC0 && (s[i + 1] & 0xC0) == 0x80) {
            const uint32_t cp = ((c & 0x1Fu) << 6) | (s[i + 1] & 0x3Fu);
            w.push_back(static_cast<char16_t>(cp));
            i += 2;
        } else if ((c & 0xF0) == 0xE0 && (s[i + 1] & 0xC0) == 0x80 &&
                   (s[i + 2] & 0xC0) == 0x80) {
            const uint32_t cp =
                ((c & 0x0Fu) << 12) | ((s[i + 1] & 0x3Fu) << 6) | (s[i + 2] & 0x3Fu);
            w.push_back(static_cast<char16_t>(cp));
            i += 3;
        } else {
            w.push_back(u'?');
            ++i;
        }
    }
    return w;
}

std::string utf16_to_utf8(const WCHAR* w) {
    std::string out;
    if (!w) return out;
    for (size_t i = 0; w[i]; ++i) {
        const uint32_t cp = w[i];
        if (cp < 0x80) {
            out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }
    return out;
}

// MAKEINTRESOURCEW ids are small integers cast to pointers.
bool is_int_resource(LPCWSTR s) {
    return reinterpret_cast<uintptr_t>(s) < 0x10000;
}

ATOM register_class(WNDPROC wndproc, const std::u16string& name) {
    std::lock_guard<std::mutex> lk(g_mutex);
    auto it = g_class_names.find(name);
    if (it != g_class_names.end()) return it->second; // re-registration: same ATOM
    const ATOM atom = static_cast<ATOM>(g_next_atom++);
    g_classes[atom] = {wndproc, name};
    g_class_names[name] = atom;
    return atom;
}

} // namespace

// ==========================================================================
// Classes
// ==========================================================================
WIN2LIN_EXPORT ATOM WINAPI RegisterClassExW(const WNDCLASSEXW* wc) {
    if (!wc || !wc->lpszClassName) return 0;
    const std::u16string name = is_int_resource(wc->lpszClassName)
                                    ? std::u16string(1, static_cast<char16_t>(
                                             reinterpret_cast<uintptr_t>(wc->lpszClassName)))
                                    : std::u16string(wc->lpszClassName);
    const ATOM atom = register_class(wc->lpfnWndProc, name);
    USER32_INFO("RegisterClassExW class='%s' -> ATOM %u", utf16_to_utf8(name.c_str()).c_str(),
                static_cast<unsigned>(atom));
    return atom;
}

WIN2LIN_EXPORT ATOM WINAPI RegisterClassW(const WNDCLASSEXW* wc) {
    // MinGW headers route RegisterClassW through the same layout minus the
    // extra fields; reuse RegisterClassExW (cbSize field may be absent, but
    // lpfnWndProc/lpszClassName share the offsets).
    return RegisterClassExW(wc);
}

WIN2LIN_EXPORT ATOM WINAPI RegisterClassExA(const void* wc) {
    // Layout-compatible with WNDCLASSEXW except char* strings; extract the
    // pieces we need without pulling in the A-struct definition.
    struct WNDCLASSEXA_LOCAL {
        UINT cbSize;
        UINT style;
        WNDPROC lpfnWndProc;
        int cbClsExtra;
        int cbWndExtra;
        HINSTANCE hInstance;
        HICON hIcon;
        HCURSOR hCursor;
        HBRUSH hbrBackground;
        LPCSTR lpszMenuName;
        LPCSTR lpszClassName;
        HICON hIconSm;
    };
    if (!wc) return 0;
    const auto* a = static_cast<const WNDCLASSEXA_LOCAL*>(wc);
    const std::u16string name = utf8_to_utf16(a->lpszClassName);
    const ATOM atom = register_class(a->lpfnWndProc, name);
    USER32_INFO("RegisterClassExA -> ATOM %u", static_cast<unsigned>(atom));
    return atom;
}

WIN2LIN_EXPORT ATOM WINAPI RegisterClassA(const void* wc) { return RegisterClassExA(wc); }

WIN2LIN_EXPORT BOOL WINAPI UnregisterClassW(LPCWSTR name, HINSTANCE) {
    if (!name || is_int_resource(name)) return 1;
    std::lock_guard<std::mutex> lk(g_mutex);
    const auto it = g_class_names.find(std::u16string(name));
    if (it == g_class_names.end()) return 1;
    g_classes.erase(it->second);
    g_class_names.erase(it);
    return 1;
}

WIN2LIN_EXPORT BOOL WINAPI UnregisterClassA(LPCSTR name, HINSTANCE inst) {
    const std::u16string w = utf8_to_utf16(name);
    return UnregisterClassW(reinterpret_cast<LPCWSTR>(w.c_str()), inst);
}

// ==========================================================================
// Windows
// ==========================================================================
WIN2LIN_EXPORT HWND WINAPI CreateWindowExW(DWORD ex_style, LPCWSTR class_name,
                                           LPCWSTR window_name, DWORD style, int x, int y,
                                           int width, int height, HWND parent, HMENU menu,
                                           HINSTANCE instance, LPVOID param) {
    (void)ex_style; (void)parent; (void)menu; (void)instance; (void)param;

    WNDPROC wndproc = nullptr;
    {
        std::lock_guard<std::mutex> lk(g_mutex);
        if (class_name && !is_int_resource(class_name)) {
            auto it = g_class_names.find(std::u16string(class_name));
            if (it != g_class_names.end()) wndproc = g_classes[it->second].wndproc;
        }
        g_window.x = (x == static_cast<int>(0x80000000)) ? 0 : x; // CW_USEDEFAULT
        g_window.y = (y == static_cast<int>(0x80000000)) ? 0 : y;
        g_window.width = (width <= 0 || width == static_cast<int>(0x80000000))
                             ? MOCK_SCREEN_W
                             : width;
        g_window.height = (height <= 0 || height == static_cast<int>(0x80000000))
                              ? MOCK_SCREEN_H
                              : height;
        g_window.style = style;
        g_window.visible = false;
        g_window.wndproc = wndproc;
    }
    USER32_INFO("CreateWindowExW class='%s' title='%s' -> HWND %p",
                class_name && !is_int_resource(class_name)
                    ? utf16_to_utf8(class_name).c_str()
                    : "(resource)",
                window_name ? utf16_to_utf8(window_name).c_str() : "",
                static_cast<void*>(MOCK_HWND));
    return MOCK_HWND;
}

WIN2LIN_EXPORT HWND WINAPI CreateWindowExA(DWORD ex, LPCSTR class_name, LPCSTR window_name,
                                           DWORD style, int x, int y, int w, int h,
                                           HWND parent, HMENU menu, HINSTANCE inst,
                                           LPVOID param) {
    const std::u16string cls = utf8_to_utf16(class_name);
    const std::u16string title = utf8_to_utf16(window_name);
    return CreateWindowExW(ex, reinterpret_cast<LPCWSTR>(cls.c_str()),
                           reinterpret_cast<LPCWSTR>(title.c_str()), style, x, y, w, h,
                           parent, menu, inst, param);
}

WIN2LIN_EXPORT LRESULT WINAPI DefWindowProcW(HWND, UINT, WPARAM, LPARAM) {
    return 0;
}

WIN2LIN_EXPORT LRESULT WINAPI DefWindowProcA(HWND h, UINT m, WPARAM w, LPARAM l) {
    return DefWindowProcW(h, m, w, l);
}

WIN2LIN_EXPORT BOOL WINAPI DestroyWindow(HWND hwnd) {
    if (hwnd != MOCK_HWND) return 0;
    std::lock_guard<std::mutex> lk(g_mutex);
    g_window.visible = false;
    return 1;
}

WIN2LIN_EXPORT BOOL WINAPI ShowWindow(HWND hwnd, int cmd) {
    if (hwnd != MOCK_HWND) return 0;
    std::lock_guard<std::mutex> lk(g_mutex);
    g_window.visible = (cmd != 0 /* SW_HIDE */);
    USER32_INFO("ShowWindow cmd=%d", cmd);
    return 1; // previous visibility was "hidden"
}

WIN2LIN_EXPORT BOOL WINAPI UpdateWindow(HWND hwnd) {
    return hwnd == MOCK_HWND ? 1 : 0;
}

WIN2LIN_EXPORT BOOL WINAPI SetForegroundWindow(HWND hwnd) {
    return hwnd == MOCK_HWND ? 1 : 0;
}

WIN2LIN_EXPORT BOOL WINAPI GetClientRect(HWND hwnd, RECT* rc) {
    if (hwnd != MOCK_HWND || !rc) return 0;
    std::lock_guard<std::mutex> lk(g_mutex);
    rc->left = 0;
    rc->top = 0;
    rc->right = g_window.width;
    rc->bottom = g_window.height;
    return 1;
}

WIN2LIN_EXPORT BOOL WINAPI GetWindowRect(HWND hwnd, RECT* rc) {
    if (hwnd != MOCK_HWND || !rc) return 0;
    std::lock_guard<std::mutex> lk(g_mutex);
    // Mock non-client frame: 8px border + 32px title bar.
    rc->left = g_window.x - 8;
    rc->top = g_window.y - 32;
    rc->right = g_window.x + g_window.width + 8;
    rc->bottom = g_window.y + g_window.height + 8;
    return 1;
}

WIN2LIN_EXPORT BOOL WINAPI MoveWindow(HWND hwnd, int x, int y, int w, int h, BOOL) {
    if (hwnd != MOCK_HWND) return 0;
    std::lock_guard<std::mutex> lk(g_mutex);
    g_window.x = x;
    g_window.y = y;
    g_window.width = w;
    g_window.height = h;
    return 1;
}

WIN2LIN_EXPORT BOOL WINAPI SetWindowPos(HWND hwnd, HWND, int x, int y, int w, int h,
                                        UINT flags) {
    (void)flags;
    if (hwnd != MOCK_HWND) return 0;
    std::lock_guard<std::mutex> lk(g_mutex);
    if (x != static_cast<int>(0x80000000)) g_window.x = x;
    if (y != static_cast<int>(0x80000000)) g_window.y = y;
    if (w > 0) g_window.width = w;
    if (h > 0) g_window.height = h;
    return 1;
}

WIN2LIN_EXPORT BOOL WINAPI AdjustWindowRectEx(RECT* rc, DWORD, BOOL) {
    if (!rc) return 0;
    // Pretend the caller already asked for client coordinates; grow by the
    // same mock frame GetWindowRect reports.
    rc->left -= 8;
    rc->top -= 32;
    rc->right += 8;
    rc->bottom += 8;
    return 1;
}

WIN2LIN_EXPORT BOOL WINAPI IsWindow(HWND hwnd) { return hwnd == MOCK_HWND; }
WIN2LIN_EXPORT BOOL WINAPI IsWindowVisible(HWND hwnd) {
    if (hwnd != MOCK_HWND) return 0;
    std::lock_guard<std::mutex> lk(g_mutex);
    return g_window.visible ? 1 : 0;
}
WIN2LIN_EXPORT BOOL WINAPI IsIconic(HWND) { return 0; }
WIN2LIN_EXPORT BOOL WINAPI OpenIcon(HWND) { return 1; }
WIN2LIN_EXPORT BOOL WINAPI EnableWindow(HWND, BOOL) { return 1; }

WIN2LIN_EXPORT HWND WINAPI GetDesktopWindow(void) { return MOCK_DESKTOP_HWND; }
WIN2LIN_EXPORT HWND WINAPI GetForegroundWindow(void) { return MOCK_HWND; }
WIN2LIN_EXPORT HWND WINAPI GetParent(HWND) { return nullptr; }
WIN2LIN_EXPORT HWND WINAPI SetParent(HWND, HWND) { return nullptr; }
WIN2LIN_EXPORT HWND WINAPI GetWindow(HWND, UINT) { return nullptr; }

WIN2LIN_EXPORT BOOL WINAPI ClientToScreen(HWND hwnd, POINT* pt) {
    if (hwnd != MOCK_HWND || !pt) return 0;
    std::lock_guard<std::mutex> lk(g_mutex);
    pt->x += g_window.x;
    pt->y += g_window.y;
    return 1;
}

WIN2LIN_EXPORT BOOL WINAPI ScreenToClient(HWND hwnd, POINT* pt) {
    if (hwnd != MOCK_HWND || !pt) return 0;
    std::lock_guard<std::mutex> lk(g_mutex);
    pt->x -= g_window.x;
    pt->y -= g_window.y;
    return 1;
}

WIN2LIN_EXPORT BOOL WINAPI SetWindowTextW(HWND hwnd, LPCWSTR text) {
    if (hwnd != MOCK_HWND) return 0;
    (void)text;
    return 1;
}

WIN2LIN_EXPORT BOOL WINAPI SetWindowTextA(HWND hwnd, LPCSTR text) {
    if (hwnd != MOCK_HWND) return 0;
    (void)text;
    return 1;
}

WIN2LIN_EXPORT int WINAPI GetWindowTextW(HWND, LPWSTR buf, int max_count) {
    if (buf && max_count > 0) buf[0] = 0;
    return 0;
}

WIN2LIN_EXPORT int WINAPI GetWindowTextA(HWND, LPSTR buf, int max_count) {
    if (buf && max_count > 0) buf[0] = 0;
    return 0;
}

WIN2LIN_EXPORT BOOL WINAPI BringWindowToTop(HWND hwnd) { return hwnd == MOCK_HWND; }

// ==========================================================================
// Cursors / icons / DCs
// ==========================================================================
WIN2LIN_EXPORT HCURSOR WINAPI LoadCursorW(HINSTANCE inst, LPCWSTR name) {
    (void)inst;
    // Integer resources (IDC_ARROW=32512 etc.) and named cursors both resolve
    // to the same mock handle.
    return MOCK_HCURSOR;
}

WIN2LIN_EXPORT HCURSOR WINAPI LoadCursorA(HINSTANCE inst, LPCSTR name) {
    (void)inst; (void)name;
    return MOCK_HCURSOR;
}

WIN2LIN_EXPORT HICON WINAPI LoadIconW(HINSTANCE inst, LPCWSTR name) {
    (void)inst; (void)name;
    return MOCK_HICON;
}

WIN2LIN_EXPORT HICON WINAPI LoadIconA(HINSTANCE inst, LPCSTR name) {
    (void)inst; (void)name;
    return MOCK_HICON;
}

WIN2LIN_EXPORT HDC WINAPI GetDC(HWND hwnd) {
    if (hwnd != MOCK_HWND && hwnd != MOCK_DESKTOP_HWND) return nullptr;
    return MOCK_HDC;
}

WIN2LIN_EXPORT int WINAPI ReleaseDC(HWND, HDC) { return 1; }

WIN2LIN_EXPORT HDC WINAPI BeginPaint(HWND hwnd, void* ps) {
    if (ps) std::memset(ps, 0, 64); // PAINTSTRUCT is larger; zero a safe prefix
    return hwnd == MOCK_HWND ? MOCK_HDC : nullptr;
}

WIN2LIN_EXPORT BOOL WINAPI EndPaint(HWND, const void*) { return 1; }

WIN2LIN_EXPORT BOOL WINAPI InvalidateRect(HWND hwnd, const RECT*, BOOL) {
    return hwnd == MOCK_HWND ? 1 : 0;
}

// ==========================================================================
// Message loop
// ==========================================================================
WIN2LIN_EXPORT BOOL WINAPI PeekMessageW(MSG* msg, HWND hwnd, UINT min, UINT max,
                                        UINT remove) {
    (void)hwnd; (void)min; (void)max; (void)remove;
    if (msg) std::memset(msg, 0, sizeof(MSG));
    return 0; // no pending messages in the mock world
}

WIN2LIN_EXPORT BOOL WINAPI PeekMessageA(MSG* msg, HWND hwnd, UINT min, UINT max,
                                        UINT remove) {
    return PeekMessageW(msg, hwnd, min, max, remove);
}

WIN2LIN_EXPORT BOOL WINAPI GetMessageW(MSG* msg, HWND hwnd, UINT min, UINT max) {
    (void)hwnd; (void)min; (void)max;
    if (msg) {
        std::memset(msg, 0, sizeof(MSG));
        msg->message = 0x0012; // WM_QUIT: loop terminates immediately
    }
    return 0; // FALSE == WM_QUIT received
}

WIN2LIN_EXPORT BOOL WINAPI GetMessageA(MSG* msg, HWND hwnd, UINT min, UINT max) {
    return GetMessageW(msg, hwnd, min, max);
}

WIN2LIN_EXPORT BOOL WINAPI TranslateMessage(const MSG*) { return 1; }

WIN2LIN_EXPORT LRESULT WINAPI DispatchMessageW(const MSG* msg) {
    if (!msg) return 0;
    WNDPROC wndproc = nullptr;
    {
        std::lock_guard<std::mutex> lk(g_mutex);
        wndproc = g_window.wndproc;
    }
    // Route through the registered WndProc so application handlers run.
    if (wndproc) return wndproc(msg->hwnd, msg->message, msg->wParam, msg->lParam);
    return DefWindowProcW(msg->hwnd, msg->message, msg->wParam, msg->lParam);
}

WIN2LIN_EXPORT LRESULT WINAPI DispatchMessageA(const MSG* msg) {
    return DispatchMessageW(msg);
}

WIN2LIN_EXPORT void WINAPI PostQuitMessage(int code) {
    USER32_INFO("PostQuitMessage(%d)", code);
}

WIN2LIN_EXPORT BOOL WINAPI PostMessageW(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    (void)hwnd; (void)msg; (void)w; (void)l;
    return 1;
}

WIN2LIN_EXPORT BOOL WINAPI PostMessageA(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    return PostMessageW(hwnd, msg, w, l);
}

WIN2LIN_EXPORT LRESULT WINAPI SendMessageW(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    WNDPROC wndproc = nullptr;
    {
        std::lock_guard<std::mutex> lk(g_mutex);
        wndproc = g_window.wndproc;
    }
    if (wndproc && hwnd == MOCK_HWND) return wndproc(hwnd, msg, w, l);
    return 0;
}

WIN2LIN_EXPORT LRESULT WINAPI SendMessageA(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    return SendMessageW(hwnd, msg, w, l);
}

WIN2LIN_EXPORT UINT_PTR WINAPI SetTimer(HWND, UINT_PTR id, UINT, void*) {
    return id ? id : 1;
}

WIN2LIN_EXPORT BOOL WINAPI KillTimer(HWND, UINT_PTR) { return 1; }

// ==========================================================================
// Misc
// ==========================================================================
WIN2LIN_EXPORT int WINAPI GetSystemMetrics(int index) {
    if (index == 0) return static_cast<int>(MOCK_SCREEN_W); // SM_CXSCREEN
    if (index == 1) return static_cast<int>(MOCK_SCREEN_H); // SM_CYSCREEN
    return 0;
}

WIN2LIN_EXPORT int WINAPI MessageBoxW(HWND, LPCWSTR text, LPCWSTR caption, UINT type) {
    (void)type;
    USER32_INFO("MessageBoxW caption='%s' text='%s'", caption ? utf16_to_utf8(caption).c_str() : "",
                text ? utf16_to_utf8(text).c_str() : "");
    return 1; // IDOK
}

WIN2LIN_EXPORT int WINAPI MessageBoxA(HWND, LPCSTR text, LPCSTR caption, UINT type) {
    (void)type;
    USER32_INFO("MessageBoxA caption='%s' text='%s'", caption ? caption : "",
                text ? text : "");
    return 1; // IDOK
}

WIN2LIN_EXPORT BOOL WINAPI SystemParametersInfoW(UINT action, UINT, PVOID pv, UINT) {
    if (action == 0x0030 /* SPI_GETWORKAREA */ && pv) {
        RECT* rc = static_cast<RECT*>(pv);
        rc->left = 0;
        rc->top = 0;
        rc->right = MOCK_SCREEN_W;
        rc->bottom = MOCK_SCREEN_H;
    }
    return 1;
}

WIN2LIN_EXPORT BOOL WINAPI SystemParametersInfoA(UINT action, UINT ui, PVOID pv, UINT f) {
    return SystemParametersInfoW(action, ui, pv, f);
}

WIN2LIN_EXPORT short WINAPI GetKeyState(int) { return 0; }
WIN2LIN_EXPORT short WINAPI GetAsyncKeyState(int) { return 0; }

WIN2LIN_EXPORT BOOL WINAPI RegisterHotKey(HWND, int, UINT, UINT) {
    USER32_UNIMPL("RegisterHotKey");
    return 0;
}

WIN2LIN_EXPORT BOOL WINAPI UnregisterHotKey(HWND, int) {
    USER32_UNIMPL("UnregisterHotKey");
    return 1;
}
