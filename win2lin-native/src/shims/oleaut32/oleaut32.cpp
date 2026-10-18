// libwin_oleaut32.so - stub shim for oleaut32.dll (COM automation / VARIANT).

#include "common/logging.hpp"

#include <cstdint>

using HRESULT = int32_t;
using DWORD = uint32_t;
using LCID = uint32_t;
using LPVOID = void*;
using LPCVOID = const void*;
using BSTR = void*;
using VARIANT = void*;
using VARIANTARG = void*;
using SAFEARRAY = void*;
using SAFEARRAYBOUND = void*;
using LPSAFEARRAY = void*;
using LPCBYTE = const uint8_t*;
using IUnknown = void*;
using IDispatch = void*;
using ICreateErrorInfo = void*;
using IErrorInfo = void*;
using DISPPARAMS = void*;
using EXCEPINFO = void*;
using ITypeInfo = void*;
using ITypeLib = void*;
using ICreateTypeInfo = void*;
using ICreateTypeLib = void*;
using IEnumVARIANT = void*;
using FUNCDESC = void*;
using TYPEDESC = void*;
using ELEMDESC = void*;
using IDLDESC = void*;
using TYPEATTR = void*;
using TYPELIBATTR = void*;
using VARDESC = void*;
using SYSKIND = int32_t;
using CALLCONV = int32_t;
using FUNCKIND = int32_t;
using TYPEKIND = int32_t;
using VARTYPE = uint16_t;
using CLEANLOCALSTORAGE = void*;
using OLECHAR = wchar_t*;
using LPCOLESTR = const wchar_t*;
using LPOLESTR = wchar_t*;
using USHORT = uint16_t;
using ULONG = uint32_t;
using IID = void*;
using CLSID = void*;
using REFGUID = const void*;
using REFCLSID = const void*;
using REFIID = const void*;
using UINT = uint32_t;
using LONG = int32_t;
using WORD = uint16_t;
using HWND = void*;
using LPOCPFIPARAMS = void*;
using LPFONTDESC = void*;
using LPICTDESC = void*;
using LPSTREAM = void*;
using IPicture = void*;
using IRecordInfo = void*;
using INTERFACEDATA = void*;
using BOOL = int32_t;

namespace win2lin {

#define WINAPI __attribute__((ms_abi))

// SysAlloc / SysFree
extern "C" WINAPI BSTR SysAllocString(const OLECHAR* psz) {
    (void)psz;
    return nullptr;
}
extern "C" WINAPI BSTR SysAllocStringByteCount(LPCBYTE lpBuffer, UINT Len) {
    (void)Len;
    return nullptr;
}
extern "C" WINAPI BSTR SysAllocStringLen(const OLECHAR* pszIn, UINT ui) {
    (void)pszIn; (void)ui;
    return nullptr;
}
extern "C" WINAPI void SysFreeString(BSTR bstrString) {
    (void)bstrString;
}
extern "C" WINAPI UINT SysStringLen(BSTR bstr) {
    (void)bstr;
    return 0;
}
extern "C" WINAPI UINT SysStringByteLen(BSTR bstr) {
    (void)bstr;
    return 0;
}
extern "C" WINAPI BSTR SysReAllocString(BSTR pbstr, const OLECHAR* psz) {
    (void)pbstr; (void)psz;
    return nullptr;
}
extern "C" WINAPI BSTR SysReAllocStringLen(BSTR pbstr, const OLECHAR* psz, UINT ui) {
    (void)pbstr; (void)psz; (void)ui;
    return nullptr;
}

// Variant
extern "C" WINAPI HRESULT VariantInit(VARIANTARG* pvarg) {
    (void)pvarg;
    return 0; // S_OK
}
extern "C" WINAPI HRESULT VariantClear(VARIANTARG* pvarg) {
    (void)pvarg;
    return 0;
}
extern "C" WINAPI HRESULT VariantCopy(VARIANTARG* pvargDest, const VARIANTARG* pvarSrc) {
    (void)pvargDest; (void)pvarSrc;
    return -2147467263; // E_NOINTERFACE
}
extern "C" WINAPI HRESULT VariantCopyInd(VARIANTARG* pvargDest, const VARIANTARG* pvarSrc) {
    (void)pvargDest; (void)pvarSrc;
    return -2147467263;
}
extern "C" WINAPI HRESULT VariantChangeType(VARIANTARG* pvargDest, const VARIANTARG* pvarSrc, USHORT wFlags, VARTYPE vt) {
    (void)pvargDest; (void)pvarSrc; (void)wFlags; (void)vt;
    return -2147467263;
}

// SafeArray
extern "C" WINAPI LPSAFEARRAY SafeArrayCreate(VARTYPE vt, UINT cDims, SAFEARRAYBOUND* rgsabound) {
    (void)vt; (void)cDims; (void)rgsabound;
    return nullptr;
}
extern "C" WINAPI LPSAFEARRAY SafeArrayCreateVector(VARTYPE vt, LONG lLower, ULONG cElements) {
    (void)vt; (void)lLower; (void)cElements;
    return nullptr;
}
extern "C" WINAPI HRESULT SafeArrayDestroy(LPSAFEARRAY psa) {
    (void)psa;
    return 0;
}
extern "C" WINAPI HRESULT SafeArrayResize(LPSAFEARRAY psa, ULONG cElements) {
    (void)psa; (void)cElements;
    return -2147467263;
}
extern "C" WINAPI HRESULT SafeArrayAllocDescriptor(UINT cDims, LPSAFEARRAY* ppsaOut) {
    (void)cDims; (void)ppsaOut;
    return -2147467263;
}
extern "C" WINAPI HRESULT SafeArrayAllocData(LPSAFEARRAY psa) {
    (void)psa;
    return -2147467263;
}
extern "C" WINAPI HRESULT SafeArrayCopyData(LPSAFEARRAY psaSource, LPSAFEARRAY psaDest) {
    (void)psaSource; (void)psaDest;
    return -2147467263;
}
extern "C" WINAPI HRESULT SafeArrayDestroyDescriptor(LPSAFEARRAY psa) {
    (void)psa;
    return -2147467263;
}
extern "C" WINAPI HRESULT SafeArrayDestroyData(LPSAFEARRAY psa) {
    (void)psa;
    return -2147467263;
}
extern "C" WINAPI HRESULT SafeArrayGetDim(LPSAFEARRAY psa) {
    (void)psa;
    return 0;
}
extern "C" WINAPI HRESULT SafeArrayGetElemsize(LPSAFEARRAY psa) {
    (void)psa;
    return 0;
}
extern "C" WINAPI HRESULT SafeArrayGetUBound(LPSAFEARRAY psa, UINT nDim, LONG* plUbound) {
    (void)psa; (void)nDim; (void)plUbound;
    return -2147467263;
}
extern "C" WINAPI HRESULT SafeArrayGetLBound(LPSAFEARRAY psa, UINT nDim, LONG* plLbound) {
    (void)psa; (void)nDim; (void)plLbound;
    return -2147467263;
}
extern "C" WINAPI HRESULT SafeArrayLock(LPSAFEARRAY psa) {
    (void)psa;
    return 0;
}
extern "C" WINAPI HRESULT SafeArrayUnlock(LPSAFEARRAY psa) {
    (void)psa;
    return 0;
}
extern "C" WINAPI HRESULT SafeArrayGetElement(LPSAFEARRAY psa, LONG* rgIndex, void* pv) {
    (void)psa; (void)rgIndex; (void)pv;
    return -2147467263;
}
extern "C" WINAPI HRESULT SafeArrayPutElement(LPSAFEARRAY psa, LONG* rgIndex, void* pv) {
    (void)psa; (void)rgIndex; (void)pv;
    return -2147467263;
}
extern "C" WINAPI HRESULT SafeArrayCopy(LPSAFEARRAY psa, LPSAFEARRAY* ppsaOut) {
    (void)psa; (void)ppsaOut;
    return -2147467263;
}

// TypeLib / TypeInfo
extern "C" WINAPI HRESULT LoadRegTypeLib(REFCLSID rclsid, WORD wVerMajor, WORD wVerMinor, LCID lcid, ITypeLib** pptlib) {
    (void)rclsid; (void)wVerMajor; (void)wVerMinor; (void)lcid; (void)pptlib;
    return -2147467263;
}
extern "C" WINAPI HRESULT RegisterTypeLib(ITypeLib* ptlib, OLECHAR* lpszFullPath, OLECHAR* lpszHelpDir) {
    (void)ptlib; (void)lpszFullPath; (void)lpszHelpDir;
    return -2147467263;
}
extern "C" WINAPI HRESULT UnRegisterTypeLib(REFCLSID rclsid, WORD wVerMajor, WORD wVerMinor, LCID lcid, SYSKIND sint) {
    (void)rclsid; (void)wVerMajor; (void)wVerMinor; (void)lcid; (void)sint;
    return -2147467263;
}
extern "C" WINAPI HRESULT CreateStdDispatch(IUnknown* punkOuter, void* pv, ITypeInfo* ptinfo, IUnknown** ppunk) {
    (void)punkOuter; (void)pv; (void)ptinfo; (void)ppunk;
    return -2147467263;
}
extern "C" WINAPI HRESULT OleCreatePropertyFrame(HWND hwndOwner, UINT x, UINT y, LPCOLESTR lpszCaption, DWORD cObjects, IUnknown** ppunk, DWORD cPages, CLSID* pPageClsID, LCID lcid, DWORD dwReserved, LPVOID pvReserved) {
    (void)hwndOwner; (void)x; (void)y; (void)lpszCaption; (void)cObjects; (void)ppunk; (void)cPages; (void)pPageClsID; (void)lcid; (void)dwReserved; (void)pvReserved;
    return -2147467263;
}
extern "C" WINAPI HRESULT OleCreatePropertyFrameIndirect(LPOCPFIPARAMS pParams) {
    (void)pParams;
    return -2147467263;
}
extern "C" WINAPI HRESULT OleCreateFontIndirect(LPFONTDESC lpFontDesc, REFIID riid, LPVOID* ppv) {
    (void)lpFontDesc; (void)riid; (void)ppv;
    return -2147467263;
}
extern "C" WINAPI HRESULT OleCreatePictureIndirect(LPICTDESC lpictdesc, REFIID riid, BOOL fOwn, LPVOID* ppv) {
    (void)lpictdesc; (void)riid; (void)fOwn; (void)ppv;
    return -2147467263;
}
extern "C" WINAPI HRESULT OleLoadPicture(LPSTREAM lpstream, LONG lSize, BOOL fRunmode, REFIID riid, LPVOID* ppvObj) {
    (void)lpstream; (void)lSize; (void)fRunmode; (void)riid; (void)ppvObj;
    return -2147467263;
}
extern "C" WINAPI HRESULT OleLoadPictureEx(LPSTREAM lpstream, LONG lSize, DWORD dwRunMode, REFIID riid, DWORD sizeMax, LPVOID* ppvObj) {
    (void)lpstream; (void)lSize; (void)dwRunMode; (void)riid; (void)sizeMax; (void)ppvObj;
    return -2147467263;
}
extern "C" WINAPI HRESULT OleLoadPictureFile(LPSTREAM lpstream, IPicture** lplpict) {
    (void)lpstream; (void)lplpict;
    return -2147467263;
}
extern "C" WINAPI HRESULT OleSavePictureFile(LPSTREAM lpstream, IPicture* lpict) {
    (void)lpstream; (void)lpict;
    return -2147467263;
}
// Misc
extern "C" WINAPI HRESULT GetRecordInfoFromTypeInfo(ITypeInfo* ptinfo, IRecordInfo** ppRecInfo) {
    (void)ptinfo; (void)ppRecInfo;
    return -2147467263;
}
extern "C" WINAPI HRESULT GetRecordInfoFromGuids(REFGUID guidTypeLib, ULONG uMajor, ULONG uMinor, LCID lcid, REFGUID guidIRecordInfo, IRecordInfo** ppRecInfo) {
    (void)guidTypeLib; (void)uMajor; (void)uMinor; (void)lcid; (void)guidIRecordInfo; (void)ppRecInfo;
    return -2147467263;
}
extern "C" WINAPI HRESULT SetVarConvLocale(LCID lcid) {
    (void)lcid;
    return 0;
}
extern "C" WINAPI LCID GetVarConvLocale(void) {
    return 0x0409; // en-US
}
extern "C" WINAPI HRESULT ClearCustData(void* lpCustData) {
    (void)lpCustData;
    return 0;
}
extern "C" WINAPI HRESULT CreateDispTypeInfo(INTERFACEDATA* ptidata, LCID lcid, ITypeInfo** ppti) {
    (void)ptidata; (void)lcid; (void)ppti;
    return -2147467263;
}
extern "C" WINAPI HRESULT GetErrorInfo(ULONG dwReserved, IErrorInfo** pperrinfo) {
    (void)dwReserved; (void)pperrinfo;
    return -2147467263;
}
extern "C" WINAPI HRESULT SetErrorInfo(ULONG dwReserved, IErrorInfo* perrinfo) {
    (void)dwReserved; (void)perrinfo;
    return 0;
}
extern "C" WINAPI HRESULT CreateErrorInfo(IErrorInfo** pperrinfo) {
    (void)pperrinfo;
    return -2147467263;
}

} // namespace win2lin
