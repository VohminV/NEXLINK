// libwin_dwmapi.so - stub shim for dwmapi.dll (Desktop Window Manager).

#include "common/logging.hpp"

#include <cstdint>

using HANDLE = void*;
using HRESULT = int32_t;
using DWORD = uint32_t;
using HWND = HANDLE;
using LPCVOID = const void*;
using LPVOID = void*;
using LPCWSTR = const wchar_t*;
using BOOL = int32_t;
using UINT = uint32_t;
using LONG = int32_t;
using WPARAM = uintptr_t;
using LPARAM = intptr_t;
using LRESULT = intptr_t;
using MARGINS = void*;
using SIZE = void*;
using DWM_BLURBEHIND = void*;
using DWM_PRESENT_PARAMETERS = void*;
using DWM_TIMING_INFO = void*;
using DWM_THUMBNAIL_PROPERTIES = void*;
constexpr int TRUE = 1;
constexpr int FALSE = 0;
using HTHUMBNAIL = HANDLE;


namespace win2lin {

#define WINAPI __attribute__((ms_abi))

extern "C" WINAPI HRESULT DwmIsCompositionEnabled(BOOL* pfEnabled) {
    (void)pfEnabled;
    return 0;
}
extern "C" WINAPI HRESULT DwmEnableComposition(UINT uCompositionAction) {
    (void)uCompositionAction;
    return 0;
}
extern "C" WINAPI HRESULT DwmEnableBlurBehindWindow(HWND hWnd, const DWM_BLURBEHIND* pBlurBehind) {
    (void)hWnd; (void)pBlurBehind;
    return 0;
}
extern "C" WINAPI HRESULT DwmExtendFrameIntoClientArea(HWND hWnd, const MARGINS* pMarInset) {
    (void)hWnd; (void)pMarInset;
    return 0;
}
extern "C" WINAPI HRESULT DwmGetColorizationColor(DWORD* pcrColorization, BOOL* pfOpaqueBlend) {
    (void)pcrColorization; (void)pfOpaqueBlend;
    return 0;
}
extern "C" WINAPI HRESULT DwmGetWindowAttribute(HWND hwnd, DWORD dwAttribute, void* pvAttribute, DWORD cbAttribute) {
    (void)hwnd; (void)dwAttribute; (void)pvAttribute; (void)cbAttribute;
    return 0;
}
extern "C" WINAPI HRESULT DwmSetWindowAttribute(HWND hwnd, DWORD dwAttribute, LPCVOID pvAttribute, DWORD cbAttribute) {
    (void)hwnd; (void)dwAttribute; (void)pvAttribute; (void)cbAttribute;
    return 0;
}
extern "C" WINAPI HRESULT DwmDefWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, LRESULT* plResult) {
    (void)hwnd; (void)msg; (void)wParam; (void)lParam; (void)plResult;
    return 0;
}
extern "C" WINAPI HRESULT DwmSetPresentParameters(HWND hwnd, DWM_PRESENT_PARAMETERS* pPresentParams) {
    (void)hwnd; (void)pPresentParams;
    return 0;
}
extern "C" WINAPI HRESULT DwmGetPresentParameters(HWND hwnd, DWM_PRESENT_PARAMETERS* pPresentParams) {
    (void)hwnd; (void)pPresentParams;
    return 0;
}
extern "C" WINAPI HRESULT DwmGetTimingInfo(HWND hwnd, DWM_TIMING_INFO* pTimingInfo) {
    (void)hwnd; (void)pTimingInfo;
    return 0;
}
extern "C" WINAPI HRESULT DwmGetCompositionTimingInfo(HWND hwnd, DWM_TIMING_INFO* pTimingInfo) {
    (void)hwnd; (void)pTimingInfo;
    return 0;
}
extern "C" WINAPI HRESULT DwmRegisterThumbnail(HWND hwndDestination, HWND hwndSource, HTHUMBNAIL* phThumbnailId) {
    (void)hwndDestination; (void)hwndSource; (void)phThumbnailId;
    return 0;
}
extern "C" WINAPI HRESULT DwmUnregisterThumbnail(HTHUMBNAIL hThumbnailId) {
    (void)hThumbnailId;
    return 0;
}
extern "C" WINAPI HRESULT DwmUpdateThumbnailProperties(HTHUMBNAIL hThumbnailId, const DWM_THUMBNAIL_PROPERTIES* ptnProperties) {
    (void)hThumbnailId; (void)ptnProperties;
    return 0;
}
extern "C" WINAPI HRESULT DwmQueryThumbnailSourceSize(HTHUMBNAIL hThumbnailId, SIZE* pSize) {
    (void)hThumbnailId; (void)pSize;
    return 0;
}

using WPARAM = uintptr_t;
using LPARAM = intptr_t;
using LRESULT = intptr_t;
using MARGINS = void*;
using SIZE = void*;

} // namespace win2lin
