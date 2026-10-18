// libwin_shell32.so - stub shim for shell32.dll (shell functions).

#include "common/logging.hpp"

#include <cstdint>

using HINSTANCE = void*;
using HWND = void*;
using LPCSTR = const char*;
using LPCWSTR = const wchar_t*;
using LPSTR = char*;
using LPWSTR = wchar_t*;
using UINT = uint32_t;
using INT = int32_t;
using DWORD = uint32_t;
using HANDLE = void*;
using HICON = void*;
using HCURSOR = void*;
using HBRUSH = void*;
using HBITMAP = void*;
using COLORREF = uint32_t;
using POINT = void*;
using SIZE = void*;
using RECT = void*;
using MSG = void*;
using WPARAM = uintptr_t;
using LPARAM = intptr_t;
using LRESULT = intptr_t;
using LPVOID = void*;
using LPSHELLEXECUTEINFOA = void*;
using LPSHELLEXECUTEINFOW = void*;
using PVOID = void*;
using IUnknown = void*;
using IShellFolder = void*;
using IPersistFolder = void*;
using IPersistIDList = void*;
using IContextMenu = void*;
using IDataObject = void*;
using IEnumIDList = void*;
using LPITEMIDLIST = void*;
using LPCITEMIDLIST = const void*;
using LPSHELLHOOKPROC = void*;
using HWND = void*;
using LPCMINVOKECOMMANDINFO = void*;
using LPCMINVOKECOMMANDINFOEX = void*;
using LPCVOID = const void*;
using HRESULT = int32_t;
using LPENUMIDLIST = void*;
using LPENUMSTRRET = void*;
using LPENUMFORMATETC = void*;
using LPENUMSTATSTG = void*;
using LPENUMSTATDATA = void*;
using LPFORMATETC = void*;
using LPSTGMEDIUM = void*;
using LPSTATSTG = void*;
using LPLOCKBYTES = void*;
using LPSTORAGE = void*;
using LPSTREAM = void*;
using LPMALLOC = void*;
using LPSHFILEINFOA = void*;
using LPSHFILEINFOW = void*;
using LPSHQUERYRBINFO = void*;
using LPSHSTOCKICONINFO = void*;
using LPSHDRAGIMAGE = void*;
using LPSHDRAGIMAGEINFO = void*;
using LPHVAC = void*;
using HDROP = void*;
using LPPOINT = void*;
using REFKNOWNFOLDERID = void*;
using PWSTR = wchar_t*;
using BOOL = int32_t;

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

namespace win2lin {

#define WINAPI __attribute__((ms_abi))

extern "C" WINAPI HINSTANCE ShellExecuteA(HWND hwnd, LPCSTR lpOperation, LPCSTR lpFile, LPCSTR lpParameters, LPCSTR lpDirectory, INT nShowCmd) {
    (void)hwnd; (void)lpOperation; (void)lpFile; (void)lpParameters; (void)lpDirectory; (void)nShowCmd;
    return (HINSTANCE)32;
}
extern "C" WINAPI HINSTANCE ShellExecuteW(HWND hwnd, LPCWSTR lpOperation, LPCWSTR lpFile, LPCWSTR lpParameters, LPCWSTR lpDirectory, INT nShowCmd) {
    (void)hwnd; (void)lpOperation; (void)lpFile; (void)lpParameters; (void)lpDirectory; (void)nShowCmd;
    return (HINSTANCE)32;
}
extern "C" WINAPI HINSTANCE FindExecutableA(LPCSTR lpFile, LPCSTR lpDir, LPSTR lpResult) {
    (void)lpFile; (void)lpDir; (void)lpResult;
    return (HINSTANCE)32;
}
extern "C" WINAPI HINSTANCE FindExecutableW(LPCWSTR lpFile, LPCWSTR lpDir, LPWSTR lpResult) {
    (void)lpFile; (void)lpDir; (void)lpResult;
    return (HINSTANCE)32;
}
extern "C" WINAPI int ShellAboutA(HWND hWnd, LPCSTR szApp, LPCSTR szOtherStuff, HICON hIcon) {
    (void)hWnd; (void)szApp; (void)szOtherStuff; (void)hIcon;
    return 0;
}
extern "C" WINAPI int ShellAboutW(HWND hWnd, LPCWSTR szApp, LPCWSTR szOtherStuff, HICON hIcon) {
    (void)hWnd; (void)szApp; (void)szOtherStuff; (void)hIcon;
    return 0;
}
extern "C" WINAPI int DragQueryFileA(HDROP hDrop, UINT iFile, LPSTR lpszFile, UINT cch) {
    (void)hDrop; (void)iFile; (void)lpszFile; (void)cch;
    return 0;
}
extern "C" WINAPI int DragQueryFileW(HDROP hDrop, UINT iFile, LPWSTR lpszFile, UINT cch) {
    (void)hDrop; (void)iFile; (void)lpszFile; (void)cch;
    return 0;
}
extern "C" WINAPI void DragFinish(HDROP hDrop) { (void)hDrop; }
extern "C" WINAPI void DragAcceptFiles(HWND hWnd, BOOL fAccept) {
    (void)hWnd; (void)fAccept;
}
extern "C" WINAPI void DragQueryPoint(HDROP hDrop, LPPOINT ppt) {
    (void)hDrop; (void)ppt;
}

// SHGetFolderPath / SHGetKnownFolderPath (stubs)
extern "C" WINAPI HRESULT SHGetFolderPathA(HWND hwnd, int csidl, HANDLE hToken, DWORD dwFlags, LPSTR pszPath) {
    (void)hwnd; (void)csidl; (void)hToken; (void)dwFlags; (void)pszPath;
    return -2147467263; // E_NOINTERFACE
}
extern "C" WINAPI HRESULT SHGetFolderPathW(HWND hwnd, int csidl, HANDLE hToken, DWORD dwFlags, LPWSTR pszPath) {
    (void)hwnd; (void)csidl; (void)hToken; (void)dwFlags; (void)pszPath;
    return -2147467263;
}
extern "C" WINAPI HRESULT SHGetKnownFolderPath(REFKNOWNFOLDERID rfid, DWORD dwFlags, HANDLE hToken, PWSTR* ppszPath) {
    (void)rfid; (void)dwFlags; (void)hToken; (void)ppszPath;
    return -2147467263;
}

extern "C" WINAPI BOOL WINAPI ShellExecuteExW(void*) {
    return FALSE;
}

} // namespace win2lin
