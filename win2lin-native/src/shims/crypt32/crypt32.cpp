// libwin_crypt32.so - stub shim for crypt32.dll (Crypto API).

#include "common/logging.hpp"

#include <cstdint>

using DWORD = uint32_t;
using BOOL = int32_t;
using HANDLE = void*;
using HCRYPTPROV = HANDLE;
using HCRYPTKEY = HANDLE;
using HCRYPTHASH = HANDLE;
using HRESULT = int32_t;
using HCERTSTORE = HANDLE;
using PCCERT_CONTEXT = void*;
using PCCERT_CHAIN_CONTEXT = void*;
using LPCSTR = const char*;
using LPCWSTR = const wchar_t*;
using LPVOID = void*;
using LPCVOID = const void*;
using LPBYTE = uint8_t*;
using LPDWORD = DWORD*;
using PCERT_INFO = void*;
using LPSTR = char*;
using LPWSTR = wchar_t*;
using LPFILETIME = void*;
using CRYPT_BIT_BLOB = void*;
using CRYPT_ALGORITHM_IDENTIFIER = void*;
using CERT_NAME_BLOB = void*;
using CERT_INFO = void*;
using PCCERT_INFO = void*;
using PCERT_EXTENSION = void*;
using LONG = int32_t;


using BYTE = uint8_t;
using HCCERTCHAINENGINE = HANDLE;
using PCERT_CHAIN_PARA = void*;
constexpr int TRUE = 1;
constexpr int FALSE = 0;

namespace win2lin {

#define WINAPI __attribute__((ms_abi))

extern "C" WINAPI BOOL CryptAcquireContextA(HCRYPTPROV* phProv, LPCSTR szContainer, LPCSTR szProvider, DWORD dwProvType, DWORD dwFlags) {
    (void)phProv; (void)szContainer; (void)szProvider; (void)dwProvType; (void)dwFlags;
    return TRUE;
}
extern "C" WINAPI BOOL CryptAcquireContextW(HCRYPTPROV* phProv, LPCWSTR szContainer, LPCWSTR szProvider, DWORD dwProvType, DWORD dwFlags) {
    (void)phProv; (void)szContainer; (void)szProvider; (void)dwProvType; (void)dwFlags;
    return TRUE;
}
extern "C" WINAPI BOOL CryptReleaseContext(HCRYPTPROV hProv, DWORD dwFlags) {
    (void)hProv; (void)dwFlags;
    return TRUE;
}
extern "C" WINAPI BOOL CryptGenKey(HCRYPTPROV hProv, DWORD Algid, DWORD dwFlags, HCRYPTKEY* phKey) {
    (void)hProv; (void)Algid; (void)dwFlags; (void)phKey;
    return TRUE;
}
extern "C" WINAPI BOOL CryptDestroyKey(HCRYPTKEY hKey) {
    (void)hKey;
    return TRUE;
}
extern "C" WINAPI BOOL CryptEncrypt(HCRYPTKEY hKey, HCRYPTHASH hHash, BOOL Final, DWORD dwFlags, BYTE* pbData, DWORD* pdwDataLen, DWORD dwBufLen) {
    (void)hKey; (void)hHash; (void)Final; (void)dwFlags; (void)pbData; (void)pdwDataLen; (void)dwBufLen;
    return TRUE;
}
extern "C" WINAPI BOOL CryptDecrypt(HCRYPTKEY hKey, HCRYPTHASH hHash, BOOL Final, DWORD dwFlags, BYTE* pbData, DWORD* pdwDataLen) {
    (void)hKey; (void)hHash; (void)Final; (void)dwFlags; (void)pbData; (void)pdwDataLen;
    return TRUE;
}
extern "C" WINAPI BOOL CryptCreateHash(HCRYPTPROV hProv, DWORD Algid, HCRYPTKEY hKey, DWORD dwFlags, HCRYPTHASH* phHash) {
    (void)hProv; (void)Algid; (void)hKey; (void)dwFlags; (void)phHash;
    return TRUE;
}
extern "C" WINAPI BOOL CryptHashData(HCRYPTHASH hHash, const BYTE* pbData, DWORD dwDataLen, DWORD dwFlags) {
    (void)hHash; (void)pbData; (void)dwDataLen; (void)dwFlags;
    return TRUE;
}
extern "C" WINAPI BOOL CryptDestroyHash(HCRYPTHASH hHash) {
    (void)hHash;
    return TRUE;
}
extern "C" WINAPI BOOL CryptGetHashParam(HCRYPTHASH hHash, DWORD dwParam, BYTE* pbData, DWORD* pdwDataLen, DWORD dwFlags) {
    (void)hHash; (void)dwParam; (void)pbData; (void)pdwDataLen; (void)dwFlags;
    return TRUE;
}
extern "C" WINAPI BOOL CryptImportKey(HCRYPTPROV hProv, const BYTE* pbData, DWORD dwDataLen, HCRYPTKEY hPubKey, DWORD dwFlags, HCRYPTKEY* phKey) {
    (void)hProv; (void)pbData; (void)dwDataLen; (void)hPubKey; (void)dwFlags; (void)phKey;
    return TRUE;
}
extern "C" WINAPI BOOL CryptExportKey(HCRYPTKEY hKey, HCRYPTKEY hExpKey, DWORD dwBlobType, DWORD dwFlags, BYTE* pbData, DWORD* pdwDataLen) {
    (void)hKey; (void)hExpKey; (void)dwBlobType; (void)dwFlags; (void)pbData; (void)pdwDataLen;
    return TRUE;
}
extern "C" WINAPI BOOL CryptSetHashParam(HCRYPTHASH hHash, DWORD dwParam, LPCVOID pbData, DWORD dwFlags) {
    (void)hHash; (void)dwParam; (void)pbData; (void)dwFlags;
    return TRUE;
}
extern "C" WINAPI BOOL CryptSetKeyParam(HCRYPTKEY hKey, DWORD dwParam, LPCVOID pbData, DWORD dwFlags) {
    (void)hKey; (void)dwParam; (void)pbData; (void)dwFlags;
    return TRUE;
}
extern "C" WINAPI BOOL CryptGenRandom(HCRYPTPROV hProv, DWORD dwLen, BYTE* pbBuffer) {
    (void)hProv; (void)dwLen; (void)pbBuffer;
    return TRUE;
}
extern "C" WINAPI BOOL CryptGetKeyParam(HCRYPTKEY hKey, DWORD dwParam, LPBYTE pbData, DWORD* pdwDataLen, DWORD dwFlags) {
    (void)hKey; (void)dwParam; (void)pbData; (void)pdwDataLen; (void)dwFlags;
    return TRUE;
}
extern "C" WINAPI BOOL CryptSetProviderA(LPCSTR pszProvName, DWORD dwProvType) {
    (void)pszProvName; (void)dwProvType;
    return TRUE;
}
extern "C" WINAPI BOOL CryptSetProviderW(LPCWSTR pszProvName, DWORD dwProvType) {
    (void)pszProvName; (void)dwProvType;
    return TRUE;
}
extern "C" WINAPI BOOL CryptGetDefaultProviderA(DWORD dwProvType, LPDWORD pdwReserved, DWORD dwFlags, LPSTR pszProvName, LPDWORD pcbProvName) {
    (void)dwProvType; (void)pdwReserved; (void)dwFlags; (void)pszProvName; (void)pcbProvName;
    return TRUE;
}
extern "C" WINAPI BOOL CryptGetDefaultProviderW(DWORD dwProvType, LPDWORD pdwReserved, DWORD dwFlags, LPWSTR pszProvName, LPDWORD pcbProvName) {
    (void)dwProvType; (void)pdwReserved; (void)dwFlags; (void)pszProvName; (void)pcbProvName;
    return TRUE;
}
extern "C" WINAPI BOOL CryptEnumProvidersA(DWORD dwIndex, LPDWORD pdwReserved, DWORD dwFlags, LPDWORD pdwProvType, LPSTR pszProvName, LPDWORD pcbProvName) {
    (void)dwIndex; (void)pdwReserved; (void)dwFlags; (void)pdwProvType; (void)pszProvName; (void)pcbProvName;
    return FALSE;
}
extern "C" WINAPI BOOL CryptEnumProvidersW(DWORD dwIndex, LPDWORD pdwReserved, DWORD dwFlags, LPDWORD pdwProvType, LPWSTR pszProvName, LPDWORD pcbProvName) {
    (void)dwIndex; (void)pdwReserved; (void)dwFlags; (void)pdwProvType; (void)pszProvName; (void)pcbProvName;
    return FALSE;
}

// Cert functions
extern "C" WINAPI HCERTSTORE CertOpenStore(LPCSTR lpszStoreProvider, DWORD dwEncodingType, HCRYPTPROV hCryptProv, DWORD dwFlags, LPCVOID pvPara) {
    (void)lpszStoreProvider; (void)dwEncodingType; (void)hCryptProv; (void)dwFlags; (void)pvPara;
    return nullptr;
}
extern "C" WINAPI HCERTSTORE CertOpenStoreW(LPCWSTR lpszStoreProvider, DWORD dwEncodingType, HCRYPTPROV hCryptProv, DWORD dwFlags, LPCVOID pvPara) {
    (void)lpszStoreProvider; (void)dwEncodingType; (void)hCryptProv; (void)dwFlags; (void)pvPara;
    return nullptr;
}
extern "C" WINAPI BOOL CertCloseStore(HCERTSTORE hCertStore, DWORD dwFlags) {
    (void)hCertStore; (void)dwFlags;
    return TRUE;
}
extern "C" WINAPI PCCERT_CONTEXT CertFindCertificateInStore(HCERTSTORE hCertStore, DWORD dwCertEncodingType, DWORD dwFindFlags, DWORD dwFindType, LPCVOID pvFindPara, PCCERT_CONTEXT pPrevCertContext) {
    (void)hCertStore; (void)dwCertEncodingType; (void)dwFindFlags; (void)dwFindType; (void)pvFindPara; (void)pPrevCertContext;
    return nullptr;
}
extern "C" WINAPI BOOL CertAddCertificateContextToStore(HCERTSTORE hCertStore, PCCERT_CONTEXT pCertContext, DWORD dwAddDisposition, PCCERT_CONTEXT* ppStoreContext) {
    (void)hCertStore; (void)pCertContext; (void)dwAddDisposition; (void)ppStoreContext;
    return TRUE;
}
extern "C" WINAPI BOOL CertDeleteCertificateFromStore(PCCERT_CONTEXT pCertContext) {
    (void)pCertContext;
    return TRUE;
}
extern "C" WINAPI PCCERT_CONTEXT CertEnumCertificatesInStore(HCERTSTORE hCertStore, PCCERT_CONTEXT pPrevCertContext) {
    (void)hCertStore; (void)pPrevCertContext;
    return nullptr;
}
extern "C" WINAPI BOOL CertFreeCertificateContext(PCCERT_CONTEXT pCertContext) {
    (void)pCertContext;
    return TRUE;
}
extern "C" WINAPI BOOL CertGetCertificateContextProperty(PCCERT_CONTEXT pCertContext, DWORD dwPropId, LPVOID pvData, LPDWORD pcbData) {
    (void)pCertContext; (void)dwPropId; (void)pvData; (void)pcbData;
    return FALSE;
}
extern "C" WINAPI BOOL CertSetCertificateContextProperty(PCCERT_CONTEXT pCertContext, DWORD dwPropId, DWORD dwFlags, LPCVOID pvData, DWORD cbData) {
    (void)pCertContext; (void)dwPropId; (void)dwFlags; (void)pvData; (void)cbData;
    return TRUE;
}
extern "C" WINAPI PCCERT_CONTEXT CertCreateCertificateContext(DWORD dwCertEncodingType, const BYTE* pbCertEncoded, DWORD cbCertEncoded) {
    (void)dwCertEncodingType; (void)pbCertEncoded; (void)cbCertEncoded;
    return nullptr;
}
extern "C" WINAPI BOOL CertVerifySubjectCertificateContext(PCCERT_CONTEXT pSubject, PCCERT_CONTEXT pIssuer) {
    (void)pSubject; (void)pIssuer;
    return TRUE;
}
extern "C" WINAPI DWORD CertVerifyTimeValidity(LPFILETIME pTimeToVerify, PCERT_INFO pCertInfo) {
    (void)pTimeToVerify; (void)pCertInfo;
    return 0;
}
extern "C" WINAPI HRESULT CertVerifyCertificateChainPolicy(LPCSTR pszPolicyOid, PCCERT_CHAIN_CONTEXT pChainContext, LPVOID pPolicyExtra, LPVOID pPolicyStatus) {
    (void)pszPolicyOid; (void)pChainContext; (void)pPolicyExtra; (void)pPolicyStatus;
    return 0;
}
extern "C" WINAPI HRESULT CertGetCertificateChain(HCCERTCHAINENGINE hChainEngine, PCCERT_CONTEXT pCertContext, LPFILETIME pTime, HCERTSTORE hAdditionalStore, PCERT_CHAIN_PARA pChainPara, DWORD dwFlags, LPVOID pvReserved, PCCERT_CHAIN_CONTEXT* ppChainContext) {
    (void)hChainEngine; (void)pCertContext; (void)pTime; (void)hAdditionalStore; (void)pChainPara; (void)dwFlags; (void)pvReserved; (void)ppChainContext;
    return 0;
}
extern "C" WINAPI BOOL CertFreeCertificateChain(PCCERT_CHAIN_CONTEXT pChainContext) {
    (void)pChainContext;
    return TRUE;
}

using BYTE = uint8_t;
using HCCERTCHAINENGINE = HANDLE;
using PCERT_CHAIN_PARA = void*;

} // namespace win2lin
