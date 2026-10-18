// libwin_shlwapi.so - stub shim for shlwapi.dll (lightweight shell helpers).

#include "common/logging.hpp"

#include <cstdint>

using DWORD = uint32_t;
using UINT = uint32_t;
using LPCSTR = const char*;
using LPCWSTR = const wchar_t*;
using LPSTR = char*;
using LPWSTR = wchar_t*;
using HANDLE = void*;
using HINSTANCE = void*;
using HWND = void*;
using HRESULT = int32_t;
using BOOL = int32_t;
using SIZE_T = uint64_t;
using LONG = int32_t;
using LPCVOID = const void*;
using LPVOID = void*;
using LPBYTE = unsigned char*;
using LPWIN32_FIND_DATAA = void*;
using LPWIN32_FIND_DATAW = void*;
using SHSTOCKICONID = int32_t;
using SHSTOCKICONINFO = void*;

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif
#ifndef INVALID_HANDLE_VALUE
#define INVALID_HANDLE_VALUE ((HANDLE)(void*)-1)
#endif

namespace win2lin {

#define WINAPI __attribute__((ms_abi))

extern "C" WINAPI int StrCmpIA(LPCSTR psz1, LPCSTR psz2) {
    (void)psz1; (void)psz2;
    return 0;
}
extern "C" WINAPI int StrCmpIW(LPCWSTR psz1, LPCWSTR psz2) {
    (void)psz1; (void)psz2;
    return 0;
}
extern "C" WINAPI int StrCmpA(LPCSTR psz1, LPCSTR psz2) {
    (void)psz1; (void)psz2;
    return 0;
}
extern "C" WINAPI int StrCmpW(LPCWSTR psz1, LPCWSTR psz2) {
    (void)psz1; (void)psz2;
    return 0;
}
extern "C" WINAPI int StrCmpLogicalW(LPCWSTR psz1, LPCWSTR psz2) {
    (void)psz1; (void)psz2;
    return 0;
}
extern "C" WINAPI int StrCmpLogicalA(LPCSTR psz1, LPCSTR psz2) {
    (void)psz1; (void)psz2;
    return 0;
}
extern "C" WINAPI BOOL PathFileExistsA(LPCSTR pszPath) {
    (void)pszPath;
    return FALSE;
}
extern "C" WINAPI BOOL PathFileExistsW(LPCWSTR pszPath) {
    (void)pszPath;
    return FALSE;
}
extern "C" WINAPI BOOL PathIsDirectoryA(LPCSTR pszPath) {
    (void)pszPath;
    return FALSE;
}
extern "C" WINAPI BOOL PathIsDirectoryW(LPCWSTR pszPath) {
    (void)pszPath;
    return FALSE;
}
extern "C" WINAPI BOOL PathIsRelativeA(LPCSTR pszPath) {
    (void)pszPath;
    return FALSE;
}
extern "C" WINAPI BOOL PathIsRelativeW(LPCWSTR pszPath) {
    (void)pszPath;
    return FALSE;
}
extern "C" WINAPI BOOL PathIsURLA(LPCSTR pszPath) {
    (void)pszPath;
    return FALSE;
}
extern "C" WINAPI BOOL PathIsURLW(LPCWSTR pszPath) {
    (void)pszPath;
    return FALSE;
}
extern "C" WINAPI BOOL PathRemoveExtensionA(LPSTR pszPath) {
    (void)pszPath;
    return FALSE;
}
extern "C" WINAPI BOOL PathRemoveExtensionW(LPWSTR pszPath) {
    (void)pszPath;
    return FALSE;
}
extern "C" WINAPI BOOL PathRemoveFileSpecA(LPSTR pszPath) {
    (void)pszPath;
    return FALSE;
}
extern "C" WINAPI BOOL PathRemoveFileSpecW(LPWSTR pszPath) {
    (void)pszPath;
    return FALSE;
}
extern "C" WINAPI BOOL PathAddExtensionA(LPSTR pszPath, LPCSTR pszExt) {
    (void)pszPath; (void)pszExt;
    return FALSE;
}
extern "C" WINAPI BOOL PathAddExtensionW(LPWSTR pszPath, LPCWSTR pszExt) {
    (void)pszPath; (void)pszExt;
    return FALSE;
}
extern "C" WINAPI BOOL PathCombineA(LPSTR pszDest, LPCSTR pszDir, LPCSTR pszFile) {
    (void)pszDest; (void)pszDir; (void)pszFile;
    return FALSE;
}
extern "C" WINAPI BOOL PathCombineW(LPWSTR pszDest, LPCWSTR pszDir, LPCWSTR pszFile) {
    (void)pszDest; (void)pszDir; (void)pszFile;
    return FALSE;
}
extern "C" WINAPI DWORD GetFileAttributesA(LPCSTR lpFileName) {
    (void)lpFileName;
    return 0xFFFFFFFF; // INVALID_FILE_ATTRIBUTES
}
extern "C" WINAPI DWORD GetFileAttributesW(LPCWSTR lpFileName) {
    (void)lpFileName;
    return 0xFFFFFFFF;
}
extern "C" WINAPI BOOL SetFileAttributesA(LPCSTR lpFileName, DWORD dwFileAttributes) {
    (void)lpFileName; (void)dwFileAttributes;
    return FALSE;
}
extern "C" WINAPI BOOL SetFileAttributesW(LPCWSTR lpFileName, DWORD dwFileAttributes) {
    (void)lpFileName; (void)dwFileAttributes;
    return FALSE;
}
extern "C" WINAPI HANDLE FindFirstFileA(LPCSTR lpFileName, LPWIN32_FIND_DATAA lpFindFileData) {
    (void)lpFileName; (void)lpFindFileData;
    return INVALID_HANDLE_VALUE;
}
extern "C" WINAPI HANDLE FindFirstFileW(LPCWSTR lpFileName, LPWIN32_FIND_DATAW lpFindFileData) {
    (void)lpFileName; (void)lpFindFileData;
    return INVALID_HANDLE_VALUE;
}
extern "C" WINAPI BOOL FindNextFileA(HANDLE hFindFile, LPWIN32_FIND_DATAA lpFindFileData) {
    (void)hFindFile; (void)lpFindFileData;
    return FALSE;
}
extern "C" WINAPI BOOL FindNextFileW(HANDLE hFindFile, LPWIN32_FIND_DATAW lpFindFileData) {
    (void)hFindFile; (void)lpFindFileData;
    return FALSE;
}
extern "C" WINAPI BOOL FindClose(HANDLE hFindFile) {
    (void)hFindFile;
    return TRUE;
}

// SHGetStockIconInfo
extern "C" WINAPI HRESULT SHGetStockIconInfo(SHSTOCKICONID siid, UINT uFlags, SHSTOCKICONINFO* psii) {
    (void)siid; (void)uFlags; (void)psii;
    return -2147467263;
}

extern "C" WINAPI BOOL WINAPI PathCanonicalizeW(LPWSTR lpszDst, LPCWSTR lpszSrc) {
    (void)lpszDst; (void)lpszSrc;
    return FALSE;
}

} // namespace win2lin
