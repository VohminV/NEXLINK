// hello_window.cpp - Phase 1.5 integration fixture.
//
// A minimal "standard" Win32 application built against the MinGW MSVCRT
// runtime. Unlike fixtures/hello_win.c (CRT-free), this binary exercises the
// full startup path relinked Windows games take:
//
//   1. mainCRTStartup (inside this PE) calls into msvcrt.dll for command-line
//      and heap setup, then runs global C++ constructors via _initterm-equivalents.
//   2. main() prints through the CRT (msvcrt printf), proving the shim works.
//   3. A window is registered/created through user32.dll (mock HWND).
//   4. DXGI factory + adapter enumeration through dxgi.dll (COM mocks).
//
// Built with: x86_64-w64-mingw32-g++ -O2 -static-libgcc -static-libstdc++
// so the C++ runtime is embedded and only msvcrt/user32/dxgi/kernel32 are
// imported - matching what the Phase 1.5 shims must cover.
#include <windows.h>
#include <dxgi1_2.h>

#include <cstdio>
#include <cstring>

namespace {

// A global C++ object with a constructor: forces the CRT to run static
// initialization (_initterm path) before main().
struct StaticProbe {
    int value;
    StaticProbe() : value(0) { value = 0x2A; }
    ~StaticProbe() { }
};

StaticProbe g_probe;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

} // namespace

int main() {
    std::printf("[gui-test] msvcrt printf alive, g_probe=%d\n", g_probe.value);
    if (g_probe.value != 0x2A) {
        std::printf("[gui-test] FAIL: global constructor did not run\n");
        return 1;
    }

    HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512)); // IDC_ARROW
    wc.lpszClassName = L"NexlinkGuiTest";
    ATOM atom = RegisterClassExW(&wc);
    std::printf("[gui-test] RegisterClassExW -> atom=%u\n", static_cast<unsigned>(atom));
    if (atom == 0) {
        std::printf("[gui-test] FAIL: RegisterClassExW returned 0\n");
        return 2;
    }

    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"NEXLINK DXGI smoke",
                                WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                                1920, 1080, nullptr, nullptr, instance, nullptr);
    std::printf("[gui-test] CreateWindowExW -> hwnd=%p\n", static_cast<void*>(hwnd));
    if (!hwnd) {
        std::printf("[gui-test] FAIL: CreateWindowExW returned NULL\n");
        return 3;
    }

    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);
    SetForegroundWindow(hwnd);

    RECT client = {};
    if (!GetClientRect(hwnd, &client)) {
        std::printf("[gui-test] FAIL: GetClientRect failed\n");
        return 4;
    }
    std::printf("[gui-test] GetClientRect -> %ldx%ld\n",
                static_cast<long>(client.right), static_cast<long>(client.bottom));
    if (client.right != 1920 || client.bottom != 1080) {
        std::printf("[gui-test] FAIL: unexpected client size\n");
        return 5;
    }

    IDXGIFactory1* factory = nullptr;
    HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1),
                                    reinterpret_cast<void**>(&factory));
    std::printf("[gui-test] CreateDXGIFactory1 -> hr=0x%08lX factory=%p\n",
                static_cast<unsigned long>(hr), static_cast<void*>(factory));
    if (hr != S_OK || !factory) {
        std::printf("[gui-test] FAIL: CreateDXGIFactory1 unsuccessful\n");
        return 6;
    }

    IDXGIAdapter1* adapter = nullptr;
    hr = factory->EnumAdapters1(0, &adapter);
    std::printf("[gui-test] EnumAdapters1(0) -> hr=0x%08lX adapter=%p\n",
                static_cast<unsigned long>(hr), static_cast<void*>(adapter));
    if (hr != S_OK || !adapter) {
        std::printf("[gui-test] FAIL: EnumAdapters1(0) unsuccessful\n");
        factory->Release();
        return 7;
    }

    DXGI_ADAPTER_DESC1 desc = {};
    adapter->GetDesc1(&desc);
    char name[128] = {};
    std::wcstombs(name, desc.Description, sizeof(name) - 1);
    std::printf("[gui-test] adapter: %s (dedicated VRAM %llu MB)\n", name,
                static_cast<unsigned long long>(desc.DedicatedVideoMemory >> 20));

    // Exhaust enumeration: second adapter must report DXGI_ERROR_NOT_FOUND.
    IDXGIAdapter1* adapter2 = nullptr;
    hr = factory->EnumAdapters1(1, &adapter2);
    std::printf("[gui-test] EnumAdapters1(1) -> hr=0x%08lX (expect DXGI_ERROR_NOT_FOUND)\n",
                static_cast<unsigned long>(hr));
    if (hr != static_cast<HRESULT>(0x887A0002L) || adapter2 != nullptr) {
        std::printf("[gui-test] FAIL: expected DXGI_ERROR_NOT_FOUND for adapter 1\n");
        adapter->Release();
        factory->Release();
        return 8;
    }

    adapter->Release();
    factory->Release();
    std::printf("[gui-test] PASS\n");
    return 0;
}
