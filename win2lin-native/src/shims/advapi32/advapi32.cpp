// libwin_advapi32.so - stub shim for advapi32.dll (registry, security, crypto).
// Exports Microsoft x64 ABI entry points; bodies are stubs that return
// safe defaults so games at least load instead of failing on missing imports.

#include "common/logging.hpp"

#include <cstdint>
#include <cstring>

using BOOL = int32_t;
using DWORD = uint32_t;
using LONG = int32_t;
using LPCSTR = const char*;
using LPCWSTR = const wchar_t*;
using LPSTR = char*;
using LPWSTR = wchar_t*;
using HANDLE = void*;
using HKEY = HANDLE*;
using PHKEY = HKEY*;
using LSTATUS = LONG;
using REGSAM = DWORD;
using DWORD_PTR = uint64_t;
using PSID = void*;
using PACL = void*;
using PSECURITY_DESCRIPTOR = void*;
using BOOL_PTR = BOOL*;
using LPVOID = void*;
using LPCVOID = const void*;
using LPDWORD = DWORD*;
using BYTE = uint8_t;
using LPBYTE = uint8_t*;
using PFILETIME = void*;
using FILETIME = void*;

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

namespace win2lin {

#define WINAPI __attribute__((ms_abi))

// Registry
extern "C" WINAPI LSTATUS RegOpenKeyExA(HKEY hKey, LPCSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult) {
    LOG_DEBUG("RegOpenKeyExA stub: %s", lpSubKey ? lpSubKey : "(null)");
    return -2; // ERROR_FILE_NOT_FOUND
}
extern "C" WINAPI LSTATUS RegOpenKeyExW(HKEY hKey, LPCWSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult) {
    return -2;
}
extern "C" WINAPI LSTATUS RegCreateKeyExA(HKEY hKey, LPCSTR lpSubKey, DWORD Reserved, LPSTR lpClass, DWORD dwOptions, REGSAM samDesired, LPVOID lpSecurityAttributes, PHKEY phkResult, LPDWORD lpdwDisposition) {
    return -2;
}
extern "C" WINAPI LSTATUS RegCreateKeyExW(HKEY hKey, LPCWSTR lpSubKey, DWORD Reserved, LPWSTR lpClass, DWORD dwOptions, REGSAM samDesired, LPVOID lpSecurityAttributes, PHKEY phkResult, LPDWORD lpdwDisposition) {
    return -2;
}
extern "C" WINAPI LSTATUS RegCloseKey(HKEY hKey) {
    return 0;
}
extern "C" WINAPI LSTATUS RegSetValueExA(HKEY hKey, LPCSTR lpValueName, DWORD Reserved, DWORD dwType, const BYTE* lpData, DWORD cbData) {
    return 0;
}
extern "C" WINAPI LSTATUS RegSetValueExW(HKEY hKey, LPCWSTR lpValueName, DWORD Reserved, DWORD dwType, const BYTE* lpData, DWORD cbData) {
    return 0;
}
extern "C" WINAPI LSTATUS RegQueryValueExA(HKEY hKey, LPCSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData) {
    return -2;
}
extern "C" WINAPI LSTATUS RegQueryValueExW(HKEY hKey, LPCWSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData) {
    return -2;
}
extern "C" WINAPI LSTATUS RegDeleteKeyA(HKEY hKey, LPCSTR lpSubKey) {
    return 0;
}
extern "C" WINAPI LSTATUS RegDeleteKeyW(HKEY hKey, LPCWSTR lpSubKey) {
    return 0;
}
extern "C" WINAPI LSTATUS RegDeleteValueA(HKEY hKey, LPCSTR lpValueName) {
    return 0;
}
extern "C" WINAPI LSTATUS RegDeleteValueW(HKEY hKey, LPCWSTR lpValueName) {
    return 0;
}
extern "C" WINAPI LSTATUS RegEnumKeyExA(HKEY hKey, DWORD dwIndex, LPSTR lpName, LPDWORD lpcName, LPDWORD lpReserved, LPSTR lpClass, LPDWORD lpcClass, PFILETIME lpftLastWriteTime) {
    return -259; // ERROR_NO_MORE_ITEMS
}
extern "C" WINAPI LSTATUS RegEnumValueA(HKEY hKey, DWORD dwIndex, LPSTR lpValueName, LPDWORD lpcValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData) {
    return -259;
}

// Crypto (stub)
extern "C" WINAPI BOOL CryptAcquireContextA(void* phProv, LPCSTR szContainer, LPCSTR szProvider, DWORD dwProvType, DWORD dwFlags) { return TRUE; }
extern "C" WINAPI BOOL CryptAcquireContextW(void* phProv, LPCWSTR szContainer, LPCWSTR szProvider, DWORD dwProvType, DWORD dwFlags) { return TRUE; }
extern "C" WINAPI BOOL CryptReleaseContext(void* hProv, DWORD dwFlags) { return TRUE; }
extern "C" WINAPI BOOL CryptGenKey(void* hProv, DWORD Algid, DWORD dwFlags, void* hKey) { return TRUE; }
extern "C" WINAPI BOOL CryptDestroyKey(void* hKey) { return TRUE; }
extern "C" WINAPI BOOL CryptEncrypt(void* hKey, void* hHash, BOOL Final, DWORD dwFlags, BYTE* pbData, DWORD* pdwDataLen, DWORD dwBufLen) { return TRUE; }
extern "C" WINAPI BOOL CryptDecrypt(void* hKey, void* hHash, BOOL Final, DWORD dwFlags, BYTE* pbData, DWORD* pdwDataLen) { return TRUE; }
extern "C" WINAPI BOOL CryptCreateHash(void* hProv, DWORD Algid, void* hKey, DWORD dwFlags, void* hHash) { return TRUE; }
extern "C" WINAPI BOOL CryptHashData(void* hHash, const BYTE* pbData, DWORD dwDataLen, DWORD dwFlags) { return TRUE; }
extern "C" WINAPI BOOL CryptDestroyHash(void* hHash) { return TRUE; }
extern "C" WINAPI BOOL CryptGetHashParam(void* hHash, DWORD dwParam, BYTE* pbData, DWORD* pdwDataLen, DWORD dwFlags) { return TRUE; }
extern "C" WINAPI BOOL CryptImportKey(void* hProv, const BYTE* pbData, DWORD dwDataLen, void* hPubKey, DWORD dwFlags, void* hKey) { return TRUE; }
extern "C" WINAPI BOOL CryptExportKey(void* hKey, void* hExpKey, DWORD dwBlobType, DWORD dwFlags, BYTE* pbData, DWORD* pdwDataLen) { return TRUE; }

// Misc
extern "C" WINAPI BOOL GetComputerNameA(LPSTR lpBuffer, LPDWORD nSize) {
    if (lpBuffer && nSize && *nSize > 0) {
        const char* name = "win2lin";
        size_t len = strlen(name);
        if (len + 1 <= *nSize) { memcpy(lpBuffer, name, len + 1); *nSize = (DWORD)len; return TRUE; }
    }
    return FALSE;
}
extern "C" WINAPI BOOL GetComputerNameW(LPWSTR lpBuffer, LPDWORD nSize) {
    if (lpBuffer && nSize && *nSize > 0) {
        const wchar_t* name = L"win2lin";
        size_t len = wcslen(name);
        if (len + 1 <= *nSize) { memcpy(lpBuffer, name, (len + 1) * sizeof(wchar_t)); *nSize = (DWORD)len; return TRUE; }
    }
    return FALSE;
}
extern "C" WINAPI DWORD GetLastError(void) { return 0; }
extern "C" WINAPI void SetLastError(DWORD dwErrCode) { (void)dwErrCode; }

// Account / token (stubs)
extern "C" WINAPI BOOL OpenProcessToken(HANDLE hProcess, DWORD dwDesiredAccess, void* TokenHandle) { return TRUE; }
extern "C" WINAPI BOOL GetTokenInformation(void* TokenHandle, void* TokenInformationClass, void* TokenInformation, DWORD TokenInformationLength, DWORD* ReturnLength) { return FALSE; }
extern "C" WINAPI BOOL CloseHandle(HANDLE hObject) { return TRUE; }

} // namespace win2lin
