// libwin_d3d11.so - D3D11 shim backed by Vulkan (Phase 2).
//
// Strategy:
//   * COM interfaces modelled as C++ abstract classes with __attribute__((ms_abi))
//     so every vtable slot follows the Microsoft x64 ABI.  Single inheritance
//     preserves slot order.  Vtable layouts verified against mingw-w64 d3d11.h.
//   * D3D11CreateDevice initialises a Vulkan instance/device and returns a
//     MockD3D11Device + MockD3D11DeviceContext.
//   * CreateBuffer maps D3D11_BUFFER_DESC to VkBuffer + VkDeviceMemory with
//     correct memory-type selection (HOST_VISIBLE for DYNAMIC / CPU_ACCESS_WRITE).
//   * Draw / DrawIndexed log the call and prepare to record into a VkCommandBuffer.
//     Full shader translation is Phase 3; here we just ensure the API surface
//     doesn't crash and the Vulkan backend sees the resource mapping.

#include "common/win32.hpp"
#include "common/com_base.h"
#include "vulkan_backend.h"
#include "stub_shaders.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

using namespace win2lin::win;
using namespace win2lin::com;
namespace d3d11vk = win2lin::d3d11vk;

namespace {

// --------------------------------------------------------------------------
// D3D11 / DXGI ABI types (subset needed for Phase 2).
// --------------------------------------------------------------------------
using DXGI_FORMAT = UINT;
using D3D_FEATURE_LEVEL = UINT;
using D3D_DRIVER_TYPE = UINT;
using D3D11_USAGE = UINT;
using D3D11_MAP = UINT;
using D3D11_INPUT_CLASSIFICATION = UINT;
using INT = int32_t;
using UINT8 = uint8_t;
constexpr BOOL FALSE = 0;

// D3D11 state enum types (used by DESC structs below).
using D3D11_FILL_MODE = UINT;
using D3D11_CULL_MODE = UINT;
using D3D11_BLEND = UINT;
using D3D11_BLEND_OP = UINT;
using D3D11_DEPTH_WRITE_MASK = UINT;
using D3D11_COMPARISON_FUNC = UINT;
using D3D11_STENCIL_OP = UINT;

struct DXGI_RATIONAL { UINT Numerator; UINT Denominator; };
struct DXGI_SAMPLE_DESC { UINT Count; UINT Quality; };
struct DXGI_MODE_DESC {
    UINT Width;
    UINT Height;
    DXGI_RATIONAL RefreshRate;
    DXGI_FORMAT Format;
    UINT ScanlineOrdering;
    UINT Scaling;
};
struct DXGI_SWAP_CHAIN_DESC {
    DXGI_MODE_DESC BufferDesc;
    DXGI_SAMPLE_DESC SampleDesc;
    UINT BufferUsage;
    UINT BufferCount;
    HWND OutputWindow;
    BOOL Windowed;
    UINT SwapEffect;
    UINT Flags;
};

struct D3D11_BUFFER_DESC {
    UINT ByteWidth;
    D3D11_USAGE Usage;
    UINT BindFlags;
    UINT CPUAccessFlags;
    UINT MiscFlags;
    UINT StructureByteStride;
};
struct D3D11_TEXTURE2D_DESC {
    UINT Width;
    UINT Height;
    UINT MipLevels;
    UINT ArraySize;
    DXGI_FORMAT Format;
    DXGI_SAMPLE_DESC SampleDesc;
    D3D11_USAGE Usage;
    UINT BindFlags;
    UINT CPUAccessFlags;
    UINT MiscFlags;
};
struct D3D11_VIEWPORT { float TopLeftX, TopLeftY, Width, Height, MinDepth, MaxDepth; };
struct D3D11_BOX { UINT left, top, front, right, bottom, back; };
struct D3D11_MAPPED_SUBRESOURCE { void* pData; UINT RowPitch; UINT DepthPitch; };
struct D3D11_INPUT_ELEMENT_DESC {
    LPCSTR SemanticName;
    UINT SemanticIndex;
    DXGI_FORMAT Format;
    UINT InputSlot;
    UINT AlignedByteOffset;
    UINT InputSlotClass;
    UINT InstanceDataStepRate;
};
struct D3D11_RASTERIZER_DESC {
    D3D11_FILL_MODE FillMode;
    D3D11_CULL_MODE CullMode;
    BOOL FrontCounterClockwise;
    INT DepthBias;
    float DepthBiasClamp;
    float SlopeScaledDepthBias;
    BOOL DepthClipEnable;
    BOOL ScissorEnable;
    BOOL MultisampleEnable;
    BOOL AntialiasedLineEnable;
};
struct D3D11_BLEND_DESC {
    BOOL AlphaToCoverageEnable;
    BOOL IndependentBlendEnable;
    struct {
        BOOL BlendEnable;
        D3D11_BLEND SrcBlend;
        D3D11_BLEND DestBlend;
        D3D11_BLEND_OP BlendOp;
        D3D11_BLEND SrcBlendAlpha;
        D3D11_BLEND DestBlendAlpha;
        D3D11_BLEND_OP BlendOpAlpha;
        UINT8 RenderTargetWriteMask;
    } RenderTarget[8];
};
struct D3D11_DEPTH_STENCIL_DESC {
    BOOL DepthEnable;
    D3D11_DEPTH_WRITE_MASK DepthWriteMask;
    D3D11_COMPARISON_FUNC DepthFunc;
    BOOL StencilEnable;
    UINT8 StencilReadMask;
    UINT8 StencilWriteMask;
    struct {
        D3D11_STENCIL_OP StencilFailOp;
        D3D11_STENCIL_OP StencilDepthFailOp;
        D3D11_STENCIL_OP StencilPassOp;
        D3D11_COMPARISON_FUNC StencilFunc;
    } FrontFace, BackFace;
};

// Enum values (subset).
constexpr D3D_FEATURE_LEVEL D3D_FEATURE_LEVEL_11_0_ = 0xb000;
constexpr D3D_DRIVER_TYPE D3D_DRIVER_TYPE_HARDWARE_ = 1;
constexpr D3D_DRIVER_TYPE D3D_DRIVER_TYPE_WARP_ = 5;
constexpr D3D11_USAGE D3D11_USAGE_DEFAULT_ = 0;
constexpr D3D11_USAGE D3D11_USAGE_IMMUTABLE_ = 1;
constexpr D3D11_USAGE D3D11_USAGE_DYNAMIC_ = 2;
constexpr D3D11_USAGE D3D11_USAGE_STAGING_ = 3;
constexpr UINT D3D11_BIND_VERTEX_BUFFER_ = 0x1;
constexpr UINT D3D11_BIND_INDEX_BUFFER_ = 0x2;
constexpr UINT D3D11_BIND_CONSTANT_BUFFER_ = 0x4;
constexpr UINT D3D11_BIND_SHADER_RESOURCE_ = 0x8;
constexpr UINT D3D11_BIND_RENDER_TARGET_ = 0x20;
constexpr UINT D3D11_BIND_DEPTH_STENCIL_ = 0x40;
constexpr UINT D3D11_CPU_ACCESS_WRITE_ = 0x10000;
constexpr UINT D3D11_CPU_ACCESS_READ_ = 0x20000;
constexpr D3D11_MAP D3D11_MAP_WRITE_DISCARD_ = 4;
constexpr UINT D3D11_SDK_VERSION_ = 7;
constexpr UINT DXGI_FORMAT_R8G8B8A8_UNORM_ = 28;
constexpr UINT DXGI_FORMAT_R32G32B32_FLOAT_ = 6;
constexpr UINT DXGI_FORMAT_R32G32_FLOAT_ = 16;
constexpr UINT DXGI_FORMAT_R32G32B32A32_FLOAT_ = 2;
constexpr UINT D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ = 4;
constexpr UINT D3D11_APPEND_ALIGNED_ELEMENT_ = 0xffffffff;

// D3D11 state enum values.
constexpr D3D11_FILL_MODE D3D11_FILL_SOLID_ = 3;
constexpr D3D11_CULL_MODE D3D11_CULL_NONE_ = 1;
constexpr D3D11_CULL_MODE D3D11_CULL_FRONT_ = 2;
constexpr D3D11_CULL_MODE D3D11_CULL_BACK_ = 3;
constexpr D3D11_BLEND D3D11_BLEND_ONE_ = 2;
constexpr D3D11_BLEND D3D11_BLEND_ZERO_ = 1;
constexpr D3D11_BLEND_OP D3D11_BLEND_OP_ADD_ = 1;
constexpr D3D11_DEPTH_WRITE_MASK D3D11_DEPTH_WRITE_MASK_ALL_ = 1;
constexpr D3D11_COMPARISON_FUNC D3D11_COMPARISON_ALWAYS_ = 8;
constexpr UINT D3D11_COLOR_WRITE_ENABLE_ALL_ = 0xf;

// DXGI IIDs already defined in dxgi.cpp; redeclare locally for d3d11.
constexpr GUID IID_IDXGIAdapter_ =
    {0x2411E7E1, 0x12AC, 0x4CCF, {0xBD, 0x14, 0x97, 0x98, 0xE8, 0x53, 0x4D, 0xC0}};

// D3D11 IIDs (verified against mingw-w64 d3d11.h).
constexpr GUID IID_ID3D11DeviceChild_ =
    {0x1841E5C8, 0x16B0, 0x489B, {0xBC, 0xC8, 0x44, 0xCF, 0xB0, 0xD5, 0xDE, 0xAE}};
constexpr GUID IID_ID3D11Resource_ =
    {0xDC8E63F3, 0xD12B, 0x4952, {0xB4, 0x7B, 0x5E, 0x45, 0x02, 0x6A, 0x86, 0x2D}};
constexpr GUID IID_ID3D11Buffer_ =
    {0x48570B85, 0xD1EE, 0x4FCD, {0xA2, 0x50, 0xEB, 0x35, 0x07, 0x22, 0xB0, 0x37}};
constexpr GUID IID_ID3D11Texture2D_ =
    {0x6F15AAF2, 0xD208, 0x4E89, {0x9A, 0xB4, 0x48, 0x95, 0x35, 0xD3, 0x4F, 0x9C}};
constexpr GUID IID_ID3D11RenderTargetView_ =
    {0xDFDBA067, 0x0B8D, 0x4865, {0x87, 0x5B, 0xD7, 0xB4, 0x51, 0x6C, 0xC1, 0x64}};
constexpr GUID IID_ID3D11Device_ =
    {0xDB6F6DDB, 0xAC77, 0x4E88, {0x82, 0x53, 0x81, 0x9D, 0xF9, 0xBB, 0xF1, 0x40}};
constexpr GUID IID_ID3D11DeviceContext_ =
    {0xC0BFA96C, 0xE089, 0x44FB, {0x8E, 0xAF, 0x26, 0xF8, 0x79, 0x61, 0x90, 0xDA}};
constexpr GUID IID_ID3D11VertexShader_ =
    {0x3B301D64, 0xD678, 0x4289, {0x88, 0x97, 0x22, 0xF8, 0x92, 0x8B, 0x72, 0xF3}};
constexpr GUID IID_ID3D11PixelShader_ =
    {0xEA82E40D, 0x51DC, 0x4F33, {0x93, 0xD4, 0xDB, 0x7C, 0x91, 0x25, 0xAE, 0x8C}};
constexpr GUID IID_ID3D11InputLayout_ =
    {0xE4819DDC, 0x4CF0, 0x4025, {0xBD, 0x26, 0x5D, 0xE8, 0x2A, 0x3E, 0x07, 0xB7}};
constexpr GUID IID_ID3D11RasterizerState_ =
    {0x9BB4AB81, 0xAB1A, 0x4D8F, {0xB5, 0x06, 0xFC, 0x04, 0x20, 0x0B, 0x6E, 0xE7}};
constexpr GUID IID_ID3D11BlendState_ =
    {0x75B68FAA, 0x347D, 0x4159, {0x8F, 0x45, 0xA0, 0x64, 0x0F, 0x01, 0xCD, 0x9A}};
constexpr GUID IID_ID3D11DepthStencilState_ =
    {0x03823EFB, 0x8D8F, 0x4E1C, {0x9A, 0xA2, 0xF6, 0x4B, 0xB2, 0xCB, 0xFD, 0xF1}};
constexpr GUID IID_ID3D11SamplerState_ =
    {0xDA6FEA51, 0x564C, 0x4487, {0x98, 0x10, 0xF0, 0xD0, 0xF9, 0xB4, 0xE3, 0xA5}};
constexpr GUID IID_IDXGISwapChain_ =
    {0x310D36A0, 0xD2E7, 0x4C0A, {0xAA, 0x04, 0x6A, 0x9D, 0x23, 0xB8, 0x88, 0x6A}};
constexpr GUID IID_IDXGIDevice_ =
    {0x54EC77FA, 0x1377, 0x44E6, {0x8C, 0x32, 0x88, 0xFD, 0x5F, 0x44, 0xC8, 0x4C}};

#define D3D11_UNIMPL(name) fprintf(stderr, "[NEXLINK D3D11 UNIMPL] %s\n", name)
#define D3D11_INFO(fmt, ...) fprintf(stderr, "[NEXLINK D3D11] " fmt "\n", ##__VA_ARGS__)

#define VK_CHECK_D3D11(expr)                                                    \
    do {                                                                        \
        VkResult _vr = (expr);                                                  \
        if (_vr != VK_SUCCESS) {                                                \
            D3D11_INFO("VK_CHECK failed: %s -> %d", #expr, static_cast<int>(_vr)); \
            return false;                                                       \
        }                                                                       \
    } while (0)

constexpr HRESULT E_FAIL_ = static_cast<HRESULT>(0x80004005LL);
constexpr HRESULT E_OUTOFMEMORY_ = static_cast<HRESULT>(0x8007000ELL);
inline bool SUCCEEDED(HRESULT hr) { return hr >= 0; }
inline bool FAILED(HRESULT hr) { return hr < 0; }

// --------------------------------------------------------------------------
// COM interface mirrors.  Default stubs return safe values so the mock only
// overrides what it needs.  Vtable order matches mingw-w64 d3d11.h exactly.
// --------------------------------------------------------------------------

// ---- ID3D11DeviceChild (7 methods) ---------------------------------------
struct __attribute__((ms_abi)) ID3D11Device;
struct __attribute__((ms_abi)) ID3D11DeviceChild : public IUnknown {
    virtual HRESULT WINAPI GetDevice(ID3D11Device** ppDevice) {
        if (ppDevice) *ppDevice = nullptr;
        return E_POINTER_;
    }
    virtual HRESULT WINAPI GetPrivateData(REFGUID, UINT* pDataSize, void*) {
        if (pDataSize) *pDataSize = 0;
        return DXGI_ERROR_NOT_FOUND_;
    }
    virtual HRESULT WINAPI SetPrivateData(REFGUID, UINT, const void*) { return S_OK_; }
    virtual HRESULT WINAPI SetPrivateDataInterface(REFGUID, const IUnknown*) { return S_OK_; }
};

// ---- ID3D11Resource (10 methods) -----------------------------------------
struct __attribute__((ms_abi)) ID3D11Resource : public ID3D11DeviceChild {
    virtual void WINAPI GetType(UINT* pResourceDimension) {
        if (pResourceDimension) *pResourceDimension = 0;
    }
    virtual void WINAPI SetEvictionPriority(UINT) {}
    virtual UINT WINAPI GetEvictionPriority() { return 0; }
};

// ---- ID3D11Buffer (11 methods) -------------------------------------------
struct __attribute__((ms_abi)) ID3D11Buffer : public ID3D11Resource {
    virtual void WINAPI GetDesc(D3D11_BUFFER_DESC* pDesc) {
        if (pDesc) std::memset(pDesc, 0, sizeof(*pDesc));
    }
};

// ---- ID3D11Texture2D (11 methods) ----------------------------------------
struct __attribute__((ms_abi)) ID3D11Texture2D : public ID3D11Resource {
    virtual void WINAPI GetDesc(D3D11_TEXTURE2D_DESC* pDesc) {
        if (pDesc) std::memset(pDesc, 0, sizeof(*pDesc));
    }
};

// ---- ID3D11RenderTargetView (9 methods) ----------------------------------
struct __attribute__((ms_abi)) ID3D11RenderTargetView : public ID3D11DeviceChild {
    virtual void WINAPI GetResource(ID3D11Resource** ppResource) {
        if (ppResource) *ppResource = nullptr;
    }
    virtual void WINAPI GetDesc(void* pDesc) {
        if (pDesc) std::memset(pDesc, 0, 44); // sizeof(D3D11_RENDER_TARGET_VIEW_DESC)
    }
};

// ---- Shader / state objects (7-8 methods each) ---------------------------
struct __attribute__((ms_abi)) ID3D11VertexShader : public ID3D11DeviceChild {};
struct __attribute__((ms_abi)) ID3D11PixelShader : public ID3D11DeviceChild {};
struct __attribute__((ms_abi)) ID3D11InputLayout : public ID3D11DeviceChild {};

struct __attribute__((ms_abi)) ID3D11RasterizerState : public ID3D11DeviceChild {
    virtual void WINAPI GetDesc(void* pDesc) {
        if (pDesc) std::memset(pDesc, 0, 44); // sizeof(D3D11_RASTERIZER_DESC)
    }
};
struct __attribute__((ms_abi)) ID3D11BlendState : public ID3D11DeviceChild {
    virtual void WINAPI GetDesc(void* pDesc) {
        if (pDesc) std::memset(pDesc, 0, 72); // sizeof(D3D11_BLEND_DESC)
    }
};
struct __attribute__((ms_abi)) ID3D11DepthStencilState : public ID3D11DeviceChild {
    virtual void WINAPI GetDesc(void* pDesc) {
        if (pDesc) std::memset(pDesc, 0, 76); // sizeof(D3D11_DEPTH_STENCIL_DESC)
    }
};
struct __attribute__((ms_abi)) ID3D11SamplerState : public ID3D11DeviceChild {
    virtual void WINAPI GetDesc(void* pDesc) {
        if (pDesc) std::memset(pDesc, 0, 52); // sizeof(D3D11_SAMPLER_DESC)
    }
};

// ---- ID3D11Device (43 methods) -------------------------------------------
struct __attribute__((ms_abi)) ID3D11DeviceContext;
struct __attribute__((ms_abi)) ID3D11Device : public IUnknown {
    virtual HRESULT WINAPI CreateBuffer(const D3D11_BUFFER_DESC*, const void*,
                                        ID3D11Buffer** ppBuffer) {
        if (ppBuffer) *ppBuffer = nullptr;
        D3D11_UNIMPL("ID3D11Device::CreateBuffer");
        return E_NOTIMPL_;
    }
    virtual HRESULT WINAPI CreateTexture1D(const void*, const void*, void** pp) {
        if (pp) *pp = nullptr;
        D3D11_UNIMPL("ID3D11Device::CreateTexture1D");
        return E_NOTIMPL_;
    }
    virtual HRESULT WINAPI CreateTexture2D(const D3D11_TEXTURE2D_DESC*, const void*,
                                           ID3D11Texture2D** ppTexture2D) {
        if (ppTexture2D) *ppTexture2D = nullptr;
        D3D11_UNIMPL("ID3D11Device::CreateTexture2D");
        return E_NOTIMPL_;
    }
    virtual HRESULT WINAPI CreateTexture3D(const void*, const void*, void** pp) {
        if (pp) *pp = nullptr;
        D3D11_UNIMPL("ID3D11Device::CreateTexture3D");
        return E_NOTIMPL_;
    }
    virtual HRESULT WINAPI CreateShaderResourceView(void*, const void*, void** pp) {
        if (pp) *pp = nullptr;
        D3D11_UNIMPL("ID3D11Device::CreateShaderResourceView");
        return E_NOTIMPL_;
    }
    virtual HRESULT WINAPI CreateUnorderedAccessView(void*, const void*, void** pp) {
        if (pp) *pp = nullptr;
        D3D11_UNIMPL("ID3D11Device::CreateUnorderedAccessView");
        return E_NOTIMPL_;
    }
    virtual HRESULT WINAPI CreateRenderTargetView(void*, const void*,
                                                  ID3D11RenderTargetView** pp) {
        if (pp) *pp = nullptr;
        D3D11_UNIMPL("ID3D11Device::CreateRenderTargetView");
        return E_NOTIMPL_;
    }
    virtual HRESULT WINAPI CreateDepthStencilView(void*, const void*, void** pp) {
        if (pp) *pp = nullptr;
        D3D11_UNIMPL("ID3D11Device::CreateDepthStencilView");
        return E_NOTIMPL_;
    }
    virtual HRESULT WINAPI CreateInputLayout(const D3D11_INPUT_ELEMENT_DESC*, UINT,
                                             const void*, SIZE_T,
                                             ID3D11InputLayout** pp) {
        if (pp) *pp = nullptr;
        D3D11_UNIMPL("ID3D11Device::CreateInputLayout");
        return E_NOTIMPL_;
    }
    virtual HRESULT WINAPI CreateVertexShader(const void*, SIZE_T, void*,
                                              ID3D11VertexShader** pp) {
        if (pp) *pp = nullptr;
        D3D11_UNIMPL("ID3D11Device::CreateVertexShader");
        return E_NOTIMPL_;
    }
    virtual HRESULT WINAPI CreateGeometryShader(const void*, SIZE_T, void*, void** pp) {
        if (pp) *pp = nullptr;
        D3D11_UNIMPL("ID3D11Device::CreateGeometryShader");
        return E_NOTIMPL_;
    }
    virtual HRESULT WINAPI CreateGeometryShaderWithStreamOutput(const void*, SIZE_T,
                                                                const void*, UINT, const void*,
                                                                UINT, UINT, void*, void** pp) {
        if (pp) *pp = nullptr;
        D3D11_UNIMPL("ID3D11Device::CreateGeometryShaderWithStreamOutput");
        return E_NOTIMPL_;
    }
    virtual HRESULT WINAPI CreatePixelShader(const void*, SIZE_T, void*,
                                             ID3D11PixelShader** pp) {
        if (pp) *pp = nullptr;
        D3D11_UNIMPL("ID3D11Device::CreatePixelShader");
        return E_NOTIMPL_;
    }
    virtual HRESULT WINAPI CreateHullShader(const void*, SIZE_T, void*, void** pp) {
        if (pp) *pp = nullptr;
        D3D11_UNIMPL("ID3D11Device::CreateHullShader");
        return E_NOTIMPL_;
    }
    virtual HRESULT WINAPI CreateDomainShader(const void*, SIZE_T, void*, void** pp) {
        if (pp) *pp = nullptr;
        D3D11_UNIMPL("ID3D11Device::CreateDomainShader");
        return E_NOTIMPL_;
    }
    virtual HRESULT WINAPI CreateComputeShader(const void*, SIZE_T, void*, void** pp) {
        if (pp) *pp = nullptr;
        D3D11_UNIMPL("ID3D11Device::CreateComputeShader");
        return E_NOTIMPL_;
    }
    virtual HRESULT WINAPI CreateClassLinkage(void** pp) {
        if (pp) *pp = nullptr;
        D3D11_UNIMPL("ID3D11Device::CreateClassLinkage");
        return E_NOTIMPL_;
    }
    virtual HRESULT WINAPI CreateBlendState(const void*, ID3D11BlendState** pp) {
        if (pp) *pp = nullptr;
        D3D11_UNIMPL("ID3D11Device::CreateBlendState");
        return E_NOTIMPL_;
    }
    virtual HRESULT WINAPI CreateDepthStencilState(const void*, ID3D11DepthStencilState** pp) {
        if (pp) *pp = nullptr;
        D3D11_UNIMPL("ID3D11Device::CreateDepthStencilState");
        return E_NOTIMPL_;
    }
    virtual HRESULT WINAPI CreateRasterizerState(const void*, ID3D11RasterizerState** pp) {
        if (pp) *pp = nullptr;
        D3D11_UNIMPL("ID3D11Device::CreateRasterizerState");
        return E_NOTIMPL_;
    }
    virtual HRESULT WINAPI CreateSamplerState(const void*, ID3D11SamplerState** pp) {
        if (pp) *pp = nullptr;
        D3D11_UNIMPL("ID3D11Device::CreateSamplerState");
        return E_NOTIMPL_;
    }
    virtual HRESULT WINAPI CreateQuery(const void*, void** pp) {
        if (pp) *pp = nullptr;
        D3D11_UNIMPL("ID3D11Device::CreateQuery");
        return E_NOTIMPL_;
    }
    virtual HRESULT WINAPI CreatePredicate(const void*, void** pp) {
        if (pp) *pp = nullptr;
        D3D11_UNIMPL("ID3D11Device::CreatePredicate");
        return E_NOTIMPL_;
    }
    virtual HRESULT WINAPI CreateCounter(const void*, void** pp) {
        if (pp) *pp = nullptr;
        D3D11_UNIMPL("ID3D11Device::CreateCounter");
        return E_NOTIMPL_;
    }
    virtual HRESULT WINAPI CreateDeferredContext(UINT, ID3D11DeviceContext** pp) {
        if (pp) *pp = nullptr;
        D3D11_UNIMPL("ID3D11Device::CreateDeferredContext");
        return E_NOTIMPL_;
    }
    virtual HRESULT WINAPI OpenSharedResource(HANDLE, REFIID, void** pp) {
        if (pp) *pp = nullptr;
        D3D11_UNIMPL("ID3D11Device::OpenSharedResource");
        return E_NOTIMPL_;
    }
    virtual HRESULT WINAPI CheckFormatSupport(DXGI_FORMAT, UINT* pSupport) {
        if (pSupport) *pSupport = 0;
        return S_OK_;
    }
    virtual HRESULT WINAPI CheckMultisampleQualityLevels(DXGI_FORMAT, UINT, UINT* pCount) {
        if (pCount) *pCount = 1;
        return S_OK_;
    }
    virtual void WINAPI CheckCounterInfo(void* pInfo) {
        if (pInfo) std::memset(pInfo, 0, 44); // sizeof(D3D11_COUNTER_INFO)
    }
    virtual HRESULT WINAPI CheckCounter(const void*, void*, void*, LPSTR, UINT*,
                                        LPSTR, UINT*, void*, UINT*, void*, UINT*) {
        D3D11_UNIMPL("ID3D11Device::CheckCounter");
        return E_NOTIMPL_;
    }
    virtual HRESULT WINAPI CheckFeatureSupport(UINT, void*, UINT) {
        D3D11_UNIMPL("ID3D11Device::CheckFeatureSupport");
        return E_NOTIMPL_;
    }
    virtual HRESULT WINAPI GetPrivateData(REFGUID, UINT* pDataSize, void*) {
        if (pDataSize) *pDataSize = 0;
        return DXGI_ERROR_NOT_FOUND_;
    }
    virtual HRESULT WINAPI SetPrivateData(REFGUID, UINT, const void*) { return S_OK_; }
    virtual HRESULT WINAPI SetPrivateDataInterface(REFGUID, const IUnknown*) { return S_OK_; }
    virtual D3D_FEATURE_LEVEL WINAPI GetFeatureLevel() { return D3D_FEATURE_LEVEL_11_0_; }
    virtual UINT WINAPI GetCreationFlags() { return 0; }
    virtual HRESULT WINAPI GetDeviceRemovedReason() { return S_OK_; }
    virtual void WINAPI GetImmediateContext(ID3D11DeviceContext** pp) {
        if (pp) *pp = nullptr;
    }
    virtual HRESULT WINAPI SetExceptionMode(UINT) { return S_OK_; }
    virtual UINT WINAPI GetExceptionMode() { return 0; }
};

// ---- ID3D11DeviceContext (115 methods) -----------------------------------
struct __attribute__((ms_abi)) ID3D11DeviceContext : public ID3D11DeviceChild {
    // -- pipeline setters (stubs; overridden by mock where needed) ----------
    virtual void WINAPI VSSetConstantBuffers(UINT, UINT, ID3D11Buffer* const*) {}
    virtual void WINAPI PSSetShaderResources(UINT, UINT, void* const*) {}
    virtual void WINAPI PSSetShader(ID3D11PixelShader*, void* const*, UINT) {}
    virtual void WINAPI PSSetSamplers(UINT, UINT, void* const*) {}
    virtual void WINAPI VSSetShader(ID3D11VertexShader*, void* const*, UINT) {}
    virtual void WINAPI DrawIndexed(UINT, UINT, INT) {
        D3D11_UNIMPL("ID3D11DeviceContext::DrawIndexed");
    }
    virtual void WINAPI Draw(UINT, UINT) {
        D3D11_UNIMPL("ID3D11DeviceContext::Draw");
    }
    virtual HRESULT WINAPI Map(ID3D11Buffer*, UINT, D3D11_MAP, UINT,
                               D3D11_MAPPED_SUBRESOURCE* pMapped) {
        if (pMapped) std::memset(pMapped, 0, sizeof(*pMapped));
        D3D11_UNIMPL("ID3D11DeviceContext::Map");
        return E_NOTIMPL_;
    }
    virtual void WINAPI Unmap(ID3D11Buffer*, UINT) {
        D3D11_UNIMPL("ID3D11DeviceContext::Unmap");
    }
    virtual void WINAPI PSSetConstantBuffers(UINT, UINT, ID3D11Buffer* const*) {}
    virtual void WINAPI IASetInputLayout(ID3D11InputLayout*) {}
    virtual void WINAPI IASetVertexBuffers(UINT, UINT, ID3D11Buffer* const*,
                                           const UINT* pStrides, const UINT* pOffsets) {}
    virtual void WINAPI IASetIndexBuffer(ID3D11Buffer*, DXGI_FORMAT, UINT) {}
    virtual void WINAPI DrawIndexedInstanced(UINT, UINT, UINT, INT, UINT) {
        D3D11_UNIMPL("ID3D11DeviceContext::DrawIndexedInstanced");
    }
    virtual void WINAPI DrawInstanced(UINT, UINT, UINT, UINT) {
        D3D11_UNIMPL("ID3D11DeviceContext::DrawInstanced");
    }
    virtual void WINAPI GSSetConstantBuffers(UINT, UINT, ID3D11Buffer* const*) {}
    virtual void WINAPI GSSetShader(void*, void* const*, UINT) {}
    virtual void WINAPI IASetPrimitiveTopology(UINT) {}
    virtual void WINAPI VSSetShaderResources(UINT, UINT, void* const*) {}
    virtual void WINAPI VSSetSamplers(UINT, UINT, void* const*) {}
    virtual void WINAPI Begin(void*) {}
    virtual void WINAPI End(void*) {}
    virtual HRESULT WINAPI GetData(void*, void*, UINT, UINT) { return S_OK_; }
    virtual void WINAPI SetPredication(void*, UINT) {}
    virtual void WINAPI GSSetShaderResources(UINT, UINT, void* const*) {}
    virtual void WINAPI GSSetSamplers(UINT, UINT, void* const*) {}
    virtual void WINAPI OMSetRenderTargets(UINT, ID3D11RenderTargetView* const*,
                                           ID3D11RenderTargetView*) {}
    virtual void WINAPI OMSetRenderTargetsAndUnorderedAccessViews(
        UINT, ID3D11RenderTargetView* const*, void*, UINT, UINT, void* const*, const UINT*) {}
    virtual void WINAPI OMSetBlendState(ID3D11BlendState*, const float*, UINT) {}
    virtual void WINAPI OMSetDepthStencilState(ID3D11DepthStencilState*, UINT) {}
    virtual void WINAPI SOSetTargets(UINT, ID3D11Buffer* const*, const UINT*) {}
    virtual void WINAPI DrawAuto() { D3D11_UNIMPL("ID3D11DeviceContext::DrawAuto"); }
    virtual void WINAPI DrawIndexedInstancedIndirect(ID3D11Buffer*, UINT) {
        D3D11_UNIMPL("ID3D11DeviceContext::DrawIndexedInstancedIndirect");
    }
    virtual void WINAPI DrawInstancedIndirect(ID3D11Buffer*, UINT) {
        D3D11_UNIMPL("ID3D11DeviceContext::DrawInstancedIndirect");
    }
    virtual void WINAPI Dispatch(UINT, UINT, UINT) {
        D3D11_UNIMPL("ID3D11DeviceContext::Dispatch");
    }
    virtual void WINAPI DispatchIndirect(ID3D11Buffer*, UINT) {
        D3D11_UNIMPL("ID3D11DeviceContext::DispatchIndirect");
    }
    virtual void WINAPI RSSetState(void*) {}
    virtual void WINAPI RSSetViewports(UINT, const D3D11_VIEWPORT*) {}
    virtual void WINAPI RSSetScissorRects(UINT, const D3D11_BOX*) {}
    virtual void WINAPI CopySubresourceRegion(ID3D11Resource*, UINT, UINT, UINT, UINT,
                                              ID3D11Resource*, UINT, const D3D11_BOX*) {}
    virtual void WINAPI CopyResource(ID3D11Resource*, ID3D11Resource*) {}
    virtual void WINAPI UpdateSubresource(ID3D11Resource*, UINT, const D3D11_BOX*,
                                          const void*, UINT, UINT) {}
    virtual void WINAPI CopyStructureCount(void*, UINT, void*) {}
    virtual void WINAPI ClearRenderTargetView(ID3D11RenderTargetView*, const float*) {}
    virtual void WINAPI ClearUnorderedAccessViewUint(void*, const UINT*) {}
    virtual void WINAPI ClearUnorderedAccessViewFloat(void*, const float*) {}
    virtual void WINAPI ClearDepthStencilView(void*, UINT, float, UINT8) {}
    virtual void WINAPI GenerateMips(void*) {}
    virtual void WINAPI SetResourceMinLOD(ID3D11Resource*, float) {}
    virtual float WINAPI GetResourceMinLOD(ID3D11Resource*) { return 0.0f; }
    virtual void WINAPI ResolveSubresource(ID3D11Resource*, UINT, ID3D11Resource*,
                                           UINT, DXGI_FORMAT) {}
    virtual void WINAPI ExecuteCommandList(void*, BOOL) {}
    virtual void WINAPI HSSetShaderResources(UINT, UINT, void* const*) {}
    virtual void WINAPI HSSetShader(void*, void* const*, UINT) {}
    virtual void WINAPI HSSetSamplers(UINT, UINT, void* const*) {}
    virtual void WINAPI HSSetConstantBuffers(UINT, UINT, ID3D11Buffer* const*) {}
    virtual void WINAPI DSSetShaderResources(UINT, UINT, void* const*) {}
    virtual void WINAPI DSSetShader(void*, void* const*, UINT) {}
    virtual void WINAPI DSSetSamplers(UINT, UINT, void* const*) {}
    virtual void WINAPI DSSetConstantBuffers(UINT, UINT, ID3D11Buffer* const*) {}
    virtual void WINAPI CSSetShaderResources(UINT, UINT, void* const*) {}
    virtual void WINAPI CSSetUnorderedAccessViews(UINT, UINT, void* const*, const UINT*) {}
    virtual void WINAPI CSSetShader(void*, void* const*, UINT) {}
    virtual void WINAPI CSSetSamplers(UINT, UINT, void* const*) {}
    virtual void WINAPI CSSetConstantBuffers(UINT, UINT, ID3D11Buffer* const*) {}
    // -- getters -------------------------------------------------------------
    virtual void WINAPI VSGetConstantBuffers(UINT, UINT, ID3D11Buffer**) {}
    virtual void WINAPI PSGetShaderResources(UINT, UINT, void**) {}
    virtual void WINAPI PSGetShader(ID3D11PixelShader**, void**, UINT*) {}
    virtual void WINAPI PSGetSamplers(UINT, UINT, void**) {}
    virtual void WINAPI VSGetShader(ID3D11VertexShader**, void**, UINT*) {}
    virtual void WINAPI PSGetConstantBuffers(UINT, UINT, ID3D11Buffer**) {}
    virtual void WINAPI IAGetInputLayout(ID3D11InputLayout**) {}
    virtual void WINAPI IAGetVertexBuffers(UINT, UINT, ID3D11Buffer**, UINT*, UINT*) {}
    virtual void WINAPI IAGetIndexBuffer(ID3D11Buffer**, DXGI_FORMAT*, UINT*) {}
    virtual void WINAPI GSGetConstantBuffers(UINT, UINT, ID3D11Buffer**) {}
    virtual void WINAPI GSGetShader(void**, void**, UINT*) {}
    virtual void WINAPI IAGetPrimitiveTopology(UINT* p) { if (p) *p = 0; }
    virtual void WINAPI VSGetShaderResources(UINT, UINT, void**) {}
    virtual void WINAPI VSGetSamplers(UINT, UINT, void**) {}
    virtual void WINAPI GetPredication(void**, UINT*) {}
    virtual void WINAPI GSGetShaderResources(UINT, UINT, void**) {}
    virtual void WINAPI GSGetSamplers(UINT, UINT, void**) {}
    virtual void WINAPI OMGetRenderTargets(UINT, ID3D11RenderTargetView**, ID3D11RenderTargetView**) {}
    virtual void WINAPI OMGetRenderTargetsAndUnorderedAccessViews(UINT, ID3D11RenderTargetView**,
                                                                  void*, UINT, UINT, void**) {}
    virtual void WINAPI OMGetBlendState(ID3D11BlendState**, float*, UINT*) {}
    virtual void WINAPI OMGetDepthStencilState(ID3D11DepthStencilState**, UINT*) {}
    virtual void WINAPI SOGetTargets(UINT, ID3D11Buffer**) {}
    virtual void WINAPI RSGetState(void**) {}
    virtual void WINAPI RSGetViewports(UINT* p, D3D11_VIEWPORT*) { if (p) *p = 0; }
    virtual void WINAPI RSGetScissorRects(UINT* p, D3D11_BOX*) { if (p) *p = 0; }
    virtual void WINAPI HSGetShaderResources(UINT, UINT, void**) {}
    virtual void WINAPI HSGetShader(void**, void**, UINT*) {}
    virtual void WINAPI HSGetSamplers(UINT, UINT, void**) {}
    virtual void WINAPI HSGetConstantBuffers(UINT, UINT, ID3D11Buffer**) {}
    virtual void WINAPI DSGetShaderResources(UINT, UINT, void**) {}
    virtual void WINAPI DSGetShader(void**, void**, UINT*) {}
    virtual void WINAPI DSGetSamplers(UINT, UINT, void**) {}
    virtual void WINAPI DSGetConstantBuffers(UINT, UINT, ID3D11Buffer**) {}
    virtual void WINAPI CSGetShaderResources(UINT, UINT, void**) {}
    virtual void WINAPI CSGetUnorderedAccessViews(UINT, UINT, void**) {}
    virtual void WINAPI CSGetShader(void**, void**, UINT*) {}
    virtual void WINAPI CSGetSamplers(UINT, UINT, void**) {}
    virtual void WINAPI CSGetConstantBuffers(UINT, UINT, ID3D11Buffer**) {}
    virtual void WINAPI ClearState() {}
    virtual void WINAPI Flush() {}
    virtual UINT WINAPI GetType() { return 1; } // D3D11_DEVICE_CONTEXT_IMMEDIATE
    virtual UINT WINAPI GetContextFlags() { return 0; }
    virtual HRESULT WINAPI FinishCommandList(BOOL, void** pp) {
        if (pp) *pp = nullptr;
        return S_OK_;
    }
};

// ---- IDXGISwapChain (18 methods) -----------------------------------------
struct __attribute__((ms_abi)) IDXGIObject : public IUnknown {
    virtual HRESULT WINAPI SetPrivateData(REFGUID, UINT, const void*) { return S_OK_; }
    virtual HRESULT WINAPI SetPrivateDataInterface(REFGUID, const IUnknown*) { return S_OK_; }
    virtual HRESULT WINAPI GetPrivateData(REFGUID, UINT* sz, void*) {
        if (sz) *sz = 0;
        return DXGI_ERROR_NOT_FOUND_;
    }
    virtual HRESULT WINAPI GetParent(REFIID, void** pp) {
        if (pp) *pp = nullptr;
        return DXGI_ERROR_INVALID_CALL_;
    }
};

struct __attribute__((ms_abi)) IDXGISwapChain : public IDXGIObject {
    virtual HRESULT WINAPI GetDevice(REFIID, void** pp) {
        if (pp) *pp = nullptr;
        return E_NOINTERFACE_;
    }
    virtual HRESULT WINAPI Present(UINT, UINT) { return S_OK_; }
    virtual HRESULT WINAPI GetBuffer(UINT, REFIID, void** pp) {
        if (pp) *pp = nullptr;
        D3D11_UNIMPL("IDXGISwapChain::GetBuffer");
        return E_NOTIMPL_;
    }
    virtual HRESULT WINAPI SetFullscreenState(BOOL, void*) { return S_OK_; }
    virtual HRESULT WINAPI GetFullscreenState(BOOL* p, void**) {
        if (p) *p = FALSE;
        return S_OK_;
    }
    virtual HRESULT WINAPI GetDesc(DXGI_SWAP_CHAIN_DESC* p) {
        if (p) std::memset(p, 0, sizeof(*p));
        return S_OK_;
    }
    virtual HRESULT WINAPI ResizeBuffers(UINT, UINT, UINT, DXGI_FORMAT, UINT) {
        return S_OK_;
    }
    virtual HRESULT WINAPI ResizeTarget(const DXGI_MODE_DESC*) { return S_OK_; }
    virtual HRESULT WINAPI GetContainingOutput(void** pp) {
        if (pp) *pp = nullptr;
        return DXGI_ERROR_INVALID_CALL_;
    }
    virtual HRESULT WINAPI GetFrameStatistics(void* p) {
        if (p) std::memset(p, 0, 24); // sizeof(DXGI_FRAME_STATISTICS)
        return S_OK_;
    }
    virtual HRESULT WINAPI GetLastPresentCount(UINT* p) {
        if (p) *p = 0;
        return S_OK_;
    }
};

// --------------------------------------------------------------------------
// Forward declarations for mock classes.
// --------------------------------------------------------------------------
class MockD3D11Buffer;
class MockD3D11DeviceContext;
class MockD3D11Device;
class MockD3D11InputLayout;
class MockD3D11VertexShader;
class MockD3D11PixelShader;
class MockD3D11RasterizerState;
class MockD3D11BlendState;
class MockD3D11DepthStencilState;
class MockDXGISwapChain;

// --------------------------------------------------------------------------
// MockD3D11InputLayout: stores D3D11 input elements + Vulkan vertex input.
// --------------------------------------------------------------------------
class MockD3D11InputLayout : public ComObjectBase<MockD3D11InputLayout, ID3D11InputLayout> {
public:
    bool query_extra(REFIID riid, void** ppv) {
        if (guid_eq(riid, IID_ID3D11DeviceChild_)) {
            *ppv = static_cast<ID3D11DeviceChild*>(this);
            return true;
        }
        if (guid_eq(riid, IID_ID3D11InputLayout_)) {
            *ppv = static_cast<ID3D11InputLayout*>(this);
            return true;
        }
        return false;
    }

    // Populated by CreateInputLayout.
    std::vector<VkVertexInputBindingDescription> bindings;
    std::vector<VkVertexInputAttributeDescription> attributes;
};

// --------------------------------------------------------------------------
// MockD3D11VertexShader / MockD3D11PixelShader: hold SPIR-V modules.
// --------------------------------------------------------------------------
class MockD3D11VertexShader : public ComObjectBase<MockD3D11VertexShader, ID3D11VertexShader> {
public:
    bool query_extra(REFIID riid, void** ppv) {
        if (guid_eq(riid, IID_ID3D11DeviceChild_)) {
            *ppv = static_cast<ID3D11DeviceChild*>(this);
            return true;
        }
        if (guid_eq(riid, IID_ID3D11VertexShader_)) {
            *ppv = static_cast<ID3D11VertexShader*>(this);
            return true;
        }
        return false;
    }

    // Owned SPIR-V module (created in CreateVertexShader, destroyed in dtor).
    VkShaderModule module = VK_NULL_HANDLE;
    MockD3D11Device* device = nullptr;

    ~MockD3D11VertexShader() {
        if (module && device) {
            vkDestroyShaderModule(d3d11vk::backend().device(), module, nullptr);
        }
    }
};

class MockD3D11PixelShader : public ComObjectBase<MockD3D11PixelShader, ID3D11PixelShader> {
public:
    bool query_extra(REFIID riid, void** ppv) {
        if (guid_eq(riid, IID_ID3D11DeviceChild_)) {
            *ppv = static_cast<ID3D11DeviceChild*>(this);
            return true;
        }
        if (guid_eq(riid, IID_ID3D11PixelShader_)) {
            *ppv = static_cast<ID3D11PixelShader*>(this);
            return true;
        }
        return false;
    }

    VkShaderModule module = VK_NULL_HANDLE;
    MockD3D11Device* device = nullptr;

    ~MockD3D11PixelShader() {
        if (module && device) {
            vkDestroyShaderModule(d3d11vk::backend().device(), module, nullptr);
        }
    }
};

// --------------------------------------------------------------------------
// MockD3D11RasterizerState / BlendState / DepthStencilState.
// Each holds a pre-built Vulkan pipeline state struct.
// --------------------------------------------------------------------------
class MockD3D11RasterizerState
    : public ComObjectBase<MockD3D11RasterizerState, ID3D11RasterizerState> {
public:
    bool query_extra(REFIID riid, void** ppv) {
        if (guid_eq(riid, IID_ID3D11DeviceChild_)) {
            *ppv = static_cast<ID3D11DeviceChild*>(this);
            return true;
        }
        if (guid_eq(riid, IID_ID3D11RasterizerState_)) {
            *ppv = static_cast<ID3D11RasterizerState*>(this);
            return true;
        }
        return false;
    }

    VkPipelineRasterizationStateCreateInfo raster{};
    VkCullModeFlags cull_mode = VK_CULL_MODE_BACK_BIT;
    VkPolygonMode polygon_mode = VK_POLYGON_MODE_FILL;
    VkFrontFace front_face = VK_FRONT_FACE_COUNTER_CLOCKWISE;
};

class MockD3D11BlendState : public ComObjectBase<MockD3D11BlendState, ID3D11BlendState> {
public:
    bool query_extra(REFIID riid, void** ppv) {
        if (guid_eq(riid, IID_ID3D11DeviceChild_)) {
            *ppv = static_cast<ID3D11DeviceChild*>(this);
            return true;
        }
        if (guid_eq(riid, IID_ID3D11BlendState_)) {
            *ppv = static_cast<ID3D11BlendState*>(this);
            return true;
        }
        return false;
    }

    VkPipelineColorBlendAttachmentState blend_attachment{};
    VkPipelineColorBlendStateCreateInfo blend{};
};

class MockD3D11DepthStencilState
    : public ComObjectBase<MockD3D11DepthStencilState, ID3D11DepthStencilState> {
public:
    bool query_extra(REFIID riid, void** ppv) {
        if (guid_eq(riid, IID_ID3D11DeviceChild_)) {
            *ppv = static_cast<ID3D11DeviceChild*>(this);
            return true;
        }
        if (guid_eq(riid, IID_ID3D11DepthStencilState_)) {
            *ppv = static_cast<ID3D11DepthStencilState*>(this);
            return true;
        }
        return false;
    }

    VkPipelineDepthStencilStateCreateInfo depth_stencil{};
};

// --------------------------------------------------------------------------
// MockDXGISwapChain: offscreen "swap chain" that writes a PPM on Present.
// --------------------------------------------------------------------------
class MockDXGISwapChain : public ComObjectBase<MockDXGISwapChain, IDXGISwapChain> {
public:
    bool query_extra(REFIID riid, void** ppv) {
        if (guid_eq(riid, IID_IDXGISwapChain_)) {
            *ppv = static_cast<IDXGISwapChain*>(this);
            return true;
        }
        return false;
    }

    // IDXGISwapChain
    HRESULT WINAPI Present(UINT SyncInterval, UINT Flags) override {
        (void)SyncInterval;
        (void)Flags;
        auto& vk = d3d11vk::backend();
        if (!vk.is_ready() || !vk.render_target().image) {
            D3D11_INFO("Present: no render target to read back");
            return S_OK_;
        }
        // Wait for any in-flight draws to finish.
        vkDeviceWaitIdle(vk.device());
        // Read back and write PPM.
        bool ok = vk.readback_and_write_ppm(ppm_path_);
        D3D11_INFO("Present: readback %s", ok ? "OK" : "FAILED");
        return ok ? S_OK_ : E_FAIL_;
    }

    HRESULT WINAPI GetBuffer(UINT Buffer, REFIID riid, void** pp) override {
        (void)Buffer;
        if (pp) *pp = nullptr;
        // Could return a texture wrapping the RT image; Phase 3: not needed.
        D3D11_UNIMPL("IDXGISwapChain::GetBuffer");
        return E_NOTIMPL_;
    }

    void set_ppm_path(const char* path) {
        ppm_path_ = path ? path : "triangle.ppm";
    }

private:
    std::string ppm_path_ = "triangle.ppm";
};

// --------------------------------------------------------------------------
// MockD3D11Buffer: wraps a real VkBuffer + VkDeviceMemory.
// --------------------------------------------------------------------------
class MockD3D11Buffer : public ComObjectBase<MockD3D11Buffer, ID3D11Buffer> {
public:
    bool query_extra(REFIID riid, void** ppv) {
        if (guid_eq(riid, IID_ID3D11DeviceChild_)) {
            *ppv = static_cast<ID3D11DeviceChild*>(this);
            return true;
        }
        if (guid_eq(riid, IID_ID3D11Resource_)) {
            *ppv = static_cast<ID3D11Resource*>(this);
            return true;
        }
        if (guid_eq(riid, IID_ID3D11Buffer_)) {
            *ppv = static_cast<ID3D11Buffer*>(this);
            return true;
        }
        return false;
    }

    // ID3D11DeviceChild
    HRESULT WINAPI GetDevice(ID3D11Device** ppDevice) override;

    // ID3D11Resource
    void WINAPI GetType(UINT* pResourceDimension) override {
        if (pResourceDimension) *pResourceDimension = 3; // D3D11_RESOURCE_DIMENSION_BUFFER
    }

    // ID3D11Buffer
    void WINAPI GetDesc(D3D11_BUFFER_DESC* pDesc) override {
        if (pDesc) *pDesc = desc_;
    }

    // Internal accessors.
    const D3D11_BUFFER_DESC& desc() const { return desc_; }
    d3d11vk::VulkanBuffer& vk() { return vk_buf_; }
    const d3d11vk::VulkanBuffer& vk() const { return vk_buf_; }

    void set_device(MockD3D11Device* dev) { device_ = dev; }

    ~MockD3D11Buffer() {
        if (vk_buf_.buffer) {
            d3d11vk::backend().destroy_buffer(&vk_buf_);
        }
    }

private:
    friend class MockD3D11Device;
    D3D11_BUFFER_DESC desc_{};
    d3d11vk::VulkanBuffer vk_buf_{};
    MockD3D11Device* device_ = nullptr;
};

// --------------------------------------------------------------------------
// MockD3D11DeviceContext: immediate context with state tracking.
// --------------------------------------------------------------------------
class MockD3D11DeviceContext : public ComObjectBase<MockD3D11DeviceContext, ID3D11DeviceContext> {
public:
    bool query_extra(REFIID riid, void** ppv) {
        if (guid_eq(riid, IID_ID3D11DeviceChild_)) {
            *ppv = static_cast<ID3D11DeviceChild*>(this);
            return true;
        }
        if (guid_eq(riid, IID_ID3D11DeviceContext_)) {
            *ppv = static_cast<ID3D11DeviceContext*>(this);
            return true;
        }
        return false;
    }

    HRESULT WINAPI GetDevice(ID3D11Device** ppDevice) override;

    ~MockD3D11DeviceContext() {
        destroy_pipeline();
    }

    // ---- Draw / Map --------------------------------------------------------
    void WINAPI Draw(UINT VertexCount, UINT StartVertexLocation) override;
    void WINAPI DrawIndexed(UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation) override;

    HRESULT WINAPI Map(ID3D11Buffer* pBuffer, UINT, D3D11_MAP MapType, UINT,
                       D3D11_MAPPED_SUBRESOURCE* pMapped) override {
        if (!pBuffer || !pMapped) return E_POINTER_;
        auto* buf = static_cast<MockD3D11Buffer*>(pBuffer);
        void* data = d3d11vk::backend().map_buffer(&buf->vk());
        if (!data) {
            D3D11_INFO("Map failed (buffer %p)", pBuffer);
            return E_NOTIMPL_;
        }
        pMapped->pData = data;
        pMapped->RowPitch = buf->desc().ByteWidth;
        pMapped->DepthPitch = buf->desc().ByteWidth;
        D3D11_INFO("Map(buffer=%p, type=%u) -> %p", pBuffer, MapType, data);
        return S_OK_;
    }

    void WINAPI Unmap(ID3D11Buffer* pBuffer, UINT) override {
        if (!pBuffer) return;
        auto* buf = static_cast<MockD3D11Buffer*>(pBuffer);
        d3d11vk::backend().unmap_buffer(&buf->vk());
        D3D11_INFO("Unmap(buffer=%p)", pBuffer);
    }

    // ---- Input assembler state --------------------------------------------
    void WINAPI IASetVertexBuffers(UINT StartSlot, UINT NumBuffers,
                                   ID3D11Buffer* const* ppBuffers,
                                   const UINT* pStrides, const UINT* pOffsets) override {
        (void)StartSlot;
        for (UINT i = 0; i < NumBuffers; ++i) {
            ia_vertex_buffers_[i] = ppBuffers ? ppBuffers[i] : nullptr;
            if (pStrides) ia_strides_[i] = pStrides[i];
            if (pOffsets) ia_offsets_[i] = pOffsets[i];
        }
        ia_num_vertex_buffers_ = NumBuffers;
        dirty_ = true;
    }

    void WINAPI IASetIndexBuffer(ID3D11Buffer* pBuffer, DXGI_FORMAT Format,
                                 UINT Offset) override {
        ia_index_buffer_ = pBuffer;
        ia_index_format_ = Format;
        ia_index_offset_ = Offset;
    }

    void WINAPI IASetInputLayout(ID3D11InputLayout* pLayout) override {
        ia_input_layout_ = pLayout;
        dirty_ = true;
    }

    void WINAPI IASetPrimitiveTopology(UINT Topology) override {
        ia_primitive_topology_ = Topology;
    }

    // ---- Shader state ------------------------------------------------------
    void WINAPI VSSetShader(ID3D11VertexShader* pShader, void* const*, UINT) override {
        vs_shader_ = pShader;
        dirty_ = true;
    }
    void WINAPI PSSetShader(ID3D11PixelShader* pShader, void* const*, UINT) override {
        ps_shader_ = pShader;
        dirty_ = true;
    }

    // ---- Rasterizer state --------------------------------------------------
    void WINAPI RSSetState(void* pState) override {
        rs_state_ = static_cast<MockD3D11RasterizerState*>(pState);
        dirty_ = true;
    }

    // ---- Output merger -----------------------------------------------------
    void WINAPI OMSetRenderTargets(UINT NumViews, ID3D11RenderTargetView* const* ppRTVs,
                                   ID3D11RenderTargetView* pDSV) override {
        (void)NumViews;
        om_render_targets_[0] = (ppRTVs && NumViews > 0) ? ppRTVs[0] : nullptr;
        om_depth_stencil_ = pDSV;
    }

    void WINAPI OMSetBlendState(ID3D11BlendState* pBlendState, const float*, UINT) override {
        om_blend_state_ = static_cast<MockD3D11BlendState*>(pBlendState);
        dirty_ = true;
    }

    void WINAPI OMSetDepthStencilState(ID3D11DepthStencilState* pState, UINT) override {
        om_depth_stencil_state_ = static_cast<MockD3D11DepthStencilState*>(pState);
        dirty_ = true;
    }

    void WINAPI ClearRenderTargetView(ID3D11RenderTargetView* pRTV,
                                      const float ColorRGBA[4]) override {
        (void)pRTV;
        if (ColorRGBA) {
            D3D11_INFO("ClearRenderTargetView(%.2f, %.2f, %.2f, %.2f)",
                       ColorRGBA[0], ColorRGBA[1], ColorRGBA[2], ColorRGBA[3]);
        }
    }

    void WINAPI ClearState() override {
        ia_num_vertex_buffers_ = 0;
        ia_index_buffer_ = nullptr;
        ia_input_layout_ = nullptr;
        vs_shader_ = nullptr;
        ps_shader_ = nullptr;
        rs_state_ = nullptr;
        om_render_targets_[0] = nullptr;
        om_depth_stencil_ = nullptr;
        om_blend_state_ = nullptr;
        om_depth_stencil_state_ = nullptr;
        dirty_ = true;
    }

    void WINAPI Flush() override {
        D3D11_INFO("Flush");
    }

    UINT draw_count() const { return draw_count_; }

    void set_device(MockD3D11Device* dev) { device_ = dev; }

private:
    friend class MockD3D11Device;

    // Pipeline management.
    bool ensure_pipeline();
    void destroy_pipeline();

    // Input assembler state.
    ID3D11Buffer* ia_vertex_buffers_[8] = {};
    UINT ia_strides_[8] = {};
    UINT ia_offsets_[8] = {};
    UINT ia_num_vertex_buffers_ = 0;
    ID3D11Buffer* ia_index_buffer_ = nullptr;
    DXGI_FORMAT ia_index_format_ = 0;
    UINT ia_index_offset_ = 0;
    ID3D11InputLayout* ia_input_layout_ = nullptr;
    UINT ia_primitive_topology_ = 0;

    // Shader state.
    ID3D11VertexShader* vs_shader_ = nullptr;
    ID3D11PixelShader* ps_shader_ = nullptr;

    // Rasterizer / blend / depth-stencil state.
    MockD3D11RasterizerState* rs_state_ = nullptr;
    MockD3D11BlendState* om_blend_state_ = nullptr;
    MockD3D11DepthStencilState* om_depth_stencil_state_ = nullptr;

    // Output merger.
    ID3D11RenderTargetView* om_render_targets_[8] = {};
    ID3D11RenderTargetView* om_depth_stencil_ = nullptr;

    // Vulkan pipeline (built lazily in Draw).
    VkPipeline pipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout_ = VK_NULL_HANDLE;
    bool dirty_ = true;

    UINT draw_count_ = 0;
    MockD3D11Device* device_ = nullptr;
};

// --------------------------------------------------------------------------
// MockD3D11Device: the central COM object.
// --------------------------------------------------------------------------
class MockD3D11Device : public ComObjectBase<MockD3D11Device, ID3D11Device> {
public:
    MockD3D11Device() {
        context_ = new MockD3D11DeviceContext();
        context_->set_device(this);
    }

    ~MockD3D11Device() {
        if (context_) context_->Release();
    }

    bool query_extra(REFIID riid, void** ppv) {
        if (guid_eq(riid, IID_ID3D11Device_)) {
            *ppv = static_cast<ID3D11Device*>(this);
            return true;
        }
        return false;
    }

    // ---- ID3D11Device::CreateBuffer ----------------------------------------
    HRESULT WINAPI CreateBuffer(const D3D11_BUFFER_DESC* pDesc, const void* pInitialData,
                                ID3D11Buffer** ppBuffer) override {
        if (!pDesc || !ppBuffer) return E_POINTER_;
        *ppBuffer = nullptr;
        if (pDesc->ByteWidth == 0) return E_INVALIDARG_;

        // Map D3D11 usage to Vulkan buffer usage + memory properties.
        VkBufferUsageFlags vk_usage = 0;
        if (pDesc->BindFlags & D3D11_BIND_VERTEX_BUFFER_)
            vk_usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        if (pDesc->BindFlags & D3D11_BIND_INDEX_BUFFER_)
            vk_usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        if (pDesc->BindFlags & D3D11_BIND_CONSTANT_BUFFER_)
            vk_usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        if (vk_usage == 0) vk_usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        VkMemoryPropertyFlags mem_props = 0;
        if (pDesc->Usage == D3D11_USAGE_DYNAMIC_ ||
            (pDesc->CPUAccessFlags & D3D11_CPU_ACCESS_WRITE_)) {
            mem_props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        } else if (pDesc->CPUAccessFlags & D3D11_CPU_ACCESS_READ_) {
            mem_props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        } else {
            mem_props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        }

        d3d11vk::VulkanBuffer vk_buf{};
        if (!d3d11vk::backend().create_buffer(pDesc->ByteWidth, vk_usage, mem_props, &vk_buf)) {
            D3D11_INFO("CreateBuffer: Vulkan allocation failed (size=%u)", pDesc->ByteWidth);
            return E_OUTOFMEMORY_;
        }

        // If initial data provided, copy it in.
        if (pInitialData) {
            void* dst = d3d11vk::backend().map_buffer(&vk_buf);
            if (dst) {
                std::memcpy(dst, pInitialData, pDesc->ByteWidth);
                d3d11vk::backend().unmap_buffer(&vk_buf);
            }
        }

        auto* buf = new MockD3D11Buffer();
        buf->desc_ = *pDesc;
        buf->vk_buf_ = vk_buf;
        buf->set_device(this);
        *ppBuffer = static_cast<ID3D11Buffer*>(buf);

        D3D11_INFO("Created VkBuffer 0x%llx for ID3D11Buffer %p (size=%u, usage=0x%x)",
                   static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(vk_buf.buffer)),
                   static_cast<void*>(buf), pDesc->ByteWidth, pDesc->BindFlags);
        return S_OK_;
    }

    // ---- ID3D11Device::CreateTexture2D --------------------------------------
    HRESULT WINAPI CreateTexture2D(const D3D11_TEXTURE2D_DESC* pDesc, const void*,
                                   ID3D11Texture2D** ppTexture2D) override {
        if (!pDesc || !ppTexture2D) return E_POINTER_;
        *ppTexture2D = nullptr;
        // Phase 2: create a mock texture without Vulkan image backing.
        // Phase 3 will wire VkImage / VkImageView.
        D3D11_UNIMPL("ID3D11Device::CreateTexture2D (mock, no VkImage yet)");
        return E_NOTIMPL_;
    }

    // ---- Other Create* methods return mock objects so the API surface works.
    HRESULT WINAPI CreateRenderTargetView(void* pResource, const void*,
                                          ID3D11RenderTargetView** pp) override {
        if (!pp) return E_POINTER_;
        *pp = nullptr;
        D3D11_UNIMPL("ID3D11Device::CreateRenderTargetView");
        return E_NOTIMPL_;
    }

    // ---- CreateInputLayout: map D3D11 elements to Vulkan vertex input ------
    HRESULT WINAPI CreateInputLayout(const D3D11_INPUT_ELEMENT_DESC* pDesc,
                                     UINT NumElements, const void*, SIZE_T,
                                     ID3D11InputLayout** pp) override {
        if (!pp) return E_POINTER_;
        *pp = nullptr;
        if (!pDesc || NumElements == 0) return E_INVALIDARG_;

        auto* layout = new MockD3D11InputLayout();

        // Group elements by input slot to build binding descriptions.
        // For simplicity assume one slot per element (typical for a triangle).
        for (UINT i = 0; i < NumElements; ++i) {
            const auto& e = pDesc[i];

            // Find or create a binding for this slot.
            VkVertexInputBindingDescription* binding = nullptr;
            for (auto& b : layout->bindings) {
                if (b.binding == e.InputSlot) {
                    binding = &b;
                    break;
                }
            }
            if (!binding) {
                layout->bindings.push_back(
                    {e.InputSlot, 0, VK_VERTEX_INPUT_RATE_VERTEX});
                binding = &layout->bindings.back();
            }

            // Compute format + size.
            VkFormat fmt = VK_FORMAT_R32G32B32_SFLOAT;
            uint32_t fmt_size = 12;
            if (e.Format == DXGI_FORMAT_R32G32_FLOAT_) {
                fmt = VK_FORMAT_R32G32_SFLOAT;
                fmt_size = 8;
            } else if (e.Format == DXGI_FORMAT_R32G32B32A32_FLOAT_) {
                fmt = VK_FORMAT_R32G32B32A32_SFLOAT;
                fmt_size = 16;
            }

            uint32_t offset = e.AlignedByteOffset;
            if (offset == D3D11_APPEND_ALIGNED_ELEMENT_) {
                offset = binding->stride;
            }
            binding->stride += fmt_size;

            layout->attributes.push_back(
                {i, e.InputSlot, fmt, offset});
        }

        *pp = static_cast<ID3D11InputLayout*>(layout);
        D3D11_INFO("CreateInputLayout: %u elements, %zu bindings, %zu attrs",
                   NumElements, layout->bindings.size(), layout->attributes.size());
        return S_OK_;
    }

    // ---- CreateVertexShader: ignore DXBC, return stub SPIR-V ----------------
    HRESULT WINAPI CreateVertexShader(const void* pShaderBytecode, SIZE_T BytecodeLength,
                                      void*, ID3D11VertexShader** pp) override {
        if (!pp) return E_POINTER_;
        *pp = nullptr;
        D3D11_INFO("CreateVertexShader called with %zu bytes of DXBC (using stub SPIR-V)",
                   static_cast<size_t>(BytecodeLength));

        auto* shader = new MockD3D11VertexShader();
        shader->device = this;

        VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        ci.codeSize = sizeof(d3d11vk::kStubVertexShaderSpv);
        ci.pCode = d3d11vk::kStubVertexShaderSpv;
        if (vkCreateShaderModule(d3d11vk::backend().device(), &ci, nullptr,
                                 &shader->module) != VK_SUCCESS) {
            delete shader;
            return E_FAIL_;
        }

        *pp = static_cast<ID3D11VertexShader*>(shader);
        D3D11_INFO("Created stub VkShaderModule %p for ID3D11VertexShader %p",
                   static_cast<void*>(shader->module), static_cast<void*>(shader));
        return S_OK_;
    }

    // ---- CreatePixelShader: ignore DXBC, return stub SPIR-V -----------------
    HRESULT WINAPI CreatePixelShader(const void* pShaderBytecode, SIZE_T BytecodeLength,
                                     void*, ID3D11PixelShader** pp) override {
        if (!pp) return E_POINTER_;
        *pp = nullptr;
        D3D11_INFO("CreatePixelShader called with %zu bytes of DXBC (using stub SPIR-V)",
                   static_cast<size_t>(BytecodeLength));

        auto* shader = new MockD3D11PixelShader();
        shader->device = this;

        VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        ci.codeSize = sizeof(d3d11vk::kStubPixelShaderSpv);
        ci.pCode = d3d11vk::kStubPixelShaderSpv;
        if (vkCreateShaderModule(d3d11vk::backend().device(), &ci, nullptr,
                                 &shader->module) != VK_SUCCESS) {
            delete shader;
            return E_FAIL_;
        }

        *pp = static_cast<ID3D11PixelShader*>(shader);
        D3D11_INFO("Created stub VkShaderModule %p for ID3D11PixelShader %p",
                   static_cast<void*>(shader->module), static_cast<void*>(shader));
        return S_OK_;
    }

    // ---- CreateRasterizerState ----------------------------------------------
    HRESULT WINAPI CreateRasterizerState(const void* pDesc,
                                         ID3D11RasterizerState** pp) override {
        if (!pp) return E_POINTER_;
        *pp = nullptr;
        auto* state = new MockD3D11RasterizerState();

        if (pDesc) {
            const auto* d = static_cast<const D3D11_RASTERIZER_DESC*>(pDesc);
            state->raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
            state->raster.polygonMode =
                (d->FillMode == D3D11_FILL_SOLID_) ? VK_POLYGON_MODE_FILL
                                                   : VK_POLYGON_MODE_LINE;
            state->raster.cullMode =
                (d->CullMode == D3D11_CULL_NONE_)   ? VK_CULL_MODE_NONE
                : (d->CullMode == D3D11_CULL_FRONT_) ? VK_CULL_MODE_FRONT_BIT
                                                     : VK_CULL_MODE_BACK_BIT;
            state->raster.frontFace = d->FrontCounterClockwise
                                          ? VK_FRONT_FACE_COUNTER_CLOCKWISE
                                          : VK_FRONT_FACE_CLOCKWISE;
            state->raster.depthClampEnable = VK_FALSE;
            state->raster.depthBiasEnable = VK_FALSE;
            state->raster.lineWidth = 1.0f;
        } else {
            // Defaults.
            state->raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
            state->raster.polygonMode = VK_POLYGON_MODE_FILL;
            state->raster.cullMode = VK_CULL_MODE_BACK_BIT;
            state->raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
            state->raster.lineWidth = 1.0f;
        }

        *pp = static_cast<ID3D11RasterizerState*>(state);
        D3D11_INFO("Created ID3D11RasterizerState %p", static_cast<void*>(state));
        return S_OK_;
    }

    // ---- CreateBlendState ----------------------------------------------------
    HRESULT WINAPI CreateBlendState(const void* pDesc,
                                    ID3D11BlendState** pp) override {
        if (!pp) return E_POINTER_;
        *pp = nullptr;
        auto* state = new MockD3D11BlendState();

        state->blend_attachment.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        state->blend_attachment.blendEnable = VK_FALSE;
        state->blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        state->blend.logicOpEnable = VK_FALSE;
        state->blend.attachmentCount = 1;
        state->blend.pAttachments = &state->blend_attachment;

        (void)pDesc; // use defaults for now
        *pp = static_cast<ID3D11BlendState*>(state);
        D3D11_INFO("Created ID3D11BlendState %p", static_cast<void*>(state));
        return S_OK_;
    }

    // ---- CreateDepthStencilState ----------------------------------------------
    HRESULT WINAPI CreateDepthStencilState(const void* pDesc,
                                           ID3D11DepthStencilState** pp) override {
        if (!pp) return E_POINTER_;
        *pp = nullptr;
        auto* state = new MockD3D11DepthStencilState();

        state->depth_stencil.sType =
            VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        state->depth_stencil.depthTestEnable = VK_FALSE;
        state->depth_stencil.depthWriteEnable = VK_FALSE;
        state->depth_stencil.depthCompareOp = VK_COMPARE_OP_ALWAYS;
        state->depth_stencil.stencilTestEnable = VK_FALSE;

        (void)pDesc;
        *pp = static_cast<ID3D11DepthStencilState*>(state);
        D3D11_INFO("Created ID3D11DepthStencilState %p", static_cast<void*>(state));
        return S_OK_;
    }

    HRESULT WINAPI CreateSamplerState(const void*, ID3D11SamplerState** pp) override {
        if (!pp) return E_POINTER_;
        *pp = nullptr;
        D3D11_UNIMPL("ID3D11Device::CreateSamplerState");
        return E_NOTIMPL_;
    }

    // ---- Getters ------------------------------------------------------------
    D3D_FEATURE_LEVEL WINAPI GetFeatureLevel() override { return D3D_FEATURE_LEVEL_11_0_; }

    HRESULT WINAPI GetDeviceRemovedReason() override { return S_OK_; }

    void WINAPI GetImmediateContext(ID3D11DeviceContext** pp) override {
        if (!pp) return;
        *pp = static_cast<ID3D11DeviceContext*>(context_);
        if (context_) context_->AddRef();
    }

    HRESULT WINAPI CheckFormatSupport(DXGI_FORMAT Format, UINT* pSupport) override {
        if (!pSupport) return E_POINTER_;
        // Advertise full support for common formats.
        *pSupport = 0x7F; // typical SUPPORT for R8G8B8A8 / R32G32B32_FLOAT
        (void)Format;
        return S_OK_;
    }

    HRESULT WINAPI CheckMultisampleQualityLevels(DXGI_FORMAT, UINT, UINT* pCount) override {
        if (pCount) *pCount = 1;
        return S_OK_;
    }

    MockD3D11DeviceContext* context() const { return context_; }

private:
    MockD3D11DeviceContext* context_ = nullptr;
};

// Implement GetDevice on buffer/context (needs MockD3D11Device complete).
HRESULT WINAPI MockD3D11Buffer::GetDevice(ID3D11Device** ppDevice) {
    if (!ppDevice) return E_POINTER_;
    if (device_) {
        *ppDevice = static_cast<ID3D11Device*>(device_);
        device_->AddRef();
        return S_OK_;
    }
    *ppDevice = nullptr;
    return E_POINTER_;
}

HRESULT WINAPI MockD3D11DeviceContext::GetDevice(ID3D11Device** ppDevice) {
    if (!ppDevice) return E_POINTER_;
    if (device_) {
        *ppDevice = static_cast<ID3D11Device*>(device_);
        device_->AddRef();
        return S_OK_;
    }
    *ppDevice = nullptr;
    return E_POINTER_;
}

// --------------------------------------------------------------------------
// Pipeline management (lazy build in Draw).
// --------------------------------------------------------------------------
void MockD3D11DeviceContext::destroy_pipeline() {
    VkDevice dev = d3d11vk::backend().device();
    if (pipeline_) {
        vkDestroyPipeline(dev, pipeline_, nullptr);
        pipeline_ = VK_NULL_HANDLE;
    }
    if (pipeline_layout_) {
        vkDestroyPipelineLayout(dev, pipeline_layout_, nullptr);
        pipeline_layout_ = VK_NULL_HANDLE;
    }
}

bool MockD3D11DeviceContext::ensure_pipeline() {
    if (pipeline_ && !dirty_) return true;
    if (pipeline_) destroy_pipeline();

    auto& vk = d3d11vk::backend();
    VkDevice dev = vk.device();
    if (!dev) return false;

    // Ensure render target + render pass + framebuffer exist.
    if (!vk.render_target().image) {
        // Default 640x480 if no swap chain was created.
        if (!vk.create_render_target(640, 480, VK_FORMAT_B8G8R8A8_UNORM))
            return false;
    }
    if (!vk.ensure_render_pass()) return false;
    if (!vk.ensure_framebuffer()) return false;

    // ---- Pipeline layout (empty for now; no descriptors yet) --------------
    VkPipelineLayoutCreateInfo plci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    VK_CHECK_D3D11(vkCreatePipelineLayout(dev, &plci, nullptr, &pipeline_layout_));

    // ---- Shader stages -----------------------------------------------------
    auto* vs = static_cast<MockD3D11VertexShader*>(vs_shader_);
    auto* ps = static_cast<MockD3D11PixelShader*>(ps_shader_);
    if (!vs || !ps || !vs->module || !ps->module) {
        D3D11_INFO("ensure_pipeline: missing shader modules");
        return false;
    }

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vs->module;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = ps->module;
    stages[1].pName = "main";

    // ---- Vertex input (from input layout) ---------------------------------
    auto* layout = static_cast<MockD3D11InputLayout*>(ia_input_layout_);
    VkPipelineVertexInputStateCreateInfo vertex_input{};
    vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    if (layout) {
        vertex_input.vertexBindingDescriptionCount =
            static_cast<uint32_t>(layout->bindings.size());
        vertex_input.pVertexBindingDescriptions = layout->bindings.data();
        vertex_input.vertexAttributeDescriptionCount =
            static_cast<uint32_t>(layout->attributes.size());
        vertex_input.pVertexAttributeDescriptions = layout->attributes.data();
    }

    // ---- Input assembly ----------------------------------------------------
    VkPipelineInputAssemblyStateCreateInfo input_assembly{};
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    // ---- Viewport / scissor (dynamic, full render target) ------------------
    VkPipelineViewportStateCreateInfo viewport{};
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;

    // ---- Rasterizer state --------------------------------------------------
    VkPipelineRasterizationStateCreateInfo raster{};
    raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode = VK_CULL_MODE_BACK_BIT;
    raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster.lineWidth = 1.0f;
    if (rs_state_) {
        raster = rs_state_->raster;
        raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        raster.lineWidth = 1.0f; // Vulkan requires >= 1.0
    }

    // ---- Multisample -------------------------------------------------------
    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // ---- Depth / stencil ---------------------------------------------------
    VkPipelineDepthStencilStateCreateInfo depth_stencil{};
    depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil.depthTestEnable = VK_FALSE;
    depth_stencil.depthWriteEnable = VK_FALSE;
    depth_stencil.depthCompareOp = VK_COMPARE_OP_ALWAYS;
    depth_stencil.stencilTestEnable = VK_FALSE;
    if (om_depth_stencil_state_) {
        depth_stencil = om_depth_stencil_state_->depth_stencil;
        depth_stencil.sType =
            VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    }

    // ---- Color blend -------------------------------------------------------
    VkPipelineColorBlendAttachmentState blend_attachment{};
    blend_attachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blend_attachment.blendEnable = VK_FALSE;
    if (om_blend_state_) {
        blend_attachment = om_blend_state_->blend_attachment;
    }

    VkPipelineColorBlendStateCreateInfo color_blend{};
    color_blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend.logicOpEnable = VK_FALSE;
    color_blend.attachmentCount = 1;
    color_blend.pAttachments = &blend_attachment;

    // ---- Dynamic states (viewport + scissor) --------------------------------
    VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT,
                                       VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic{};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = 2;
    dynamic.pDynamicStates = dynamic_states;

    // ---- Graphics pipeline --------------------------------------------------
    VkGraphicsPipelineCreateInfo gpci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    gpci.stageCount = 2;
    gpci.pStages = stages;
    gpci.pVertexInputState = &vertex_input;
    gpci.pInputAssemblyState = &input_assembly;
    gpci.pViewportState = &viewport;
    gpci.pRasterizationState = &raster;
    gpci.pMultisampleState = &multisample;
    gpci.pDepthStencilState = &depth_stencil;
    gpci.pColorBlendState = &color_blend;
    gpci.pDynamicState = &dynamic;
    gpci.layout = pipeline_layout_;
    gpci.renderPass = vk.render_pass();
    gpci.subpass = 0;

    VK_CHECK_D3D11(vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &gpci, nullptr,
                                       &pipeline_));
    dirty_ = false;
    D3D11_INFO("Created VkPipeline %p (dirty reset)",
               static_cast<void*>(pipeline_));
    return true;
}

// --------------------------------------------------------------------------
// Draw: build pipeline if needed, record command buffer, submit.
// --------------------------------------------------------------------------
void MockD3D11DeviceContext::Draw(UINT VertexCount, UINT StartVertexLocation) {
    D3D11_INFO("Draw(%u, %u)", VertexCount, StartVertexLocation);
    draw_count_++;

    auto& vk = d3d11vk::backend();
    VkDevice dev = vk.device();
    if (!dev) return;

    if (!ensure_pipeline()) {
        D3D11_INFO("Draw: pipeline creation failed");
        return;
    }

    // Allocate a one-shot command buffer.
    VkCommandBuffer cmd = vk.begin_one_shot();
    if (!cmd) {
        D3D11_INFO("Draw: begin_one_shot failed");
        return;
    }

    // Begin render pass with a dark clear color.
    VkClearValue clear{};
    clear.color = {{0.1f, 0.1f, 0.1f, 1.0f}};
    VkRenderPassBeginInfo rpbi{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rpbi.renderPass = vk.render_pass();
    rpbi.framebuffer = vk.framebuffer();
    rpbi.renderArea = {{0, 0}, {vk.render_target().width, vk.render_target().height}};
    rpbi.clearValueCount = 1;
    rpbi.pClearValues = &clear;
    vkCmdBeginRenderPass(cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE);

    // Viewport + scissor (full RT).
    VkViewport vp{};
    vp.x = 0;
    vp.y = 0;
    vp.width = static_cast<float>(vk.render_target().width);
    vp.height = static_cast<float>(vk.render_target().height);
    vp.minDepth = 0.0f;
    vp.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &vp);
    VkRect2D scissor{{0, 0},
                     {vk.render_target().width, vk.render_target().height}};
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // Bind pipeline.
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);

    // Bind vertex buffer(s).
    if (ia_num_vertex_buffers_ > 0 && ia_vertex_buffers_[0]) {
        auto* vbuf = static_cast<MockD3D11Buffer*>(ia_vertex_buffers_[0]);
        VkBuffer vk_buf = vbuf->vk().buffer;
        VkDeviceSize offset = ia_offsets_[0];
        vkCmdBindVertexBuffers(cmd, 0, 1, &vk_buf, &offset);
    }

    // Draw.
    vkCmdDraw(cmd, VertexCount, 1, StartVertexLocation, 0);
    vkCmdEndRenderPass(cmd);
    vk.end_one_shot(cmd);

    D3D11_INFO("Draw(%u): recorded and submitted VkCommandBuffer", VertexCount);
}

void MockD3D11DeviceContext::DrawIndexed(UINT IndexCount, UINT StartIndexLocation,
                                         INT BaseVertexLocation) {
    D3D11_INFO("DrawIndexed(%u, %u, %d)", IndexCount, StartIndexLocation,
               BaseVertexLocation);
    // Phase 3: treat like Draw for now (no index buffer binding yet).
    Draw(IndexCount, 0);
}

// --------------------------------------------------------------------------
// Factory creation helper.
// --------------------------------------------------------------------------
HRESULT create_device(ID3D11Device** ppDevice, ID3D11DeviceContext** ppContext) {
    if (!d3d11vk::backend().init()) {
        fprintf(stderr, "[NEXLINK D3D11] Vulkan backend init failed\n");
        return E_FAIL_;
    }

    auto* dev = new MockD3D11Device();
    if (ppDevice) {
        *ppDevice = static_cast<ID3D11Device*>(dev);
        dev->AddRef(); // QI-style: caller gets a reference
    }
    if (ppContext) {
        auto* ctx = dev->context();
        *ppContext = static_cast<ID3D11DeviceContext*>(ctx);
        ctx->AddRef();
    }
    dev->Release(); // drop the creation reference
    D3D11_INFO("D3D11 device created (Vulkan backend ready=%d)",
               d3d11vk::backend().is_ready() ? 1 : 0);
    return S_OK_;
}

} // namespace

// ==========================================================================
// Exported entry points
// ==========================================================================

WIN2LIN_EXPORT HRESULT WINAPI D3D11CreateDevice(
    void* pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags,
    const D3D_FEATURE_LEVEL* pFeatureLevels, UINT FeatureLevels, UINT SDKVersion,
    ID3D11Device** ppDevice, D3D_FEATURE_LEVEL* pFeatureLevel,
    ID3D11DeviceContext** ppImmediateContext) {
    (void)pAdapter;
    (void)DriverType;
    (void)Software;
    (void)Flags;
    (void)pFeatureLevels;
    (void)FeatureLevels;
    (void)SDKVersion;

    D3D11_INFO("D3D11CreateDevice(adapter=%p, driverType=%u)", pAdapter, DriverType);

    HRESULT hr = create_device(ppDevice, ppImmediateContext);
    if (SUCCEEDED(hr) && pFeatureLevel) *pFeatureLevel = D3D_FEATURE_LEVEL_11_0_;
    return hr;
}

WIN2LIN_EXPORT HRESULT WINAPI D3D11CreateDeviceAndSwapChain(
    void* pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags,
    const D3D_FEATURE_LEVEL* pFeatureLevels, UINT FeatureLevels, UINT SDKVersion,
    const DXGI_SWAP_CHAIN_DESC* pSwapChainDesc, IDXGISwapChain** ppSwapChain,
    ID3D11Device** ppDevice, D3D_FEATURE_LEVEL* pFeatureLevel,
    ID3D11DeviceContext** ppImmediateContext) {
    (void)pAdapter;
    (void)DriverType;
    (void)Software;
    (void)Flags;
    (void)pFeatureLevels;
    (void)FeatureLevels;
    (void)SDKVersion;

    D3D11_INFO("D3D11CreateDeviceAndSwapChain(adapter=%p, driverType=%u)",
               pAdapter, DriverType);

    // Create the device first.
    HRESULT hr = create_device(ppDevice, ppImmediateContext);
    if (FAILED(hr)) return hr;
    if (SUCCEEDED(hr) && pFeatureLevel) *pFeatureLevel = D3D_FEATURE_LEVEL_11_0_;

    // Create the offscreen render target matching the swap chain description.
    uint32_t width = 640, height = 480;
    if (pSwapChainDesc) {
        if (pSwapChainDesc->BufferDesc.Width) width = pSwapChainDesc->BufferDesc.Width;
        if (pSwapChainDesc->BufferDesc.Height) height = pSwapChainDesc->BufferDesc.Height;
    }
    auto& vk = d3d11vk::backend();
    if (!vk.render_target().image) {
        if (!vk.create_render_target(width, height, VK_FORMAT_B8G8R8A8_UNORM)) {
            fprintf(stderr, "[NEXLINK D3D11] swap chain render target failed\n");
        }
    }

    if (ppSwapChain) {
        auto* sc = new MockDXGISwapChain();
        sc->set_ppm_path("triangle.ppm");
        *ppSwapChain = static_cast<IDXGISwapChain*>(sc);
        D3D11_INFO("Created MockDXGISwapChain %p (%ux%u)",
                   static_cast<void*>(sc), width, height);
    }
    return hr;
}
