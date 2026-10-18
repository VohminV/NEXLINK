// libwin_ole32.so - stub shim for ole32.dll (COM).

#include "common/logging.hpp"

#include <cstdint>

using HINSTANCE = void*;
using HWND = void*;
using REFCLSID = void*;
using REFIID = void*;
using LPVOID = void*;
using HRESULT = int32_t;
using DWORD = uint32_t;
using ULONG = uint32_t;
using LPCVOID = const void*;
using LPVOID = void*;
using LPCOLESTR = const wchar_t*;
using LPOLESTR = wchar_t*;
using LPUNKNOWN = void*;
using LPCLASSFACTORY = void*;
using LPMalloc = void*;
using LPENUMMONIKER = void*;
using LPMONIKER = void*;
using LPBINDCTX = void*;
using LPENUMSTRING = void*;
using LPWSTR = wchar_t*;
using OLECHAR = wchar_t*;
using LPCOLESTR = const wchar_t*;
using CLSID = void*;
using IID = void*;
using IUnknown = void*;
using IClassFactory = void*;
using IMalloc = void*;
using IMoniker = void*;
using IBindCtx = void*;
using IEnumString = void*;
using IRunningObjectTable = void*;
using ICreateTypeLib = void*;
using ICreateTypeInfo = void*;
using ICreateErrorInfo = void*;
using IErrorInfo = void*;
using ITypeLib = void*;
using ITypeInfo = void*;
using ITypeComp = void*;
using IDispatch = void*;
using IEnumVARIANT = void*;
using IProvideClassInfo = void*;
using IConnectionPointContainer = void*;
using IConnectionPoint = void*;
using IEnumConnections = void*;
using IAdviseSink = void*;
using IDataObject = void*;
using IEnumFORMATETC = void*;
using IEnumSTATDATA = void*;
using IOleWindow = void*;
using IOleInPlaceSite = void*;
using IOleInPlaceFrame = void*;
using IOleInPlaceUIWindow = void*;
using IOleInPlaceObject = void*;
using IOleClientSite = void*;
using IOleObject = void*;
using IOleContainer = void*;
using IOleItemContainer = void*;
using IOleLink = void*;
using IOleCommandTarget = void*;
using IOlePreservePages = void*;
using IViewObject = void*;
using IViewObject2 = void*;
using IDataAdviseHolder = void*;
using IEnumOLEVERB = void*;
using IOleInPlaceActiveObject = void*;
using IOleControlSite = void*;
using IOleControl = void*;
using IQuickActivate = void*;
using IObjectSafety = void*;
using IObjectWithSite = void*;
using IOleCommandTarget2 = void*;
using LPCLSID = void*;
using LPIID = void*;
using PVARIANT = void*;
using REFPVARIANT = const void*;
using WORD = uint16_t;
using LCID = uint32_t;
using SYSKIND = int32_t;
using LPDATAADVISEHOLDER = void*;
using SIZE_T = uint64_t;
using LPOCPFIPARAMS = void*;
using LPFONTDESC = void*;
using LPICTDESC = void*;
using LPSTREAM = void*;
using IPicture = void*;
using IRecordInfo = void*;
using LPVERSION = void*;

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

namespace win2lin {

#define WINAPI __attribute__((ms_abi))

extern "C" WINAPI HRESULT CoInitialize(LPVOID pvReserved) {
    (void)pvReserved;
    return 0; // S_OK
}
extern "C" WINAPI HRESULT CoInitializeEx(LPVOID pvReserved, DWORD dwCoInit) {
    (void)pvReserved; (void)dwCoInit;
    return 0;
}
extern "C" WINAPI void CoUninitialize(void) {}
extern "C" WINAPI HRESULT CoCreateInstance(REFCLSID rclsid, LPUNKNOWN pUnkOuter, DWORD dwClsContext, REFIID riid, LPVOID* ppv) {
    (void)rclsid; (void)pUnkOuter; (void)dwClsContext; (void)riid; (void)ppv;
    return -2147467263; // E_NOINTERFACE
}
extern "C" WINAPI HRESULT CoCreateInstanceEx(...) { return -2147467263; }
extern "C" WINAPI HRESULT CoGetClassObject(REFCLSID rclsid, DWORD dwClsContext, LPVOID pServerInfo, REFIID riid, LPVOID* ppv) {
    (void)rclsid; (void)dwClsContext; (void)pServerInfo; (void)riid; (void)ppv;
    return -2147467263;
}
extern "C" WINAPI HRESULT CoRegisterClassObject(...) { return -2147467263; }
extern "C" WINAPI HRESULT CoRevokeClassObject(...) { return 0; }
extern "C" WINAPI HRESULT CoMarshalInterThreadInterfaceInStream(...) { return -2147467263; }
extern "C" WINAPI HRESULT CoGetInterfaceAndReleaseStream(...) { return -2147467263; }
extern "C" WINAPI HRESULT CoMarshalInterface(...) { return -2147467263; }
extern "C" WINAPI HRESULT CoUnmarshalInterface(...) { return -2147467263; }
extern "C" WINAPI HRESULT CoReleaseMarshalData(...) { return -2147467263; }
extern "C" WINAPI HRESULT CoDisconnectObject(...) { return -2147467263; }
extern "C" WINAPI HRESULT CoLockObjectExternal(...) { return -2147467263; }
extern "C" WINAPI HRESULT CoAddRefServerProcess(void) { return 0; }
extern "C" WINAPI HRESULT CoReleaseServerProcess(void) { return 0; }
extern "C" WINAPI HRESULT CoCreateFreeThreadedMarshaler(...) { return -2147467263; }
extern "C" WINAPI HRESULT CoGetMarshalSizeMax(...) { return -2147467263; }
extern "C" WINAPI HRESULT CoGetStandardMarshal(...) { return -2147467263; }
extern "C" WINAPI HRESULT CoGetObjectContext(...) { return -2147467263; }
extern "C" WINAPI HRESULT CoCreateInstanceFromApp(...) { return -2147467263; }
extern "C" WINAPI HRESULT CoCreateInstanceFromAppEx(...) { return -2147467263; }
extern "C" WINAPI HRESULT CoGetApartmentType(...) { return -2147467263; }
extern "C" WINAPI HRESULT CoGetCurrentLogicalThreadId(...) { return -2147467263; }
extern "C" WINAPI HRESULT CoGetCurrentProcessorNumber(...) { return -2147467263; }
extern "C" WINAPI HRESULT CoGetCurrentProcessorIndex(...) { return -2147467263; }
extern "C" WINAPI HRESULT CoGetCurrentActication(...) { return -2147467263; }

extern "C" WINAPI LPMalloc CoGetMalloc(DWORD dwMemContext) {
    (void)dwMemContext;
    return nullptr;
}
extern "C" WINAPI LPMalloc CoTaskMemAlloc(SIZE_T cb) {
    (void)cb;
    return nullptr;
}
extern "C" WINAPI void CoTaskMemFree(LPVOID pv) {
    (void)pv;
}
extern "C" WINAPI LPVOID CoTaskMemRealloc(LPVOID pv, SIZE_T cb) {
    (void)pv; (void)cb;
    return nullptr;
}

// String conversions (stubs)
extern "C" WINAPI int StringFromCLSID(REFCLSID rclsid, LPOLESTR* lplpsz) {
    (void)rclsid; (void)lplpsz;
    return 0;
}
extern "C" WINAPI int CLSIDFromString(LPCOLESTR lpsz, LPCLSID pclsid) {
    (void)lpsz; (void)pclsid;
    return -2147467263;
}
extern "C" WINAPI int CLSIDFromProgID(LPCOLESTR lpszProgID, LPCLSID pclsid) {
    (void)lpszProgID; (void)pclsid;
    return -2147467263;
}
extern "C" WINAPI int ProgIDFromCLSID(REFCLSID clsid, LPOLESTR* lplpszProgID) {
    (void)clsid; (void)lplpszProgID;
    return -2147467263;
}
extern "C" WINAPI int StringFromIID(REFIID rclsid, LPOLESTR* lplpsz) {
    (void)rclsid; (void)lplpsz;
    return 0;
}
extern "C" WINAPI int IIDFromString(LPCOLESTR lpsz, LPIID lpiid) {
    (void)lpsz; (void)lpiid;
    return -2147467263;
}

// Variant
extern "C" WINAPI HRESULT VariantInit(PVARIANT pvarg) {
    (void)pvarg;
    return 0;
}
extern "C" WINAPI HRESULT VariantClear(PVARIANT pvarg) {
    (void)pvarg;
    return 0;
}
extern "C" WINAPI HRESULT VariantCopy(PVARIANT pvargDest, REFPVARIANT pvarSrc) {
    (void)pvargDest; (void)pvarSrc;
    return -2147467263;
}
extern "C" WINAPI HRESULT VariantCopyInd(PVARIANT pvargDest, REFPVARIANT pvarSrc) {
    (void)pvargDest; (void)pvarSrc;
    return -2147467263;
}

// TypeLib
extern "C" WINAPI HRESULT LoadRegTypeLib(REFCLSID rclsid, WORD wVerMajor, WORD wVerMinor, LCID lcid, ITypeLib** pptlib) {
    (void)rclsid; (void)wVerMajor; (void)wVerMinor; (void)lcid; (void)pptlib;
    return -2147467263;
}
extern "C" WINAPI HRESULT RegisterTypeLib(ITypeLib* ptlib, LPOLESTR lpszFullPath, LPOLESTR lpszHelpDir) {
    (void)ptlib; (void)lpszFullPath; (void)lpszHelpDir;
    return -2147467263;
}
extern "C" WINAPI HRESULT UnRegisterTypeLib(REFCLSID rclsid, WORD wVerMajor, WORD wVerMinor, LCID lcid, SYSKIND sint) {
    (void)rclsid; (void)wVerMajor; (void)wVerMinor; (void)lcid; (void)sint;
    return -2147467263;
}

// Moniker / BindCtx
extern "C" WINAPI HRESULT CreateBindCtx(DWORD reserved, LPBINDCTX* ppbc) {
    (void)reserved; (void)ppbc;
    return -2147467263;
}
extern "C" WINAPI HRESULT CreateGenericComposite(LPMONIKER pmkFirst, LPMONIKER pmkRest, LPMONIKER* ppmkComposite) {
    (void)pmkFirst; (void)pmkRest; (void)ppmkComposite;
    return -2147467263;
}
extern "C" WINAPI HRESULT MkParseDisplayName(LPBINDCTX pbc, LPCOLESTR lpszDisplayName, ULONG* pchEaten, LPMONIKER* ppmk) {
    (void)pbc; (void)lpszDisplayName; (void)pchEaten; (void)ppmk;
    return -2147467263;
}

// Error info
extern "C" WINAPI HRESULT SetErrorInfo(ULONG dwReserved, IErrorInfo* perrinfo) {
    (void)dwReserved; (void)perrinfo;
    return 0;
}
extern "C" WINAPI HRESULT GetErrorInfo(ULONG dwReserved, IErrorInfo** pperrinfo) {
    (void)dwReserved; (void)pperrinfo;
    return -2147467263;
}
extern "C" WINAPI HRESULT CreateErrorInfo(IErrorInfo** pperrinfo) {
    (void)pperrinfo;
    return -2147467263;
}

// Data object
extern "C" WINAPI HRESULT CreateDataAdviseHolder(LPDATAADVISEHOLDER* ppDAHolder) {
    (void)ppDAHolder;
    return -2147467263;
}
extern "C" WINAPI HRESULT CreateStdInterfaceImpl(...) { return -2147467263; }
extern "C" WINAPI HRESULT CreateOleSite(...) { return -2147467263; }

// Misc
extern "C" WINAPI HRESULT CoInitializeSecurity(...) { return 0; }
extern "C" WINAPI HRESULT CoSwitchCallContext(...) { return -2147467263; }
extern "C" WINAPI HRESULT CoGetCallerTID(...) { return -2147467263; }
extern "C" WINAPI HRESULT CoImpersonateClient(void) { return 0; }
extern "C" WINAPI HRESULT CoRevertToSelf(void) { return 0; }
extern "C" WINAPI HRESULT CoQueryClientBlanket(...) { return -2147467263; }
extern "C" WINAPI HRESULT CoQueryProxyBlanket(...) { return -2147467263; }
extern "C" WINAPI HRESULT CoSetProxyBlanket(...) { return -2147467263; }
extern "C" WINAPI HRESULT CoCopyProxy(...) { return -2147467263; }
extern "C" WINAPI HRESULT CoCreateInstanceAsAdmin(...) { return -2147467263; }
extern "C" WINAPI HRESULT CoCreateInstanceAsAdminEx(...) { return -2147467263; }

} // namespace win2lin
