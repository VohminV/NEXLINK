// libwin_imm32.so - stub shim for imm32.dll (Input Method Editor).

#include "common/logging.hpp"

#include <cstdint>

using HANDLE = void*;
using HWND = HANDLE;
using HDC = HANDLE;
using HIMC = HANDLE;
using HIMCC = HANDLE;
using LONG = int32_t;
using DWORD = uint32_t;
using UINT = uint32_t;
using LPVOID = void*;
using LPCVOID = const void*;
using LPCSTR = const char*;
using LPCWSTR = const wchar_t*;
using LPSTR = char*;
using LPWSTR = wchar_t*;
using BOOL = int32_t;
using LRESULT = intptr_t;
using WPARAM = uintptr_t;
using LPARAM = intptr_t;


using LPDWORD = DWORD*;
using HKL = HANDLE;

constexpr int TRUE = 1;
constexpr int FALSE = 0;

namespace win2lin {

#define WINAPI __attribute__((ms_abi))

extern "C" WINAPI HIMC ImmGetContext(HWND hWnd) { (void)hWnd; return nullptr; }
extern "C" WINAPI BOOL ImmReleaseContext(HWND hWnd, HIMC hIMC) { (void)hWnd; (void)hIMC; return TRUE; }
extern "C" WINAPI HIMC ImmCreateContext(void) { return nullptr; }
extern "C" WINAPI BOOL ImmDestroyContext(HIMC hIMC) { (void)hIMC; return TRUE; }
extern "C" WINAPI LONG ImmGetCompositionStringA(HIMC hIMC, DWORD dwIndex, LPVOID lpBuf, DWORD dwBufLen) { (void)hIMC; (void)dwIndex; (void)lpBuf; (void)dwBufLen; return 0; }
extern "C" WINAPI LONG ImmGetCompositionStringW(HIMC hIMC, DWORD dwIndex, LPVOID lpBuf, DWORD dwBufLen) { (void)hIMC; (void)dwIndex; (void)lpBuf; (void)dwBufLen; return 0; }
extern "C" WINAPI BOOL ImmSetCompositionStringA(HIMC hIMC, DWORD dwIndex, LPCVOID lpComp, DWORD dwCompLen, LPCVOID lpRead, DWORD dwReadLen) { (void)hIMC; (void)dwIndex; (void)lpComp; (void)dwCompLen; (void)lpRead; (void)dwReadLen; return TRUE; }
extern "C" WINAPI BOOL ImmSetCompositionStringW(HIMC hIMC, DWORD dwIndex, LPCVOID lpComp, DWORD dwCompLen, LPCVOID lpRead, DWORD dwReadLen) { (void)hIMC; (void)dwIndex; (void)lpComp; (void)dwCompLen; (void)lpRead; (void)dwReadLen; return TRUE; }
extern "C" WINAPI BOOL ImmAssociateContext(HWND hWnd, HIMC hIMC) { (void)hWnd; (void)hIMC; return TRUE; }
extern "C" WINAPI BOOL ImmGetOpenStatus(HIMC hIMC) { (void)hIMC; return TRUE; }
extern "C" WINAPI BOOL ImmSetOpenStatus(HIMC hIMC, BOOL bOpen) { (void)hIMC; (void)bOpen; return TRUE; }
extern "C" WINAPI DWORD ImmGetConversionStatus(HIMC hIMC, LPDWORD lpdw, LPDWORD lpdw2) { (void)hIMC; (void)lpdw; (void)lpdw2; return 0; }
extern "C" WINAPI BOOL ImmSetConversionStatus(HIMC hIMC, DWORD dw1, DWORD dw2) { (void)hIMC; (void)dw1; (void)dw2; return TRUE; }
extern "C" WINAPI BOOL ImmGetVirtualKey(HWND hWnd) { (void)hWnd; return 0; }
extern "C" WINAPI LONG ImmGetCompositionWord(HIMC hIMC, DWORD dwIndex, LPVOID lpBuf) { (void)hIMC; (void)dwIndex; (void)lpBuf; return 0; }
extern "C" WINAPI DWORD ImmGetProperty(HIMC hIMC, DWORD dwIndex) { (void)hIMC; (void)dwIndex; return 0; }
extern "C" WINAPI BOOL ImmSimulateHotKey(HWND hWnd, DWORD dwHotkey) { (void)hWnd; (void)dwHotkey; return TRUE; }
extern "C" WINAPI BOOL ImmGetGuideLineA(HIMC hIMC, DWORD dwIndex, LPSTR lpBuf, DWORD dwBufLen) { (void)hIMC; (void)dwIndex; (void)lpBuf; (void)dwBufLen; return 0; }
extern "C" WINAPI BOOL ImmGetGuideLineW(HIMC hIMC, DWORD dwIndex, LPWSTR lpBuf, DWORD dwBufLen) { (void)hIMC; (void)dwIndex; (void)lpBuf; (void)dwBufLen; return 0; }
extern "C" WINAPI BOOL ImmEscapeA(HIMC hIMC, UINT uEscape, LPVOID lpData) { (void)hIMC; (void)uEscape; (void)lpData; return TRUE; }
extern "C" WINAPI BOOL ImmEscapeW(HIMC hIMC, UINT uEscape, LPVOID lpData) { (void)hIMC; (void)uEscape; (void)lpData; return TRUE; }
extern "C" WINAPI DWORD ImmGetDesiredStyle(HWND hWnd) { (void)hWnd; return 0; }
extern "C" WINAPI BOOL ImmIsIME(HKL hKL) { (void)hKL; return TRUE; }
extern "C" WINAPI HKL ImmGetDefaultIMEWnd(HWND hWnd) { (void)hWnd; return nullptr; }
extern "C" WINAPI LRESULT ImmGetCompositionString(HIMC hIMC, DWORD dwIndex, LPVOID lpBuf, DWORD dwBufLen) { (void)hIMC; (void)dwIndex; (void)lpBuf; (void)dwBufLen; return 0; }
extern "C" WINAPI HIMC ImmAssociateContextEx(HWND hWnd, DWORD dwFlags) { (void)hWnd; (void)dwFlags; return nullptr; }

using LPDWORD = DWORD*;
using HKL = HANDLE;

} // namespace win2lin
