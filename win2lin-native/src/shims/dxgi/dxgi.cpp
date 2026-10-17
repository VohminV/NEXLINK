// libwin_dxgi.so - skeleton DXGI shim with mock COM objects (Phase 1.5).
//
// Games never talk to D3D11 before asking DXGI "which adapters exist?".
// If CreateDXGIFactory* fails, they abort before device creation. This shim
// therefore provides a working (mock) object graph:
//
//   CreateDXGIFactory{,1,2} -> MockFactory  (IDXGIFactory2-capable)
//     EnumAdapters/EnumAdapters1(0) -> MockAdapter ("NEXLINK Vulkan Adapter",
//                                       8 GiB dedicated VRAM)
//     EnumAdapters1(1..)            -> DXGI_ERROR_NOT_FOUND
//     EnumOutputs(0) on adapter     -> MockOutput (1920x1080 desktop)
//
// COM layout: interfaces are modelled as C++ classes annotated with
// __attribute__((ms_abi)) so every virtual method (and therefore every vtable
// slot) uses the Microsoft x64 ABI, which is what the relinked PE expects.
// Single inheritance keeps the vtable order identical to the COM layout:
// IUnknown -> IDXGIObject -> IDXGIFactory -> IDXGIFactory1 -> IDXGIFactory2.
//
// Unimplemented methods log "[NEXLINK DXGI UNIMPL] <name>" and return safe
// values (E_NOTIMPL / S_OK / DXGI_ERROR_NOT_FOUND) to keep the app alive.

#include "common/win32.hpp"

#include <cstdio>
#include <cstring>

#include <atomic>

using namespace win2lin::win;

namespace {

// --------------------------------------------------------------------------
// Win32/DXGI ABI types not present in common/win32.hpp
// --------------------------------------------------------------------------
struct GUID {
    uint32_t Data1;
    uint16_t Data2;
    uint16_t Data3;
    uint8_t Data4[8];
};

using REFIID = const GUID&;
using REFGUID = const GUID&;
using LUID = uint64_t; // struct { DWORD LowPart; LONG HighPart; } - 8 bytes

constexpr HRESULT S_OK_ = 0;
constexpr HRESULT E_NOTIMPL_ = static_cast<HRESULT>(0x80004001LL);
constexpr HRESULT E_NOINTERFACE_ = static_cast<HRESULT>(0x80004002LL);
constexpr HRESULT E_POINTER_ = static_cast<HRESULT>(0x80004003LL);
constexpr HRESULT DXGI_ERROR_INVALID_CALL_ = static_cast<HRESULT>(0x887A0001LL);
constexpr HRESULT DXGI_ERROR_NOT_FOUND_ = static_cast<HRESULT>(0x887A0002LL);
constexpr HRESULT DXGI_ERROR_MORE_DATA_ = static_cast<HRESULT>(0x887A0003LL);
constexpr HRESULT DXGI_ERROR_UNSUPPORTED_ = static_cast<HRESULT>(0x887A0004LL);

struct DXGI_RATIONAL { UINT Numerator; UINT Denominator; };
struct DXGI_MODE_DESC {
    UINT Width;
    UINT Height;
    DXGI_RATIONAL RefreshRate;
    UINT Format;            // DXGI_FORMAT
    UINT ScanlineOrdering;  // DXGI_MODE_SCANLINE_ORDER
    UINT Scaling;           // DXGI_MODE_SCALING
};
struct DXGI_OUTPUT_DESC {
    WCHAR DeviceName[32];
    RECT DesktopCoordinates;
    BOOL AttachedToDesktop;
    UINT Rotation; // DXGI_MODE_ROTATION
    void* Monitor; // HMONITOR
};
struct DXGI_ADAPTER_DESC {
    WCHAR Description[128];
    UINT VendorId;
    UINT DeviceId;
    UINT SubSysId;
    UINT Revision;
    SIZE_T DedicatedVideoMemory;
    SIZE_T DedicatedSystemMemory;
    SIZE_T SharedSystemMemory;
    LUID AdapterLuid;
};
struct DXGI_ADAPTER_DESC1 {
    WCHAR Description[128];
    UINT VendorId;
    UINT DeviceId;
    UINT SubSysId;
    UINT Revision;
    SIZE_T DedicatedVideoMemory;
    SIZE_T DedicatedSystemMemory;
    SIZE_T SharedSystemMemory;
    LUID AdapterLuid;
    UINT Flags;
};
struct DXGI_ADAPTER_DESC2 {
    WCHAR Description[128];
    UINT VendorId;
    UINT DeviceId;
    UINT SubSysId;
    UINT Revision;
    SIZE_T DedicatedVideoMemory;
    SIZE_T DedicatedSystemMemory;
    SIZE_T SharedSystemMemory;
    LUID AdapterLuid;
    UINT Flags;
    UINT GraphicsPreemptionGranularity;
    UINT ComputePreemptionGranularity;
};
struct DXGI_FRAME_STATISTICS {
    UINT PresentCount;
    UINT PresentTime; // LARGE_INTEGER
    UINT SyncRefreshCount;
    UINT SyncQPCTime;
    UINT SyncGPUTime;
};

// IIDs (verified against mingw-w64 dxgi*.h).
constexpr GUID IID_IUnknown_ =
    {0x00000000, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
constexpr GUID IID_IDXGIObject_ =
    {0xAEC22FB8, 0x76F3, 0x4639, {0x9B, 0xE0, 0x28, 0xEB, 0x43, 0xA6, 0x7A, 0x2E}};
constexpr GUID IID_IDXGIFactory_ =
    {0x7B7166EC, 0x21C7, 0x44AE, {0xB2, 0x1A, 0xC9, 0xAE, 0x32, 0x1A, 0xE3, 0x69}};
constexpr GUID IID_IDXGIFactory1_ =
    {0x770AAE78, 0xF26F, 0x4DBA, {0xA8, 0x29, 0x25, 0x3C, 0x83, 0xD1, 0xB3, 0x87}};
constexpr GUID IID_IDXGIFactory2_ =
    {0x50C83A1C, 0xE072, 0x4C48, {0x87, 0xB0, 0x36, 0x30, 0xFA, 0x36, 0xA6, 0xD0}};
constexpr GUID IID_IDXGIAdapter_ =
    {0x2411E7E1, 0x12AC, 0x4CCF, {0xBD, 0x14, 0x97, 0x98, 0xE8, 0x53, 0x4D, 0xC0}};
constexpr GUID IID_IDXGIAdapter1_ =
    {0x29038F61, 0x3839, 0x4626, {0x91, 0xFD, 0x08, 0x68, 0x79, 0x01, 0x1A, 0x05}};
constexpr GUID IID_IDXGIAdapter2_ =
    {0x0AA1AE0A, 0xFA0E, 0x4B84, {0x86, 0x44, 0xE0, 0x5F, 0xF8, 0xE5, 0xAC, 0xB5}};
constexpr GUID IID_IDXGIOutput_ =
    {0xAE02EEDB, 0xC735, 0x4690, {0x8D, 0x52, 0x5A, 0x8D, 0xC2, 0x02, 0x13, 0xAA}};

bool guid_eq(const GUID& a, const GUID& b) {
    return std::memcmp(&a, &b, sizeof(GUID)) == 0;
}

#define DXGI_UNIMPL(name) fprintf(stderr, "[NEXLINK DXGI UNIMPL] %s\n", name)

#define DXGI_INFO(fmt, ...) fprintf(stderr, "[NEXLINK DXGI] " fmt "\n", ##__VA_ARGS__)

// --------------------------------------------------------------------------
// COM interface mirrors. The ms_abi class attribute makes every virtual
// method (and the vtable itself) follow the Microsoft x64 ABI. Single
// inheritance preserves COM vtable slot order.
// --------------------------------------------------------------------------
struct __attribute__((ms_abi)) IUnknown {
    virtual HRESULT WINAPI QueryInterface(REFIID riid, void** ppv) = 0;
    virtual ULONG WINAPI AddRef() = 0;
    virtual ULONG WINAPI Release() = 0;

protected:
    ~IUnknown() = default;
};

struct __attribute__((ms_abi)) IDXGIObject : public IUnknown {
    virtual HRESULT WINAPI SetPrivateData(REFGUID, UINT, const void*) = 0;
    virtual HRESULT WINAPI SetPrivateDataInterface(REFGUID, const IUnknown*) = 0;
    virtual HRESULT WINAPI GetPrivateData(REFGUID, UINT*, void*) = 0;
    virtual HRESULT WINAPI GetParent(REFIID, void**) = 0;
};

struct IDXGIAdapter; // fwd

struct __attribute__((ms_abi)) IDXGIFactory : public IDXGIObject {
    virtual HRESULT WINAPI EnumAdapters(UINT index, IDXGIAdapter** ppAdapter) = 0;
    virtual HRESULT WINAPI MakeWindowAssociation(HWND, UINT) = 0;
    virtual HRESULT WINAPI GetWindowAssociation(HWND* pHWND) = 0;
    virtual HRESULT WINAPI CreateSwapChain(void* pDevice, void* pDesc,
                                           void** ppSwapChain) = 0;
    virtual HRESULT WINAPI CreateSoftwareAdapter(HMODULE, IDXGIAdapter** ppAdapter) = 0;
};

struct __attribute__((ms_abi)) IDXGIFactory1 : public IDXGIFactory {
    virtual HRESULT WINAPI EnumAdapters1(UINT index, IDXGIAdapter** ppAdapter) = 0;
    virtual BOOL WINAPI IsCurrent() = 0;
};

struct __attribute__((ms_abi)) IDXGIFactory2 : public IDXGIFactory1 {
    virtual BOOL WINAPI IsWindowedStereoEnabled() = 0;
    virtual HRESULT WINAPI CreateSwapChainForHwnd(void* pDevice, HWND, const void*,
                                                  const void*, void* pRestrictOutput,
                                                  void** ppSwapChain) = 0;
    virtual HRESULT WINAPI CreateSwapChainForCoreWindow(void* pDevice, void* pWindow,
                                                        const void*, void* pRestrictOutput,
                                                        void** ppSwapChain) = 0;
    virtual HRESULT WINAPI GetSharedResourceAdapterLuid(LUID* pLuid) = 0;
    virtual HRESULT WINAPI RegisterStereoStatusWindow(HWND, UINT, DWORD* pCookie) = 0;
    virtual HRESULT WINAPI RegisterStereoStatusEvent(void* hEvent, DWORD* pCookie) = 0;
    virtual void WINAPI UnregisterStereoStatus(DWORD cookie) = 0;
    virtual HRESULT WINAPI RegisterOcclusionStatusWindow(HWND, UINT, DWORD* pCookie) = 0;
    virtual HRESULT WINAPI RegisterOcclusionStatusEvent(void* hEvent, DWORD* pCookie) = 0;
    virtual void WINAPI UnregisterOcclusionStatus(DWORD cookie) = 0;
    virtual HRESULT WINAPI CreateSwapChainForComposition(void* pDevice, const void*,
                                                         void* pRestrictOutput,
                                                         void** ppSwapChain) = 0;
};

struct __attribute__((ms_abi)) IDXGIAdapter : public IDXGIObject {
    virtual HRESULT WINAPI EnumOutputs(UINT index, void** ppOutput) = 0; // IDXGIOutput*
    virtual HRESULT WINAPI GetDesc(DXGI_ADAPTER_DESC* pDesc) = 0;
    virtual HRESULT WINAPI CheckInterfaceSupport(REFGUID InterfaceName,
                                                 int64_t* pUmdVersion) = 0;
};

struct __attribute__((ms_abi)) IDXGIAdapter1 : public IDXGIAdapter {
    virtual HRESULT WINAPI GetDesc1(DXGI_ADAPTER_DESC1* pDesc) = 0;
};

struct __attribute__((ms_abi)) IDXGIAdapter2 : public IDXGIAdapter1 {
    virtual HRESULT WINAPI GetDesc2(DXGI_ADAPTER_DESC2* pDesc) = 0;
};

struct __attribute__((ms_abi)) IDXGIOutput : public IDXGIObject {
    virtual HRESULT WINAPI GetDesc(DXGI_OUTPUT_DESC* pDesc) = 0;
    virtual HRESULT WINAPI GetDisplayModeList(UINT format, UINT flags, UINT* pNumModes,
                                              DXGI_MODE_DESC* pModeDescs) = 0;
    virtual HRESULT WINAPI FindClosestMatchingMode(const DXGI_MODE_DESC* pModeToMatch,
                                                   DXGI_MODE_DESC* pClosestMatch,
                                                   void* pConcernedDevice) = 0;
    virtual HRESULT WINAPI WaitForVBlank() = 0;
    virtual HRESULT WINAPI TakeOwnership(void* pDevice, BOOL Exclusive) = 0;
    virtual void WINAPI ReleaseOwnership() = 0;
    virtual HRESULT WINAPI GetGammaControlCapabilities(void* pCaps) = 0;
    virtual HRESULT WINAPI SetGammaControl(const void* pSet) = 0;
    virtual HRESULT WINAPI GetGammaControl(void* pSet) = 0;
    virtual HRESULT WINAPI SetDisplaySurface(void* pScanoutSurface) = 0;
    virtual HRESULT WINAPI GetDisplaySurfaceData(void* pDestination) = 0;
    virtual HRESULT WINAPI GetFrameStatistics(DXGI_FRAME_STATISTICS* pStats) = 0;
};

// --------------------------------------------------------------------------
// Shared COM scaffolding (refcount + IUnknown/IDXGIObject body).
//
// The interface the mock implements is a template parameter: a mock must
// inherit the *full* interface (e.g. IDXGIAdapter2) so its GetDesc*/Enum*
// methods can be `override`, and an IDXGIObject-only base cannot sit between
// the mock and that interface without breaking the single-inheritance chain.
// --------------------------------------------------------------------------
template <typename Derived, typename Iface>
class ComObjectBase : public Iface {
public:
    HRESULT WINAPI QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER_;
        *ppv = nullptr;
        if (guid_eq(riid, IID_IUnknown_) || guid_eq(riid, IID_IDXGIObject_)) {
            *ppv = static_cast<Iface*>(this);
        } else if (static_cast<Derived*>(this)->query_extra(riid, ppv)) {
            // Derived filled *ppv.
        } else {
            return E_NOINTERFACE_;
        }
        AddRef();
        return S_OK_;
    }

    ULONG WINAPI AddRef() override {
        return ++refcount_;
    }

    ULONG WINAPI Release() override {
        const ULONG n = --refcount_;
        if (n == 0) delete static_cast<Derived*>(this);
        return n;
    }

    HRESULT WINAPI SetPrivateData(REFGUID, UINT, const void*) override { return S_OK_; }
    HRESULT WINAPI SetPrivateDataInterface(REFGUID, const IUnknown*) override { return S_OK_; }
    HRESULT WINAPI GetPrivateData(REFGUID, UINT* pDataSize, void*) override {
        if (pDataSize) *pDataSize = 0;
        return DXGI_ERROR_NOT_FOUND_;
    }
    HRESULT WINAPI GetParent(REFIID, void**) override { return DXGI_ERROR_INVALID_CALL_; }

protected:
    ComObjectBase() = default;
    ~ComObjectBase() = default;

private:
    std::atomic<ULONG> refcount_{1};
};

// --------------------------------------------------------------------------
// Defined after MockOutput (which it needs complete); MockAdapter below only
// sees this declaration.
void* make_mock_output();

// --------------------------------------------------------------------------
// Mock adapter: one generic Vulkan-capable GPU.
// --------------------------------------------------------------------------
class MockAdapter : public ComObjectBase<MockAdapter, IDXGIAdapter2> {
public:
    bool query_extra(REFIID riid, void** ppv) {
        if (guid_eq(riid, IID_IDXGIAdapter_)) {
            *ppv = static_cast<IDXGIAdapter*>(this);
            return true;
        }
        if (guid_eq(riid, IID_IDXGIAdapter1_)) {
            *ppv = static_cast<IDXGIAdapter1*>(this);
            return true;
        }
        if (guid_eq(riid, IID_IDXGIAdapter2_)) {
            *ppv = static_cast<IDXGIAdapter2*>(this);
            return true;
        }
        return false;
    }

    HRESULT WINAPI EnumOutputs(UINT index, void** ppOutput) override {
        if (!ppOutput) return E_POINTER_;
        *ppOutput = nullptr;
        if (index != 0) return DXGI_ERROR_NOT_FOUND_;
        *ppOutput = make_mock_output();
        DXGI_INFO("EnumOutputs(0) -> mock output");
        return S_OK_;
    }

    HRESULT WINAPI GetDesc(DXGI_ADAPTER_DESC* pDesc) override {
        if (!pDesc) return E_POINTER_;
        fill_desc_common(reinterpret_cast<DXGI_ADAPTER_DESC1*>(pDesc), false);
        return S_OK_;
    }

    HRESULT WINAPI CheckInterfaceSupport(REFGUID, int64_t* pUmdVersion) override {
        if (pUmdVersion) *pUmdVersion = 0;
        return S_OK_;
    }

    HRESULT WINAPI GetDesc1(DXGI_ADAPTER_DESC1* pDesc) override {
        if (!pDesc) return E_POINTER_;
        fill_desc_common(pDesc, true);
        return S_OK_;
    }

    HRESULT WINAPI GetDesc2(DXGI_ADAPTER_DESC2* pDesc) override {
        if (!pDesc) return E_POINTER_;
        DXGI_ADAPTER_DESC1 d1{};
        fill_desc_common(&d1, true);
        std::memcpy(pDesc, &d1, sizeof(DXGI_ADAPTER_DESC1));
        pDesc->Flags = d1.Flags;
        pDesc->GraphicsPreemptionGranularity = 3; // DXGI_GRAPHICS_PREEMPTION_DMA_BUFFER
        pDesc->ComputePreemptionGranularity = 3;  // DXGI_COMPUTE_PREEMPTION_DMA_BUFFER
        return S_OK_;
    }

private:
    static void fill_wide(char16_t* dst, size_t cap, const char16_t* src) {
        size_t i = 0;
        for (; src[i] && i + 1 < cap; ++i) dst[i] = src[i];
        dst[i] = 0;
    }

    void fill_desc_common(DXGI_ADAPTER_DESC1* d, bool with_flags) {
        static const char16_t kName[] = u"NEXLINK Vulkan Adapter";
        std::memset(d, 0, sizeof(*d));
        fill_wide(d->Description, 128, kName);
        d->VendorId = 0x1002;                 // AMD (arbitrary for the mock)
        d->DeviceId = 0x73BF;                 // RX 6800-class id (mock)
        d->SubSysId = 0;
        d->Revision = 0;
        d->DedicatedVideoMemory = 8ull << 30; // 8 GiB
        d->DedicatedSystemMemory = 0;
        d->SharedSystemMemory = 16ull << 30;  // 16 GiB shared
        d->AdapterLuid = 0x4C'58'45'4Eull;    // "NEXL"
        d->Flags = with_flags ? 0 : 0;        // DXGI_ADAPTER_FLAG_NONE
    }
};

// --------------------------------------------------------------------------
// Mock output: one 1920x1080 virtual display.
// --------------------------------------------------------------------------
class MockOutput : public ComObjectBase<MockOutput, IDXGIOutput> {
public:
    bool query_extra(REFIID, void**) { return false; }

    HRESULT WINAPI GetDesc(DXGI_OUTPUT_DESC* pDesc) override {
        if (!pDesc) return E_POINTER_;
        std::memset(pDesc, 0, sizeof(*pDesc));
        static const char16_t kName[] = u"\\\\.\\DISPLAY1";
        fill_wide(pDesc->DeviceName, 32, kName);
        pDesc->DesktopCoordinates = {0, 0, 1920, 1080};
        pDesc->AttachedToDesktop = 1;
        pDesc->Rotation = 1; // DXGI_MODE_ROTATION_IDENTITY
        pDesc->Monitor = reinterpret_cast<void*>(static_cast<uintptr_t>(0xDEAD0001));
        return S_OK_;
    }

    HRESULT WINAPI GetDisplayModeList(UINT, UINT flags, UINT* pNumModes,
                                      DXGI_MODE_DESC* pModeDescs) override {
        (void)flags;
        if (!pNumModes) return E_POINTER_;
        constexpr UINT kModeCount = 1;
        if (!pModeDescs || *pNumModes < kModeCount) {
            *pNumModes = kModeCount;
            return DXGI_ERROR_MORE_DATA_;
        }
        DXGI_MODE_DESC& m = pModeDescs[0];
        std::memset(&m, 0, sizeof(m));
        m.Width = 1920;
        m.Height = 1080;
        m.RefreshRate = {60, 1};
        m.Format = 28; // DXGI_FORMAT_R8G8B8A8_UNORM
        pNumModes[0] = kModeCount;
        return S_OK_;
    }

    HRESULT WINAPI FindClosestMatchingMode(const DXGI_MODE_DESC* pModeToMatch,
                                           DXGI_MODE_DESC* pClosestMatch,
                                           void*) override {
        if (!pClosestMatch) return E_POINTER_;
        std::memset(pClosestMatch, 0, sizeof(*pClosestMatch));
        if (pModeToMatch && pModeToMatch->Width && pModeToMatch->Height) {
            *pClosestMatch = *pModeToMatch;
        } else {
            pClosestMatch->Width = 1920;
            pClosestMatch->Height = 1080;
            pClosestMatch->RefreshRate = {60, 1};
            pClosestMatch->Format = 28;
        }
        return S_OK_;
    }

    HRESULT WINAPI WaitForVBlank() override { return S_OK_; }
    HRESULT WINAPI TakeOwnership(void*, BOOL) override { return S_OK_; }
    void WINAPI ReleaseOwnership() override {}

    HRESULT WINAPI GetGammaControlCapabilities(void* pCaps) override {
        if (pCaps) std::memset(pCaps, 0, 4 + 1024 * sizeof(float) + 3 * sizeof(float));
        return S_OK_;
    }
    HRESULT WINAPI SetGammaControl(const void*) override { return S_OK_; }
    HRESULT WINAPI GetGammaControl(void* pSet) override {
        if (pSet) std::memset(pSet, 0, 3 * (1024 * sizeof(float) + 3 * sizeof(float)));
        return S_OK_;
    }
    HRESULT WINAPI SetDisplaySurface(void*) override {
        DXGI_UNIMPL("IDXGIOutput::SetDisplaySurface");
        return E_NOTIMPL_;
    }
    HRESULT WINAPI GetDisplaySurfaceData(void*) override {
        DXGI_UNIMPL("IDXGIOutput::GetDisplaySurfaceData");
        return E_NOTIMPL_;
    }
    HRESULT WINAPI GetFrameStatistics(DXGI_FRAME_STATISTICS* pStats) override {
        if (pStats) std::memset(pStats, 0, sizeof(*pStats));
        return S_OK_;
    }

private:
    static void fill_wide(char16_t* dst, size_t cap, const char16_t* src) {
        size_t i = 0;
        for (; src[i] && i + 1 < cap; ++i) dst[i] = src[i];
        dst[i] = 0;
    }
};

// Factory thunk: lets MockAdapter (declared earlier) obtain a MockOutput
// while that type is still incomplete at the call site.
void* make_mock_output() {
    return static_cast<void*>(static_cast<IDXGIObject*>(new MockOutput()));
}

// --------------------------------------------------------------------------
// Mock factory.
// --------------------------------------------------------------------------
class MockFactory : public ComObjectBase<MockFactory, IDXGIFactory2> {
public:
    bool query_extra(REFIID riid, void** ppv) {
        if (guid_eq(riid, IID_IDXGIFactory_)) {
            *ppv = static_cast<IDXGIFactory*>(this);
            return true;
        }
        if (guid_eq(riid, IID_IDXGIFactory1_)) {
            *ppv = static_cast<IDXGIFactory1*>(this);
            return true;
        }
        if (guid_eq(riid, IID_IDXGIFactory2_)) {
            *ppv = static_cast<IDXGIFactory2*>(this);
            return true;
        }
        return false;
    }

    // ---- IDXGIFactory ------------------------------------------------------
    HRESULT WINAPI EnumAdapters(UINT index, IDXGIAdapter** ppAdapter) override {
        return EnumAdapters1(index, ppAdapter);
    }

    HRESULT WINAPI MakeWindowAssociation(HWND, UINT Flags) override {
        (void)Flags;
        return S_OK_;
    }

    HRESULT WINAPI GetWindowAssociation(HWND* pHWND) override {
        if (!pHWND) return E_POINTER_;
        *pHWND = reinterpret_cast<HWND>(static_cast<uintptr_t>(0xDEADBEEF));
        return S_OK_;
    }

    HRESULT WINAPI CreateSwapChain(void*, void*, void** ppSwapChain) override {
        DXGI_UNIMPL("IDXGIFactory::CreateSwapChain");
        if (ppSwapChain) *ppSwapChain = nullptr;
        return E_NOTIMPL_;
    }

    HRESULT WINAPI CreateSoftwareAdapter(HMODULE, IDXGIAdapter** ppAdapter) override {
        if (!ppAdapter) return E_POINTER_;
        *ppAdapter = nullptr;
        return DXGI_ERROR_NOT_FOUND_;
    }

    // ---- IDXGIFactory1 -----------------------------------------------------
    HRESULT WINAPI EnumAdapters1(UINT index, IDXGIAdapter** ppAdapter) override {
        if (!ppAdapter) return E_POINTER_;
        *ppAdapter = nullptr;
        if (index != 0) return DXGI_ERROR_NOT_FOUND_;
        auto* adapter = new MockAdapter();
        *ppAdapter = static_cast<IDXGIAdapter*>(static_cast<IDXGIObject*>(adapter));
        DXGI_INFO("EnumAdapters1(0) -> mock adapter");
        return S_OK_;
    }

    BOOL WINAPI IsCurrent() override { return 1; }

    // ---- IDXGIFactory2 -----------------------------------------------------
    BOOL WINAPI IsWindowedStereoEnabled() override { return 0; }

    HRESULT WINAPI CreateSwapChainForHwnd(void*, HWND, const void*, const void*, void*,
                                          void** ppSwapChain) override {
        DXGI_UNIMPL("IDXGIFactory2::CreateSwapChainForHwnd");
        if (ppSwapChain) *ppSwapChain = nullptr;
        return E_NOTIMPL_;
    }

    HRESULT WINAPI CreateSwapChainForCoreWindow(void*, void*, const void*, void*,
                                                void** ppSwapChain) override {
        DXGI_UNIMPL("IDXGIFactory2::CreateSwapChainForCoreWindow");
        if (ppSwapChain) *ppSwapChain = nullptr;
        return E_NOTIMPL_;
    }

    HRESULT WINAPI GetSharedResourceAdapterLuid(LUID* pLuid) override {
        if (pLuid) *pLuid = 0x4C'58'45'4Eull;
        return S_OK_;
    }

    HRESULT WINAPI RegisterStereoStatusWindow(HWND, UINT, DWORD* pCookie) override {
        if (pCookie) *pCookie = 1;
        return S_OK_;
    }

    HRESULT WINAPI RegisterStereoStatusEvent(void*, DWORD* pCookie) override {
        if (pCookie) *pCookie = 2;
        return S_OK_;
    }

    void WINAPI UnregisterStereoStatus(DWORD) override {}

    HRESULT WINAPI RegisterOcclusionStatusWindow(HWND, UINT, DWORD* pCookie) override {
        if (pCookie) *pCookie = 3;
        return S_OK_;
    }

    HRESULT WINAPI RegisterOcclusionStatusEvent(void*, DWORD* pCookie) override {
        if (pCookie) *pCookie = 4;
        return S_OK_;
    }

    void WINAPI UnregisterOcclusionStatus(DWORD) override {}

    HRESULT WINAPI CreateSwapChainForComposition(void*, const void*, void*,
                                                 void** ppSwapChain) override {
        DXGI_UNIMPL("IDXGIFactory2::CreateSwapChainForComposition");
        if (ppSwapChain) *ppSwapChain = nullptr;
        return E_NOTIMPL_;
    }
};

// Factory creation shared by the three exported entry points.
HRESULT create_factory(REFIID riid, void** ppFactory) {
    if (!ppFactory) return E_POINTER_;
    *ppFactory = nullptr;
    auto* factory = new MockFactory();
    const HRESULT hr = factory->QueryInterface(riid, ppFactory);
    factory->Release(); // drop the creation reference; QI took its own
    return hr;
}

} // namespace

// ==========================================================================
// Exported entry points
// ==========================================================================
WIN2LIN_EXPORT HRESULT WINAPI CreateDXGIFactory(REFIID riid, void** ppFactory) {
    DXGI_INFO("CreateDXGIFactory");
    return create_factory(riid, ppFactory);
}

WIN2LIN_EXPORT HRESULT WINAPI CreateDXGIFactory1(REFIID riid, void** ppFactory) {
    DXGI_INFO("CreateDXGIFactory1");
    return create_factory(riid, ppFactory);
}

WIN2LIN_EXPORT HRESULT WINAPI CreateDXGIFactory2(UINT flags, REFIID riid,
                                                 void** ppFactory) {
    DXGI_INFO("CreateDXGIFactory2(flags=0x%x)", flags);
    return create_factory(riid, ppFactory);
}

// DXGIGetDebugInterface1 lives in dxgi.dll's debug layer; stub it so debug
// builds do not crash on load.
WIN2LIN_EXPORT HRESULT WINAPI DXGIGetDebugInterface1(UINT, REFIID riid, void** ppv) {
    (void)riid;
    if (ppv) *ppv = nullptr;
    DXGI_UNIMPL("DXGIGetDebugInterface1");
    return E_NOTIMPL_;
}
