// libwin_version.so - stub shim for version.dll (file version info).

#include "common/logging.hpp"

#include <cstdint>

using DWORD = uint32_t;
using LPVOID = void*;
using LPCSTR = const char*;
using LPCWSTR = const wchar_t*;
using UINT = uint32_t;
using LPTSTR = char*;
using LPWSTR = wchar_t*;
using BOOL = int32_t;
using HANDLE = void*;
using LPDWORD = DWORD*;
using LPCVOID = const void*;
using PUINT = UINT*;
using LPSTR = char*;

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

namespace win2lin {

#define WINAPI __attribute__((ms_abi))

extern "C" WINAPI DWORD GetFileVersionInfoSizeA(LPCSTR lpszFileName, LPDWORD lpdwHandle) {
    (void)lpszFileName; (void)lpdwHandle;
    return 0;
}
extern "C" WINAPI DWORD GetFileVersionInfoSizeW(LPCWSTR lpszFileName, LPDWORD lpdwHandle) {
    (void)lpszFileName; (void)lpdwHandle;
    return 0;
}
extern "C" WINAPI BOOL GetFileVersionInfoA(LPCSTR lpszFileName, DWORD dwHandle, DWORD dwLen, LPVOID lpData) {
    (void)lpszFileName; (void)dwHandle; (void)dwLen; (void)lpData;
    return FALSE;
}
extern "C" WINAPI BOOL GetFileVersionInfoW(LPCWSTR lpszFileName, DWORD dwHandle, DWORD dwLen, LPVOID lpData) {
    (void)lpszFileName; (void)dwHandle; (void)dwLen; (void)lpData;
    return FALSE;
}
extern "C" WINAPI BOOL VerQueryValueA(LPCVOID pBlock, LPCSTR lpSubBlock, LPVOID* lplpBuffer, PUINT puLen) {
    (void)pBlock; (void)lpSubBlock; (void)lplpBuffer; (void)puLen;
    return FALSE;
}
extern "C" WINAPI BOOL VerQueryValueW(LPCVOID pBlock, LPCWSTR lpSubBlock, LPVOID* lplpBuffer, PUINT puLen) {
    (void)pBlock; (void)lpSubBlock; (void)lplpBuffer; (void)puLen;
    return FALSE;
}
extern "C" WINAPI DWORD VerGetFileSize(LPVOID lplpBuffer, PUINT puLen) {
    (void)lplpBuffer; (void)puLen;
    return 0;
}
extern "C" WINAPI BOOL VerGetFileInfo(LPVOID lplpBuffer, DWORD dwLevel, LPVOID lpBuffer, PUINT puLen) {
    (void)lplpBuffer; (void)dwLevel; (void)lpBuffer; (void)puLen;
    return FALSE;
}
extern "C" WINAPI BOOL VerSetFileInfo(LPVOID lplpBuffer, DWORD dwLevel, LPVOID lpBuffer, UINT uLen) {
    (void)lplpBuffer; (void)dwLevel; (void)lpBuffer; (void)uLen;
    return FALSE;
}
extern "C" WINAPI DWORD VerInstallFileA(DWORD uFlags, LPCSTR szSrcFileName, LPCSTR szDestFileName, LPCSTR szTmpFileName, LPCSTR szSourcePath, LPCSTR szDestPath, LPTSTR szCurDir, PUINT puCurDirLen) {
    (void)uFlags; (void)szSrcFileName; (void)szDestFileName; (void)szTmpFileName; (void)szSourcePath; (void)szDestPath; (void)szCurDir; (void)puCurDirLen;
    return 0;
}
extern "C" WINAPI DWORD VerInstallFileW(DWORD uFlags, LPCWSTR szSrcFileName, LPCWSTR szDestFileName, LPCWSTR szTmpFileName, LPCWSTR szSourcePath, LPCWSTR szDestPath, LPWSTR szCurDir, PUINT puCurDirLen) {
    (void)uFlags; (void)szSrcFileName; (void)szDestFileName; (void)szTmpFileName; (void)szSourcePath; (void)szDestPath; (void)szCurDir; (void)puCurDirLen;
    return 0;
}
extern "C" WINAPI DWORD VerLanguageNameA(DWORD wLang, LPSTR szLang, DWORD nSize) {
    (void)wLang; (void)szLang; (void)nSize;
    return 0;
}
extern "C" WINAPI DWORD VerLanguageNameW(DWORD wLang, LPWSTR szLang, DWORD nSize) {
    (void)wLang; (void)szLang; (void)nSize;
    return 0;
}

} // namespace win2lin
