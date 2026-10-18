// libwin_uxtheme.so - stub shim for uxtheme.dll (Visual Styles).

#include "common/logging.hpp"

#include <cstdint>

using HANDLE = void*;
using HTHEME = void*;
using HWND = HANDLE;
using HDC = HANDLE;
using HRESULT = int32_t;
using DWORD = uint32_t;
using UINT = uint32_t;
using LPCWSTR = const wchar_t*;
using LPWSTR = wchar_t*;
using LPCSTR = const char*;
using LPVOID = void*;
using LPCVOID = const void*;
using BOOL = int32_t;
using COLORREF = uint32_t;
using RECT = void*;
using LPRECT = void*;
using MARGINS = void*;
using SIZE = void*;
using THEMESIZE = int32_t;
using POINT = void*;
using HBITMAP = HANDLE;
using HRGN = HANDLE;
using WPARAM = uintptr_t;
using LPARAM = intptr_t;
using LRESULT = intptr_t;
using DTTOPTS = void*;
using LOGFONTW = void*;
using WCHAR = wchar_t;
using HWND = HANDLE;
using HANDLE = void*;
using HDC = HANDLE;
using HRESULT = int32_t;
using DWORD = uint32_t;
using UINT = uint32_t;
using LPCWSTR = const wchar_t*;
using LPCSTR = const char*;
using LPVOID = void*;
using LPCVOID = const void*;
using BOOL = int32_t;
using COLORREF = uint32_t;
using RECT = void*;
using MARGINS = void*;
using SIZE = void*;
using POINT = void*;
using HBITMAP = HANDLE;
using HIMAGELIST = HANDLE;
using WPARAM = uintptr_t;
using LPARAM = intptr_t;
using LRESULT = intptr_t;
using DTTOPTS = void*;
using LOGFONTW = void*;
using WCHAR = wchar_t;
constexpr int FALSE = 0;
constexpr int TRUE = 1;

namespace win2lin {

#define WINAPI __attribute__((ms_abi))

extern "C" WINAPI HTHEME OpenThemeData(HWND hwnd, LPCWSTR pszClassList) {
    (void)hwnd; (void)pszClassList;
    return nullptr;
}
extern "C" WINAPI HRESULT CloseThemeData(HTHEME hTheme) {
    (void)hTheme;
    return 0;
}
extern "C" WINAPI HRESULT DrawThemeBackground(HTHEME hTheme, HDC hdc, int iPartId, int iStateId, const RECT* pRect, const RECT* pClipRect) {
    (void)hTheme; (void)hdc; (void)iPartId; (void)iStateId; (void)pRect; (void)pClipRect;
    return 0;
}
extern "C" WINAPI HRESULT DrawThemeText(HTHEME hTheme, HDC hdc, int iPartId, int iStateId, LPCWSTR pszText, int iCharCount, DWORD dwTextFlags, DWORD dwTextFlags2, const RECT* pRect) {
    (void)hTheme; (void)hdc; (void)iPartId; (void)iStateId; (void)pszText; (void)iCharCount; (void)dwTextFlags; (void)dwTextFlags2; (void)pRect;
    return 0;
}
extern "C" WINAPI HRESULT GetThemeBackgroundContentRect(HTHEME hTheme, HDC hdc, int iPartId, int iStateId, const RECT* pBoundingRect, RECT* pContentRect) {
    (void)hTheme; (void)hdc; (void)iPartId; (void)iStateId; (void)pBoundingRect; (void)pContentRect;
    return 0;
}
extern "C" WINAPI HRESULT GetThemeBackgroundExtent(HTHEME hTheme, HDC hdc, int iPartId, int iStateId, const RECT* pContentRect, RECT* pExtentRect) {
    (void)hTheme; (void)hdc; (void)iPartId; (void)iStateId; (void)pContentRect; (void)pExtentRect;
    return 0;
}
extern "C" WINAPI HRESULT GetThemePartSize(HTHEME hTheme, HDC hdc, int iPartId, int iStateId, const RECT* prc, THEMESIZE eSize, SIZE* psz) {
    (void)hTheme; (void)hdc; (void)iPartId; (void)iStateId; (void)prc; (void)eSize; (void)psz;
    return 0;
}
extern "C" WINAPI HRESULT GetThemeColor(HTHEME hTheme, int iPartId, int iStateId, int iPropId, COLORREF* pColor) {
    (void)hTheme; (void)iPartId; (void)iStateId; (void)iPropId; (void)pColor;
    return 0;
}
extern "C" WINAPI HRESULT GetThemeBool(HTHEME hTheme, int iPartId, int iStateId, int iPropId, BOOL* pfVal) {
    (void)hTheme; (void)iPartId; (void)iStateId; (void)iPropId; (void)pfVal;
    return 0;
}
extern "C" WINAPI HRESULT GetThemeInt(HTHEME hTheme, int iPartId, int iStateId, int iPropId, int* piVal) {
    (void)hTheme; (void)iPartId; (void)iStateId; (void)iPropId; (void)piVal;
    return 0;
}
extern "C" WINAPI HRESULT GetThemeString(HTHEME hTheme, int iPartId, int iStateId, int iPropId, LPWSTR pszBuff, int cchMaxBuffChars) {
    (void)hTheme; (void)iPartId; (void)iStateId; (void)iPropId; (void)pszBuff; (void)cchMaxBuffChars;
    return 0;
}
extern "C" WINAPI HRESULT GetThemeRect(HTHEME hTheme, int iPartId, int iStateId, int iPropId, RECT* pRect) {
    (void)hTheme; (void)iPartId; (void)iStateId; (void)iPropId; (void)pRect;
    return 0;
}
extern "C" WINAPI HRESULT GetThemePosition(HTHEME hTheme, int iPartId, int iStateId, int iPropId, POINT* pPoint) {
    (void)hTheme; (void)iPartId; (void)iStateId; (void)iPropId; (void)pPoint;
    return 0;
}
extern "C" WINAPI HRESULT GetThemeFont(HTHEME hTheme, HDC hdc, int iPartId, int iStateId, int iPropId, LOGFONTW* plf) {
    (void)hTheme; (void)hdc; (void)iPartId; (void)iStateId; (void)iPropId; (void)plf;
    return 0;
}
extern "C" WINAPI HRESULT GetThemeBitmap(HTHEME hTheme, int iPartId, int iStateId, int iPropId, HBITMAP* phBitmap) {
    (void)hTheme; (void)iPartId; (void)iStateId; (void)iPropId; (void)phBitmap;
    return 0;
}
extern "C" WINAPI HRESULT GetThemeSysBool(int iBoolId) { (void)iBoolId; return FALSE; }
extern "C" WINAPI HRESULT GetThemeSysColor(int iColorId) { (void)iColorId; return 0; }
extern "C" WINAPI HRESULT GetThemeSysColorBrush(int iColorId) { (void)iColorId; return 0; }
extern "C" WINAPI HRESULT GetThemeSysFont(int iFontId, LOGFONTW* plf) { (void)iFontId; (void)plf; return 0; }
extern "C" WINAPI HRESULT GetThemeSysString(int iStringId, LPWSTR pszString, int cchMaxStringChars) { (void)iStringId; (void)pszString; (void)cchMaxStringChars; return 0; }
extern "C" WINAPI HRESULT GetThemeSysSize(int iSizeId) { (void)iSizeId; return 0; }
extern "C" WINAPI HRESULT GetThemeSysInt(int iIntId) { (void)iIntId; return 0; }
extern "C" WINAPI HRESULT GetThemeSysMargins(int iMarginId, MARGINS* pMargins) { (void)iMarginId; (void)pMargins; return 0; }
extern "C" WINAPI BOOL IsThemeActive(void) { return FALSE; }
extern "C" WINAPI BOOL IsAppThemed(void) { return FALSE; }
extern "C" WINAPI BOOL IsThemePartDefined(HTHEME hTheme, int iPartId, int iStateId) { (void)hTheme; (void)iPartId; (void)iStateId; return FALSE; }
extern "C" WINAPI HRESULT GetCurrentThemeName(LPWSTR pszThemeFileName, int cchMaxNameChars, LPWSTR pszColorBuff, int cchMaxColorChars, LPWSTR pszSizeBuff, int cchMaxSizeChars) {
    (void)pszThemeFileName; (void)cchMaxNameChars; (void)pszColorBuff; (void)cchMaxColorChars; (void)pszSizeBuff; (void)cchMaxSizeChars;
    return 0;
}
extern "C" WINAPI HRESULT GetWindowTheme(HWND hwnd) { (void)hwnd; return 0; }
extern "C" WINAPI HRESULT SetWindowTheme(HWND hwnd, LPCWSTR pszSubAppName, LPCWSTR pszSubIdList) {
    (void)hwnd; (void)pszSubAppName; (void)pszSubIdList;
    return 0;
}
extern "C" WINAPI HRESULT GetThemeDocumentationProperty(LPCWSTR pszThemeName, LPCWSTR pszPropertyName, LPWSTR pszValueBuff, int cchMaxValChars) {
    (void)pszThemeName; (void)pszPropertyName; (void)pszValueBuff; (void)cchMaxValChars;
    return 0;
}
extern "C" WINAPI HRESULT DrawThemeParentBackground(HWND hwnd, HDC hdc, const RECT* prc) {
    (void)hwnd; (void)hdc; (void)prc;
    return 0;
}
extern "C" WINAPI HRESULT DrawThemeParentBackgroundEx(HWND hwnd, HDC hdc, DWORD dwFlags, const RECT* prc) {
    (void)hwnd; (void)hdc; (void)dwFlags; (void)prc;
    return 0;
}
extern "C" WINAPI HRESULT DrawThemeEdge(HTHEME hTheme, HDC hdc, int iPartId, int iStateId, const RECT* pDestRect, DWORD uEdge, DWORD uFlags, RECT* pMirrorRect) {
    (void)hTheme; (void)hdc; (void)iPartId; (void)iStateId; (void)pDestRect; (void)uEdge; (void)uFlags; (void)pMirrorRect;
    return 0;
}
extern "C" WINAPI HRESULT DrawThemeIcon(HTHEME hTheme, HDC hdc, int iPartId, int iStateId, const RECT* pRect, HIMAGELIST himl, int iIndex) {
    (void)hTheme; (void)hdc; (void)iPartId; (void)iStateId; (void)pRect; (void)himl; (void)iIndex;
    return 0;
}
extern "C" WINAPI HRESULT DrawThemeTextEx(HTHEME hTheme, HDC hdc, int iPartId, int iStateId, LPCWSTR pszText, int iCharCount, DWORD dwTextFlags, LPRECT pRect, DTTOPTS* pOptions) {
    (void)hTheme; (void)hdc; (void)iPartId; (void)iStateId; (void)pszText; (void)iCharCount; (void)dwTextFlags; (void)pRect; (void)pOptions;
    return 0;
}
extern "C" WINAPI HRESULT GetThemeBackgroundRegion(HTHEME hTheme, HDC hdc, int iPartId, int iStateId, const RECT* pRect, HRGN* pRegion) {
    (void)hTheme; (void)hdc; (void)iPartId; (void)iStateId; (void)pRect; (void)pRegion;
    return 0;
}
extern "C" WINAPI HRESULT GetThemeMargins(HTHEME hTheme, HDC hdc, int iPartId, int iStateId, int iPropId, const RECT* prc, MARGINS* pMargins) {
    (void)hTheme; (void)hdc; (void)iPartId; (void)iStateId; (void)iPropId; (void)prc; (void)pMargins;
    return 0;
}
extern "C" WINAPI HRESULT GetThemeEnumValue(HTHEME hTheme, int iPartId, int iStateId, int iPropId, int* piVal) {
    (void)hTheme; (void)iPartId; (void)iStateId; (void)iPropId; (void)piVal;
    return 0;
}
extern "C" WINAPI HRESULT GetThemeFilename(HTHEME hTheme, int iPartId, int iStateId, int iPropId, LPWSTR pszBuff, int cchMaxBuffChars) {
    (void)hTheme; (void)iPartId; (void)iStateId; (void)iPropId; (void)pszBuff; (void)cchMaxBuffChars;
    return 0;
}

using POINT = void*;
using HBITMAP = HANDLE;
using HIMAGELIST = HANDLE;
using DTTOPTS = void*;
using LOGFONTW = void*;

} // namespace win2lin
