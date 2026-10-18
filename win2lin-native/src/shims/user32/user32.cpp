// libwin_user32.so - Win32 windowing shim backed by SDL2 (Phase 4).
//
// CreateWindowExW creates a real SDL_Window (SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN)
// and hands back a synthetic HWND (heap-allocated WindowInfo*).  The HWND<->SDL_Window
// registry is exported via win2lin_user32_get_sdl_window() (SysV ABI) so the D3D11
// shim can call SDL_Vulkan_CreateSurface / SDL_Vulkan_GetInstanceExtensions.
//
// If SDL initialisation or window creation fails (headless CI, no display),
// the shim falls back to the Phase 1.5 mock (no SDL_Window, synthetic sizes).
// GetClientRect / GetWindowRect / message-pump stubs all work in both modes.
//
// Message pump: PeekMessageW / GetMessageW translate SDL events (SDL_QUIT ->
// WM_QUIT, SDL_KEYDOWN/UP -> WM_KEYDOWN/UP, SDL_MOUSEMOTION -> WM_MOUSEMOVE, ...)
// into Win32 MSG structures so a real application's message loop terminates
// cleanly when the window is closed.

#include "common/win32.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#include <chrono>
#include <cstdio>
#include <cstring>

#include <map>
#include <mutex>
#include <string>
#include <vector>

using namespace win2lin::win;
using LPPOINT = POINT*;

namespace {

// --------------------------------------------------------------------------
// Logging
// --------------------------------------------------------------------------
#define USER32_UNIMPL(name)                                                    \
    fprintf(stderr, "[NEXLINK USER32 UNIMPL] %s\n", name)

#define USER32_INFO(fmt, ...) fprintf(stderr, "[NEXLINK USER32] " fmt "\n", ##__VA_ARGS__)

constexpr LONG MOCK_SCREEN_W = 1920;
constexpr LONG MOCK_SCREEN_H = 1080;

// --------------------------------------------------------------------------
// Window / class registries.
// --------------------------------------------------------------------------
struct WindowClass {
    WNDPROC wndproc = nullptr;
    std::u16string name;
};

struct WindowInfo {
    void* sdl_window = nullptr; // SDL_Window* (nullptr in mock/headless mode)
    LONG x = 0, y = 0;
    LONG width = MOCK_SCREEN_W, height = MOCK_SCREEN_H;
    UINT style = 0;
    bool visible = false;
    WNDPROC wndproc = nullptr;
};

std::mutex g_mutex;
std::map<ATOM, WindowClass> g_classes;
std::map<std::u16string, ATOM> g_class_names;
std::map<HWND, WindowInfo> g_windows;
unsigned g_next_atom = 1;
bool g_sdl_inited = false;
std::vector<MSG> g_msg_queue; // translated SDL events waiting for GetMessageW

// Synthetic HWNDs: opaque heap pointers into WindowInfo values.
// (The map key IS the HWND; we allocate a dummy byte so each HWND is unique.)

HWND make_hwnd() {
    // Each WindowInfo is stored by value in the map; the key is a unique
    // heap pointer that never aliases.
    static char sink;
    (void)sink;
    auto* key = new char; // unique address
    return reinterpret_cast<HWND>(key);
}

// --------------------------------------------------------------------------
// UTF helpers (unchanged from Phase 1.5).
// --------------------------------------------------------------------------
std::u16string utf8_to_utf16(const char* s) {
    std::u16string w;
    if (!s) return w;
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

bool is_int_resource(LPCWSTR s) {
    return reinterpret_cast<uintptr_t>(s) < 0x10000;
}

ATOM register_class(WNDPROC wndproc, const std::u16string& name) {
    std::lock_guard<std::mutex> lk(g_mutex);
    auto it = g_class_names.find(name);
    if (it != g_class_names.end()) return it->second;
    const ATOM atom = static_cast<ATOM>(g_next_atom++);
    g_classes[atom] = {wndproc, name};
    g_class_names[name] = atom;
    return atom;
}

bool ensure_sdl() {
    if (g_sdl_inited) return true;
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        USER32_INFO("SDL_InitSubSystem(VIDEO) failed: %s", SDL_GetError());
        return false;
    }
    g_sdl_inited = true;
    USER32_INFO("SDL2 video subsystem initialised");
    return true;
}

// --------------------------------------------------------------------------
// SDL event -> Win32 MSG translation.
// --------------------------------------------------------------------------
void translate_and_queue_events() {
    if (!g_sdl_inited) return;
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        MSG msg{};
        switch (e.type) {
        case SDL_QUIT:
            msg.message = 0x0012; // WM_QUIT
            break;
        case SDL_KEYDOWN:
            msg.message = 0x0100; // WM_KEYDOWN
            msg.wParam = e.key.keysym.sym;
            break;
        case SDL_KEYUP:
            msg.message = 0x0101; // WM_KEYUP
            msg.wParam = e.key.keysym.sym;
            break;
        case SDL_MOUSEMOTION:
            msg.message = 0x0200; // WM_MOUSEMOVE
            msg.lParam = (static_cast<int64_t>(e.motion.y) << 16) |
                         (static_cast<int64_t>(e.motion.x) & 0xFFFF);
            break;
        case SDL_MOUSEBUTTONDOWN:
            msg.message = 0x0201; // WM_LBUTTONDOWN
            msg.wParam = e.button.button;
            break;
        case SDL_MOUSEBUTTONUP:
            msg.message = 0x0202; // WM_LBUTTONUP
            msg.wParam = e.button.button;
            break;
        case SDL_WINDOWEVENT:
            switch (e.window.event) {
            case SDL_WINDOWEVENT_CLOSE:
                msg.message = 0x0010; // WM_CLOSE
                break;
            case SDL_WINDOWEVENT_RESIZED:
                msg.message = 0x0005; // WM_SIZE
                msg.wParam = 0;
                msg.lParam = (static_cast<int64_t>(e.window.data2) << 16) |
                             (static_cast<int64_t>(e.window.data1) & 0xFFFF);
                break;
            default:
                continue; // skip uninteresting window events
            }
            break;
        default:
            continue;
        }
        std::lock_guard<std::mutex> lk(g_mutex);
        for (auto& kv : g_windows) {
            if (kv.second.sdl_window) {
                auto* w = static_cast<SDL_Window*>(kv.second.sdl_window);
                if (SDL_GetWindowID(w) == e.window.windowID || e.type == SDL_QUIT) {
                    msg.hwnd = kv.first;
                    break;
                }
            }
        }
        if (!msg.hwnd && !g_windows.empty())
            msg.hwnd = g_windows.begin()->first;
        g_msg_queue.push_back(msg);
    }
}

} // namespace

// ==========================================================================
// Internal export for the D3D11 shim (SysV ABI).
// ==========================================================================
extern "C" WIN2LIN_EXPORT void* win2lin_user32_get_sdl_window(void* hwnd) {
    if (!hwnd) return nullptr;
    std::lock_guard<std::mutex> lk(g_mutex);
    auto it = g_windows.find(static_cast<HWND>(hwnd));
    if (it == g_windows.end()) return nullptr;
    return it->second.sdl_window;
}

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
    return RegisterClassExW(wc);
}

WIN2LIN_EXPORT ATOM WINAPI RegisterClassExA(const void* wc) {
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
    }

    const int win_w = (width <= 0 || width == static_cast<int>(0x80000000))
                          ? MOCK_SCREEN_W
                          : width;
    const int win_h = (height <= 0 || height == static_cast<int>(0x80000000))
                          ? MOCK_SCREEN_H
                          : height;
    const int win_x = (x == static_cast<int>(0x80000000)) ? 0 : x;
    const int win_y = (y == static_cast<int>(0x80000000)) ? 0 : y;

    const std::string title = window_name ? utf16_to_utf8(window_name) : "NEXLINK";

    void* sdl_win = nullptr;
    if (ensure_sdl()) {
        // Prefer a Vulkan-capable window so D3D11 can create a real surface.
        Uint32 sdl_flags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;
        SDL_Window* w = SDL_CreateWindow(title.c_str(), win_x, win_y, win_w, win_h,
                                         sdl_flags | SDL_WINDOW_VULKAN);
        if (!w) {
            USER32_INFO("SDL_CreateWindow(VULKAN) failed: %s; retrying without", SDL_GetError());
            w = SDL_CreateWindow(title.c_str(), win_x, win_y, win_w, win_h, sdl_flags);
        }
        if (w) {
            sdl_win = w;
            int rw = 0, rh = 0;
            SDL_GetWindowSize(w, &rw, &rh);
            USER32_INFO("SDL_CreateWindow '%s' -> %p (%dx%d, vulkan=%d)", title.c_str(),
                        static_cast<void*>(w), rw, rh,
                        (SDL_GetWindowFlags(w) & SDL_WINDOW_VULKAN) ? 1 : 0);
        } else {
            USER32_INFO("SDL_CreateWindow failed: %s", SDL_GetError());
        }
    }

    HWND hwnd = make_hwnd();
    WindowInfo info{};
    info.sdl_window = sdl_win;
    info.x = win_x;
    info.y = win_y;
    info.width = win_w;
    info.height = win_h;
    info.style = style;
    info.visible = false;
    info.wndproc = wndproc;
    {
        std::lock_guard<std::mutex> lk(g_mutex);
        g_windows[hwnd] = info;
    }
    USER32_INFO("CreateWindowExW class='%s' title='%s' -> HWND %p (sdl=%p)",
                class_name && !is_int_resource(class_name)
                    ? utf16_to_utf8(class_name).c_str()
                    : "(resource)",
                title.c_str(), static_cast<void*>(hwnd), sdl_win);
    return hwnd;
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
    std::lock_guard<std::mutex> lk(g_mutex);
    auto it = g_windows.find(hwnd);
    if (it == g_windows.end()) return 0;
    if (it->second.sdl_window) {
        SDL_Window* w = static_cast<SDL_Window*>(it->second.sdl_window);
        auto t0 = std::chrono::steady_clock::now();
        SDL_DestroyWindow(w);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::steady_clock::now() - t0)
                      .count();
        USER32_INFO("SDL_DestroyWindow took %lld ms", static_cast<long long>(ms));
    }
    g_windows.erase(it);
    return 1;
}

WIN2LIN_EXPORT BOOL WINAPI ShowWindow(HWND hwnd, int cmd) {
    std::lock_guard<std::mutex> lk(g_mutex);
    auto it = g_windows.find(hwnd);
    if (it == g_windows.end()) return 0;
    it->second.visible = (cmd != 0 /* SW_HIDE */);
    if (it->second.sdl_window) {
        if (cmd == 0) SDL_HideWindow(static_cast<SDL_Window*>(it->second.sdl_window));
        else SDL_ShowWindow(static_cast<SDL_Window*>(it->second.sdl_window));
    }
    USER32_INFO("ShowWindow cmd=%d", cmd);
    return 1;
}

WIN2LIN_EXPORT BOOL WINAPI UpdateWindow(HWND hwnd) {
    std::lock_guard<std::mutex> lk(g_mutex);
    return g_windows.count(hwnd) ? 1 : 0;
}

WIN2LIN_EXPORT BOOL WINAPI SetForegroundWindow(HWND hwnd) {
    std::lock_guard<std::mutex> lk(g_mutex);
    auto it = g_windows.find(hwnd);
    if (it == g_windows.end()) return 0;
    if (it->second.sdl_window)
        SDL_RaiseWindow(static_cast<SDL_Window*>(it->second.sdl_window));
    return 1;
}

WIN2LIN_EXPORT BOOL WINAPI GetClientRect(HWND hwnd, RECT* rc) {
    if (!rc) return 0;
    std::lock_guard<std::mutex> lk(g_mutex);
    auto it = g_windows.find(hwnd);
    if (it == g_windows.end()) return 0;
    rc->left = 0;
    rc->top = 0;
    if (it->second.sdl_window) {
        int w = 0, h = 0;
        SDL_GetWindowSize(static_cast<SDL_Window*>(it->second.sdl_window), &w, &h);
        rc->right = w;
        rc->bottom = h;
        it->second.width = w;
        it->second.height = h;
    } else {
        rc->right = it->second.width;
        rc->bottom = it->second.height;
    }
    return 1;
}

WIN2LIN_EXPORT BOOL WINAPI GetWindowRect(HWND hwnd, RECT* rc) {
    if (!rc) return 0;
    std::lock_guard<std::mutex> lk(g_mutex);
    auto it = g_windows.find(hwnd);
    if (it == g_windows.end()) return 0;
    rc->left = it->second.x - 8;
    rc->top = it->second.y - 32;
    rc->right = it->second.x + it->second.width + 8;
    rc->bottom = it->second.y + it->second.height + 8;
    return 1;
}

WIN2LIN_EXPORT BOOL WINAPI MoveWindow(HWND hwnd, int x, int y, int w, int h, BOOL) {
    std::lock_guard<std::mutex> lk(g_mutex);
    auto it = g_windows.find(hwnd);
    if (it == g_windows.end()) return 0;
    it->second.x = x;
    it->second.y = y;
    it->second.width = w;
    it->second.height = h;
    if (it->second.sdl_window)
        SDL_SetWindowSize(static_cast<SDL_Window*>(it->second.sdl_window), w, h);
    return 1;
}

WIN2LIN_EXPORT BOOL WINAPI SetWindowPos(HWND hwnd, HWND, int x, int y, int w, int h,
                                        UINT flags) {
    (void)flags;
    std::lock_guard<std::mutex> lk(g_mutex);
    auto it = g_windows.find(hwnd);
    if (it == g_windows.end()) return 0;
    if (x != static_cast<int>(0x80000000)) it->second.x = x;
    if (y != static_cast<int>(0x80000000)) it->second.y = y;
    if (w > 0) it->second.width = w;
    if (h > 0) it->second.height = h;
    return 1;
}

WIN2LIN_EXPORT BOOL WINAPI AdjustWindowRectEx(RECT* rc, DWORD, BOOL) {
    if (!rc) return 0;
    rc->left -= 8;
    rc->top -= 32;
    rc->right += 8;
    rc->bottom += 8;
    return 1;
}

WIN2LIN_EXPORT BOOL WINAPI IsWindow(HWND hwnd) {
    std::lock_guard<std::mutex> lk(g_mutex);
    return g_windows.count(hwnd) ? 1 : 0;
}
WIN2LIN_EXPORT BOOL WINAPI IsWindowVisible(HWND hwnd) {
    std::lock_guard<std::mutex> lk(g_mutex);
    auto it = g_windows.find(hwnd);
    if (it == g_windows.end()) return 0;
    return it->second.visible ? 1 : 0;
}
WIN2LIN_EXPORT BOOL WINAPI IsIconic(HWND) { return 0; }
WIN2LIN_EXPORT BOOL WINAPI OpenIcon(HWND) { return 1; }
WIN2LIN_EXPORT BOOL WINAPI EnableWindow(HWND, BOOL) { return 1; }

WIN2LIN_EXPORT HWND WINAPI GetDesktopWindow(void) {
    return reinterpret_cast<HWND>(static_cast<uintptr_t>(0xDEAD0000));
}
WIN2LIN_EXPORT HWND WINAPI GetForegroundWindow(void) {
    std::lock_guard<std::mutex> lk(g_mutex);
    return g_windows.empty() ? nullptr : g_windows.begin()->first;
}
WIN2LIN_EXPORT HWND WINAPI GetParent(HWND) { return nullptr; }
WIN2LIN_EXPORT HWND WINAPI SetParent(HWND, HWND) { return nullptr; }
WIN2LIN_EXPORT HWND WINAPI GetWindow(HWND, UINT) { return nullptr; }

WIN2LIN_EXPORT BOOL WINAPI ClientToScreen(HWND hwnd, POINT* pt) {
    if (!pt) return 0;
    std::lock_guard<std::mutex> lk(g_mutex);
    auto it = g_windows.find(hwnd);
    if (it == g_windows.end()) return 0;
    pt->x += it->second.x;
    pt->y += it->second.y;
    return 1;
}

WIN2LIN_EXPORT BOOL WINAPI ScreenToClient(HWND hwnd, POINT* pt) {
    if (!pt) return 0;
    std::lock_guard<std::mutex> lk(g_mutex);
    auto it = g_windows.find(hwnd);
    if (it == g_windows.end()) return 0;
    pt->x -= it->second.x;
    pt->y -= it->second.y;
    return 1;
}

WIN2LIN_EXPORT BOOL WINAPI SetWindowTextW(HWND hwnd, LPCWSTR text) {
    std::lock_guard<std::mutex> lk(g_mutex);
    auto it = g_windows.find(hwnd);
    if (it == g_windows.end()) return 0;
    if (it->second.sdl_window && text) {
        const std::string t = utf16_to_utf8(text);
        SDL_SetWindowTitle(static_cast<SDL_Window*>(it->second.sdl_window), t.c_str());
    }
    return 1;
}

WIN2LIN_EXPORT BOOL WINAPI SetWindowTextA(HWND hwnd, LPCSTR text) {
    std::lock_guard<std::mutex> lk(g_mutex);
    auto it = g_windows.find(hwnd);
    if (it == g_windows.end()) return 0;
    if (it->second.sdl_window && text) {
        SDL_SetWindowTitle(static_cast<SDL_Window*>(it->second.sdl_window), text);
    }
    return 1;
}

WIN2LIN_EXPORT int WINAPI GetWindowTextW(HWND hwnd, LPWSTR buf, int max_count) {
    if (!buf || max_count <= 0) return 0;
    std::lock_guard<std::mutex> lk(g_mutex);
    auto it = g_windows.find(hwnd);
    if (it == g_windows.end() || !it->second.sdl_window) {
        buf[0] = 0;
        return 0;
    }
    const char* t = SDL_GetWindowTitle(static_cast<SDL_Window*>(it->second.sdl_window));
    const std::u16string w = utf8_to_utf16(t);
    int n = 0;
    for (; n < max_count - 1 && n < static_cast<int>(w.size()); ++n) buf[n] = w[n];
    buf[n] = 0;
    return n;
}

WIN2LIN_EXPORT int WINAPI GetWindowTextA(HWND hwnd, LPSTR buf, int max_count) {
    if (!buf || max_count <= 0) return 0;
    std::lock_guard<std::mutex> lk(g_mutex);
    auto it = g_windows.find(hwnd);
    if (it == g_windows.end() || !it->second.sdl_window) {
        buf[0] = 0;
        return 0;
    }
    const char* t = SDL_GetWindowTitle(static_cast<SDL_Window*>(it->second.sdl_window));
    int n = 0;
    for (; n < max_count - 1 && t[n]; ++n) buf[n] = t[n];
    buf[n] = 0;
    return n;
}

WIN2LIN_EXPORT BOOL WINAPI BringWindowToTop(HWND hwnd) {
    std::lock_guard<std::mutex> lk(g_mutex);
    return g_windows.count(hwnd) ? 1 : 0;
}

// ==========================================================================
// Cursors / icons / DCs (unchanged mocks).
// ==========================================================================
WIN2LIN_EXPORT HCURSOR WINAPI LoadCursorW(HINSTANCE inst, LPCWSTR name) {
    (void)inst; (void)name;
    return reinterpret_cast<HCURSOR>(static_cast<uintptr_t>(0xC1));
}
WIN2LIN_EXPORT HCURSOR WINAPI LoadCursorA(HINSTANCE inst, LPCSTR name) {
    (void)inst; (void)name;
    return reinterpret_cast<HCURSOR>(static_cast<uintptr_t>(0xC1));
}
WIN2LIN_EXPORT HICON WINAPI LoadIconW(HINSTANCE inst, LPCWSTR name) {
    (void)inst; (void)name;
    return reinterpret_cast<HICON>(static_cast<uintptr_t>(0xC2));
}
WIN2LIN_EXPORT HICON WINAPI LoadIconA(HINSTANCE inst, LPCSTR name) {
    (void)inst; (void)name;
    return reinterpret_cast<HICON>(static_cast<uintptr_t>(0xC2));
}
WIN2LIN_EXPORT HDC WINAPI GetDC(HWND hwnd) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (g_windows.count(hwnd)) return reinterpret_cast<HDC>(static_cast<uintptr_t>(0xD1));
    return nullptr;
}
WIN2LIN_EXPORT int WINAPI ReleaseDC(HWND, HDC) { return 1; }
WIN2LIN_EXPORT HDC WINAPI BeginPaint(HWND hwnd, void* ps) {
    if (ps) std::memset(ps, 0, 64);
    std::lock_guard<std::mutex> lk(g_mutex);
    return g_windows.count(hwnd) ? reinterpret_cast<HDC>(static_cast<uintptr_t>(0xD1))
                                 : nullptr;
}
WIN2LIN_EXPORT BOOL WINAPI EndPaint(HWND, const void*) { return 1; }
WIN2LIN_EXPORT BOOL WINAPI InvalidateRect(HWND hwnd, const RECT*, BOOL) {
    std::lock_guard<std::mutex> lk(g_mutex);
    return g_windows.count(hwnd) ? 1 : 0;
}

// ==========================================================================
// Message loop (SDL event translation).
// ==========================================================================
WIN2LIN_EXPORT BOOL WINAPI PeekMessageW(MSG* msg, HWND hwnd, UINT min, UINT max,
                                        UINT remove) {
    (void)min; (void)max; (void)remove;
    if (!msg) return 0;
    translate_and_queue_events();
    std::lock_guard<std::mutex> lk(g_mutex);
    // Filter by hwnd if requested (0 == any).
    for (auto it = g_msg_queue.begin(); it != g_msg_queue.end(); ++it) {
        if (hwnd == nullptr || it->hwnd == hwnd) {
            *msg = *it;
            g_msg_queue.erase(it);
            return 1;
        }
    }
    std::memset(msg, 0, sizeof(MSG));
    return 0;
}

WIN2LIN_EXPORT BOOL WINAPI PeekMessageA(MSG* msg, HWND hwnd, UINT min, UINT max,
                                        UINT remove) {
    return PeekMessageW(msg, hwnd, min, max, remove);
}

WIN2LIN_EXPORT BOOL WINAPI GetMessageW(MSG* msg, HWND hwnd, UINT min, UINT max) {
    (void)min; (void)max;
    if (!msg) return 0;
    translate_and_queue_events();
    {
        std::lock_guard<std::mutex> lk(g_mutex);
        for (auto it = g_msg_queue.begin(); it != g_msg_queue.end(); ++it) {
            if (hwnd == nullptr || it->hwnd == hwnd) {
                *msg = *it;
                g_msg_queue.erase(it);
                if (msg->message == 0x0012) return 0; // WM_QUIT -> FALSE
                return 1;
            }
        }
    }
    // No event: block briefly on SDL (up to ~16ms) then re-check once.
    if (g_sdl_inited) {
        SDL_Delay(16);
        translate_and_queue_events();
        std::lock_guard<std::mutex> lk(g_mutex);
        for (auto it = g_msg_queue.begin(); it != g_msg_queue.end(); ++it) {
            if (hwnd == nullptr || it->hwnd == hwnd) {
                *msg = *it;
                g_msg_queue.erase(it);
                if (msg->message == 0x0012) return 0;
                return 1;
            }
        }
    }
    // Fallback: report WM_QUIT so loops terminate (headless / no events).
    std::memset(msg, 0, sizeof(MSG));
    msg->message = 0x0012;
    return 0;
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
        auto it = g_windows.find(msg->hwnd);
        if (it != g_windows.end()) wndproc = it->second.wndproc;
    }
    if (wndproc) return wndproc(msg->hwnd, msg->message, msg->wParam, msg->lParam);
    return DefWindowProcW(msg->hwnd, msg->message, msg->wParam, msg->lParam);
}

WIN2LIN_EXPORT LRESULT WINAPI DispatchMessageA(const MSG* msg) {
    return DispatchMessageW(msg);
}

WIN2LIN_EXPORT void WINAPI PostQuitMessage(int code) {
    USER32_INFO("PostQuitMessage(%d)", code);
    std::lock_guard<std::mutex> lk(g_mutex);
    MSG msg{};
    msg.message = 0x0012; // WM_QUIT
    msg.wParam = static_cast<WPARAM>(code);
    g_msg_queue.push_back(msg);
}

WIN2LIN_EXPORT BOOL WINAPI PostMessageW(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    std::lock_guard<std::mutex> lk(g_mutex);
    MSG m{};
    m.hwnd = hwnd;
    m.message = msg;
    m.wParam = w;
    m.lParam = l;
    g_msg_queue.push_back(m);
    return 1;
}

WIN2LIN_EXPORT BOOL WINAPI PostMessageA(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    return PostMessageW(hwnd, msg, w, l);
}

WIN2LIN_EXPORT LRESULT WINAPI SendMessageW(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    WNDPROC wndproc = nullptr;
    {
        std::lock_guard<std::mutex> lk(g_mutex);
        auto it = g_windows.find(hwnd);
        if (it != g_windows.end()) wndproc = it->second.wndproc;
    }
    if (wndproc) return wndproc(hwnd, msg, w, l);
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
// Misc.
// ==========================================================================
WIN2LIN_EXPORT int WINAPI GetSystemMetrics(int index) {
    if (index == 0) return static_cast<int>(MOCK_SCREEN_W);
    if (index == 1) return static_cast<int>(MOCK_SCREEN_H);
    return 0;
}

WIN2LIN_EXPORT int WINAPI MessageBoxW(HWND, LPCWSTR text, LPCWSTR caption, UINT type) {
    (void)type;
    USER32_INFO("MessageBoxW caption='%s' text='%s'",
                caption ? utf16_to_utf8(caption).c_str() : "",
                text ? utf16_to_utf8(text).c_str() : "");
    return 1;
}

WIN2LIN_EXPORT int WINAPI MessageBoxA(HWND, LPCSTR text, LPCSTR caption, UINT type) {
    (void)type;
    USER32_INFO("MessageBoxA caption='%s' text='%s'", caption ? caption : "",
                text ? text : "");
    return 1;
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

WIN2LIN_EXPORT void* WINAPI GetStockObject(int) {
    return nullptr;
}

WIN2LIN_EXPORT HCURSOR WINAPI SetCursor(HCURSOR hCursor) {
    (void)hCursor;
    return nullptr;
}

WIN2LIN_EXPORT BOOL WINAPI ShowCursor(BOOL bShow) {
    (void)bShow;
    return TRUE;
}

WIN2LIN_EXPORT BOOL WINAPI GetCursorPos(LPPOINT lpPoint) {
    (void)lpPoint;
    return TRUE;
}

WIN2LIN_EXPORT BOOL WINAPI SetCursorPos(int X, int Y) {
    (void)X; (void)Y;
    return TRUE;
}
