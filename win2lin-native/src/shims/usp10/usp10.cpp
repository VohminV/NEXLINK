// libwin_usp10.so - stub shim for usp10.dll (Uniscribe text shaping).

#include "common/logging.hpp"

#include <cstdint>

using SCRIPT_ANALYSIS = void*;
using SCRIPT_ITEM = void*;
using SCRIPT_LOGATTR = void*;
using SCRIPT_STATE = void*;
using SCRIPT_TABDEF = void*;
using SCRIPT_CHARPROP = void*;
using SCRIPT_FONTPROPERTIES = void*;
using SCRIPT_CACHE = void*;
using SCRIPT_VISATTR = void*;
using ABC = void*;
using HFONT = void*;
using HANDLE = void*;
using HDC = HANDLE;
using HWND = HANDLE;
using LONG = int32_t;
using DWORD = uint32_t;
using UINT = uint32_t;
using LPVOID = void*;
using LPCVOID = const void*;
using LPCSTR = const char*;
using LPSTR = char*;
using LPWSTR = wchar_t*;
using HRESULT = int32_t;
using RECT = void*;
using GOFFSET = void*;
using SCRIPT_CONTROL = void*;
using SCRIPT_STRING_ANALYSIS = void*;
using USHORT = uint16_t;
using WCHAR = wchar_t;
using WORD = uint16_t;
using SCRIPT_PROPERTIES = void*;
using LPCSTR = const char*;
using LPCWSTR = const wchar_t*;
using LPSTR = char*;
using LPWSTR = wchar_t*;
using BOOL = int32_t;
using LRESULT = intptr_t;

namespace win2lin {

#define WINAPI __attribute__((ms_abi))

extern "C" WINAPI HRESULT ScriptShape(HDC hdc, SCRIPT_ANALYSIS* psa, const WCHAR* pwcChars, int cChars, int cMaxGlyphs, WORD* pwGlyphs, int* pcGlyphs, SCRIPT_VISATTR* psva, SCRIPT_ANALYSIS* psaOut) {
    (void)hdc; (void)psa; (void)pwcChars; (void)cChars; (void)cMaxGlyphs; (void)pwGlyphs; (void)pcGlyphs; (void)psva; (void)psaOut;
    return 0;
}
extern "C" WINAPI HRESULT ScriptPlace(HDC hdc, SCRIPT_ANALYSIS* psa, const WORD* pwGlyphs, int cGlyphs, int* piAdvance, int* piJust, GOFFSET* pg) {
    (void)hdc; (void)psa; (void)pwGlyphs; (void)cGlyphs; (void)piAdvance; (void)piJust; (void)pg;
    return 0;
}
extern "C" WINAPI HRESULT ScriptTextOut(HDC hdc, SCRIPT_ANALYSIS* psa, const WCHAR* pwcChars, int cChars, int cGlyphs, const WORD* pwGlyphs, const int* piAdvance, const GOFFSET* pg, const RECT* lprc, int iMin, int iMax, const UINT* piDxyOrder, int* piDxy, int cDxy) {
    (void)hdc; (void)psa; (void)pwcChars; (void)cChars; (void)cGlyphs; (void)pwGlyphs; (void)piAdvance; (void)pg; (void)lprc; (void)iMin; (void)iMax; (void)piDxyOrder; (void)piDxy; (void)cDxy;
    return 0;
}
extern "C" WINAPI HRESULT ScriptItemize(LPCWSTR pwcInChars, int cInChars, int cMaxItems, const SCRIPT_CONTROL* psControl, const SCRIPT_STATE* psState, SCRIPT_ITEM* pictem, int* piItems) {
    (void)pwcInChars; (void)cInChars; (void)cMaxItems; (void)psControl; (void)psState; (void)pictem; (void)piItems;
    return 0;
}
extern "C" WINAPI HRESULT ScriptGetProperties(const SCRIPT_PROPERTIES** ppsp, int* piNumScripts) {
    (void)ppsp; (void)piNumScripts;
    return 0;
}
extern "C" WINAPI HRESULT ScriptGetCMap(int cChars, const WCHAR* pwcInChars, DWORD dwFlags, SCRIPT_ANALYSIS* psa, WORD* pwGlyphs, WORD* pwTo) {
    (void)cChars; (void)pwcInChars; (void)dwFlags; (void)psa; (void)pwGlyphs; (void)pwTo;
    return 0;
}
extern "C" WINAPI HRESULT ScriptBreak(const SCRIPT_LOGATTR* psla, int cChars, int* piCluster, int* piCharType) {
    (void)psla; (void)cChars; (void)piCluster; (void)piCharType;
    return 0;
}
extern "C" WINAPI HRESULT ScriptPlaceSingleGlyph(HDC hdc, SCRIPT_ANALYSIS* psa, WORD wGlyph, int piAdvance, int piJust, GOFFSET* pg) {
    (void)hdc; (void)psa; (void)wGlyph; (void)piAdvance; (void)piJust; (void)pg;
    return 0;
}
extern "C" WINAPI HRESULT ScriptGetGlyphABCWidths(HDC hdc, SCRIPT_ANALYSIS* psa, const WORD* pwGlyphs, int cGlyphs, ABC* pABC) {
    (void)hdc; (void)psa; (void)pwGlyphs; (void)cGlyphs; (void)pABC;
    return 0;
}
extern "C" WINAPI HRESULT ScriptGetFontAlternateGlyphs(HDC hdc, SCRIPT_ANALYSIS* psa, const WCHAR* pwcInChars, int cInChars, const SCRIPT_ANALYSIS* psaIn, const SCRIPT_LOGATTR* psla, int* piGlyphs, USHORT* pwGlyphs) {
    (void)hdc; (void)psa; (void)pwcInChars; (void)cInChars; (void)psaIn; (void)psla; (void)piGlyphs; (void)pwGlyphs;
    return 0;
}
extern "C" WINAPI HRESULT ScriptGetFontScriptProperties(HDC hdc, SCRIPT_FONTPROPERTIES* psfp) {
    (void)hdc; (void)psfp;
    return 0;
}
extern "C" WINAPI HRESULT ScriptFontSubstitute(HDC hdc, SCRIPT_ANALYSIS* psa, WCHAR* pwcInChars, int cInChars, WCHAR* pwcOutChars, int* piOutChars) {
    (void)hdc; (void)psa; (void)pwcInChars; (void)cInChars; (void)pwcOutChars; (void)piOutChars;
    return 0;
}
extern "C" WINAPI HRESULT ScriptString_p(const SCRIPT_STRING_ANALYSIS* psa) { (void)psa; return 0; }
extern "C" WINAPI HRESULT ScriptString_free(SCRIPT_STRING_ANALYSIS* pssa) { (void)pssa; return 0; }
extern "C" WINAPI HRESULT ScriptString_analyze(HDC hdc, const void* pString, int cString, int cGlyphs, int iCharset, DWORD dwFlags, int iDxy, RECT* rcl, SCRIPT_CONTROL* psControl, SCRIPT_STATE* psState, SCRIPT_TABDEF* pTabDef, SCRIPT_STRING_ANALYSIS* pssa) {
    (void)hdc; (void)pString; (void)cString; (void)cGlyphs; (void)iCharset; (void)dwFlags; (void)iDxy; (void)rcl; (void)psControl; (void)psState; (void)pTabDef; (void)pssa;
    return 0;
}

using SCRIPT_CONTROL = void*;
using SCRIPT_STATE = void*;
using SCRIPT_TABDEF = void*;
using SCRIPT_STRING_ANALYSIS = void*;
using USHORT = uint16_t;
using RECT = void*;
using GOFFSET = void*;
using SCRIPT_PROPERTIES = void*;

} // namespace win2lin
