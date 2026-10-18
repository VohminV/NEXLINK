// libwin_secur32.so - stub shim for secur32.dll (Security Support Provider Interface).

#include "common/logging.hpp"

#include <cstdint>

using HANDLE = void*;
using DWORD = uint32_t;
using BOOL = int32_t;
using HRESULT = int32_t;
using LPCSTR = const char*;
using LPCWSTR = const wchar_t*;
using LPVOID = void*;
using LPCVOID = const void*;
using LPTSTR = char*;
using LPWSTR = wchar_t*;
using LONG = int32_t;
using ULONG = uint32_t;
using PVOID = void*;
using PCtxtHandle = void*;
using PCredHandle = void*;
using PSecBufferDesc = void*;
using PSecurityString = void*;


using SECURITY_STATUS = HRESULT;

namespace win2lin {

#define WINAPI __attribute__((ms_abi))

extern "C" WINAPI HRESULT AcquireCredentialsHandleA(LPCSTR pszPrincipal, LPCSTR pszPackage, DWORD fCredentialUse, LPCVOID pvLogonId, LPCVOID pvAuthData, void* pGetKeyFn, LPCVOID pvGetKeyArgument, PCredHandle phCredential, PVOID ptsExpiry) {
    (void)pszPrincipal; (void)pszPackage; (void)fCredentialUse; (void)pvLogonId; (void)pvAuthData; (void)pGetKeyFn; (void)pvGetKeyArgument; (void)phCredential; (void)ptsExpiry;
    return 0;
}
extern "C" WINAPI HRESULT AcquireCredentialsHandleW(LPCWSTR pszPrincipal, LPCWSTR pszPackage, DWORD fCredentialUse, LPCVOID pvLogonId, LPCVOID pvAuthData, void* pGetKeyFn, LPCVOID pvGetKeyArgument, PCredHandle phCredential, PVOID ptsExpiry) {
    (void)pszPrincipal; (void)pszPackage; (void)fCredentialUse; (void)pvLogonId; (void)pvAuthData; (void)pGetKeyFn; (void)pvGetKeyArgument; (void)phCredential; (void)ptsExpiry;
    return 0;
}
extern "C" WINAPI HRESULT FreeCredentialsHandle(PCredHandle phCredential) {
    (void)phCredential;
    return 0;
}
extern "C" WINAPI HRESULT QueryCredentialsAttributesA(PCredHandle phCredential, DWORD ulAttribute, PVOID pBuffer) {
    (void)phCredential; (void)ulAttribute; (void)pBuffer;
    return 0;
}
extern "C" WINAPI HRESULT QueryCredentialsAttributesW(PCredHandle phCredential, DWORD ulAttribute, PVOID pBuffer) {
    (void)phCredential; (void)ulAttribute; (void)pBuffer;
    return 0;
}
extern "C" WINAPI HRESULT ImpersonateSecurityContext(PCtxtHandle phContext) {
    (void)phContext;
    return 0;
}
extern "C" WINAPI HRESULT RevertSecurityContext(void) {
    return 0;
}
extern "C" WINAPI HRESULT QueryContextAttributesA(PCtxtHandle phContext, DWORD ulAttribute, PVOID pBuffer) {
    (void)phContext; (void)ulAttribute; (void)pBuffer;
    return 0;
}
extern "C" WINAPI HRESULT QueryContextAttributesW(PCtxtHandle phContext, DWORD ulAttribute, PVOID pBuffer) {
    (void)phContext; (void)ulAttribute; (void)pBuffer;
    return 0;
}
extern "C" WINAPI HRESULT InitializeSecurityContextA(PCredHandle phCredential, PCtxtHandle phContext, LPCSTR pszTargetName, DWORD fContextReq, DWORD Reserved1, DWORD TargetDataRep, PSecBufferDesc pInput, DWORD Reserved2, PCtxtHandle phNewContext, PSecBufferDesc pOutput, DWORD* pfContextAttr, PVOID ptsExpiry) {
    (void)phCredential; (void)phContext; (void)pszTargetName; (void)fContextReq; (void)Reserved1; (void)TargetDataRep; (void)pInput; (void)Reserved2; (void)phNewContext; (void)pOutput; (void)pfContextAttr; (void)ptsExpiry;
    return 0;
}
extern "C" WINAPI HRESULT InitializeSecurityContextW(PCredHandle phCredential, PCtxtHandle phContext, LPCWSTR pszTargetName, DWORD fContextReq, DWORD Reserved1, DWORD TargetDataRep, PSecBufferDesc pInput, DWORD Reserved2, PCtxtHandle phNewContext, PSecBufferDesc pOutput, DWORD* pfContextAttr, PVOID ptsExpiry) {
    (void)phCredential; (void)phContext; (void)pszTargetName; (void)fContextReq; (void)Reserved1; (void)TargetDataRep; (void)pInput; (void)Reserved2; (void)phNewContext; (void)pOutput; (void)pfContextAttr; (void)ptsExpiry;
    return 0;
}
extern "C" WINAPI HRESULT AcceptSecurityContext(PCredHandle phCredential, PCtxtHandle phContext, PSecBufferDesc pInput, DWORD fContextReq, DWORD TargetDataRep, PCtxtHandle phNewContext, PSecBufferDesc pOutput, DWORD* pfContextAttr, PVOID ptsExpiry) {
    (void)phCredential; (void)phContext; (void)pInput; (void)fContextReq; (void)TargetDataRep; (void)phNewContext; (void)pOutput; (void)pfContextAttr; (void)ptsExpiry;
    return 0;
}
extern "C" WINAPI HRESULT DeleteSecurityContext(PCtxtHandle phContext) {
    (void)phContext;
    return 0;
}
extern "C" WINAPI HRESULT ApplyControlToken(PCtxtHandle phContext, PSecBufferDesc pInput) {
    (void)phContext; (void)pInput;
    return 0;
}
extern "C" WINAPI HRESULT QueryContextAttributesExA(PCtxtHandle phContext, DWORD ulAttribute, PVOID pBuffer, DWORD flags) {
    (void)phContext; (void)ulAttribute; (void)pBuffer; (void)flags;
    return 0;
}
extern "C" WINAPI HRESULT QueryContextAttributesExW(PCtxtHandle phContext, DWORD ulAttribute, PVOID pBuffer, DWORD flags) {
    (void)phContext; (void)ulAttribute; (void)pBuffer; (void)flags;
    return 0;
}
extern "C" WINAPI HRESULT CompleteAuthToken(PCtxtHandle phContext, PSecBufferDesc pToken) {
    (void)phContext; (void)pToken;
    return 0;
}

using SECURITY_STATUS = HRESULT;

} // namespace win2lin
