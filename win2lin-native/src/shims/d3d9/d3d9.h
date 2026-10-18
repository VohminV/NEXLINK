#pragma once

// libwin_d3d9.so - D3D9 shim type definitions.
// Implements IDirect3D9 / IDirect3DDevice9 COM vtables (__attribute__((ms_abi)))
// so Windows games from the 2000-2010 era can initialize D3D9 without DXVK.

#include <cstdint>
#include <cstddef>

using HRESULT = int32_t;
using DWORD = uint32_t;
using UINT = uint32_t;
using INT = int32_t;
using FLOAT = float;
using BOOL = int32_t;
using HANDLE = void*;
using HMONITOR = HANDLE;
using HWND = HANDLE;
using HINSTANCE = HANDLE;
using HDC = HANDLE;
using LPVOID = void*;
using LPCVOID = const void*;
using LPBYTE = uint8_t*;
using LPDWORD = DWORD*;
using UINT_PTR = uintptr_t;
using WORD = uint16_t;
using BYTE = uint8_t;
using LONG = int32_t;
using ULONG = uint32_t;
using LPCSTR = const char*;
using LPSTR = char*;
using LPCWSTR = const wchar_t*;
using LPWSTR = wchar_t*;
using REFCLSID = const struct GUID*;
using REFIID = const struct GUID*;
using VOID = void;

#define WINAPI __attribute__((ms_abi))

constexpr HRESULT D3D_OK = 0;
constexpr HRESULT D3DERR_INVALIDCALL = -2005530518;
constexpr HRESULT D3DERR_NOTAVAILABLE = -2005530516;
constexpr HRESULT D3DERR_OUTOFVIDEOMEMORY = -2005530514;
constexpr HRESULT D3DERR_DEVICELOST = -2005530520;
constexpr HRESULT D3DERR_DEVICENOTRESET = -2005530519;
constexpr HRESULT E_NOINTERFACE = -2147467263;
constexpr HRESULT S_OK = 0;
constexpr HRESULT E_FAIL = -2147467259;

// Forward declarations of all COM interfaces used in vtables
struct IDirect3D9;
struct IDirect3DDevice9;
struct IDirect3DTexture9;
struct IDirect3DVolumeTexture9;
struct IDirect3DCubeTexture9;
struct IDirect3DVertexBuffer9;
struct IDirect3DIndexBuffer9;
struct IDirect3DSurface9;
struct IDirect3DVolume9;
struct IDirect3DVertexDeclaration9;
struct IDirect3DVertexShader9;
struct IDirect3DPixelShader9;
struct IDirect3DStateBlock9;
struct IDirect3DQuery9;
struct IDirect3DBaseTexture9;
struct IDirect3DResource9;

// GUID
struct GUID {
    DWORD Data1;
    WORD Data2;
    WORD Data3;
    BYTE Data4[8];
};

// D3DFORMAT
enum D3DFORMAT : DWORD {
    D3DFMT_UNKNOWN = 0,
    D3DFMT_R8G8B8 = 20,
    D3DFMT_A8R8G8B8 = 21,
    D3DFMT_X8R8G8B8 = 22,
    D3DFMT_R5G6B5 = 23,
    D3DFMT_X1R5G5B5 = 24,
    D3DFMT_A1R5G5B5 = 25,
    D3DFMT_A4R4G4B4 = 26,
    D3DFMT_R3G3B2 = 27,
    D3DFMT_A8 = 28,
    D3DFMT_A8R3G3B2 = 29,
    D3DFMT_X4R4G4B4 = 30,
    D3DFMT_A2B10G10R10 = 31,
    D3DFMT_A8B8G8R8 = 32,
    D3DFMT_X8B8G8R8 = 33,
    D3DFMT_G16R16 = 34,
    D3DFMT_A2R10G10B10 = 35,
    D3DFMT_A16B16G16R16 = 36,
    D3DFMT_A8P8 = 40,
    D3DFMT_P8 = 41,
    D3DFMT_L8 = 50,
    D3DFMT_L8R8 = 51,
    D3DFMT_L16 = 81,
    D3DFMT_V8U8 = 82,
    D3DFMT_L6V5U5 = 83,
    D3DFMT_X8L8V8U8 = 84,
    D3DFMT_Q8W8V8U8 = 85,
    D3DFMT_V16U16 = 86,
    D3DFMT_A2W10V10U10 = 87,
    D3DFMT_D16_LOCKABLE = 70,
    D3DFMT_D32 = 71,
    D3DFMT_D15S1 = 73,
    D3DFMT_D24S8 = 75,
    D3DFMT_D24X8 = 77,
    D3DFMT_D24X4S4 = 79,
    D3DFMT_D16 = 80,
    D3DFMT_D32F_LOCKABLE = 82,
    D3DFMT_D24FS8 = 83,
    D3DFMT_D32_LOCKABLE = 84,
    D3DFMT_D24LOCKABLE = 85,
    D3DFMT_LIN_D16 = 130,
};

// D3DDEVTYPE
enum D3DDEVTYPE : DWORD {
    D3DDEVTYPE_HAL = 1,
    D3DDEVTYPE_REF = 2,
    D3DDEVTYPE_SW = 3,
    D3DDEVTYPE_NULLREF = 4,
};

// D3DMULTISAMPLE_TYPE
enum D3DMULTISAMPLE_TYPE : DWORD {
    D3DMULTISAMPLE_NONE = 0,
    D3DMULTISAMPLE_NONMASKABLE = 1,
    D3DMULTISAMPLE_2_SAMPLES = 2,
    D3DMULTISAMPLE_4_SAMPLES = 4,
    D3DMULTISAMPLE_8_SAMPLES = 8,
    D3DMULTISAMPLE_16_SAMPLES = 16,
};

// D3DSWAPEFFECT
enum D3DSWAPEFFECT : DWORD {
    D3DSWAPEFFECT_DISCARD = 1,
    D3DSWAPEFFECT_FLIP = 2,
    D3DSWAPEFFECT_COPY = 3,
};

// D3DPOOL
enum D3DPOOL : DWORD {
    D3DPOOL_DEFAULT = 0,
    D3DPOOL_MANAGED = 1,
    D3DPOOL_SYSTEMMEM = 2,
    D3DPOOL_SCRATCH = 3,
};

// D3DRESOURCETYPE
enum D3DRESOURCETYPE : DWORD {
    D3DRTYPE_SURFACE = 1,
    D3DRTYPE_VOLUME = 2,
    D3DRTYPE_TEXTURE = 3,
    D3DRTYPE_VOLUMETEXTURE = 4,
    D3DRTYPE_CUBETEXTURE = 5,
    D3DRTYPE_VERTEXBUFFER = 6,
    D3DRTYPE_INDEXBUFFER = 7,
};

// D3DPRESENT_PARAMETERS
struct D3DPRESENT_PARAMETERS {
    UINT BackBufferWidth;
    UINT BackBufferHeight;
    D3DFORMAT BackBufferFormat;
    UINT BackBufferCount;
    D3DMULTISAMPLE_TYPE MultiSampleType;
    DWORD MultiSampleQuality;
    D3DSWAPEFFECT SwapEffect;
    HWND hDeviceWindow;
    BOOL Windowed;
    BOOL EnableAutoDepthStencil;
    D3DFORMAT AutoDepthStencilFormat;
    DWORD Flags;
    UINT FullScreen_RefreshRateInHz;
    UINT PresentationInterval;
};

// D3DDISPLAYMODE
struct D3DDISPLAYMODE {
    UINT Width;
    UINT Height;
    DWORD RefreshRate;
    D3DFORMAT Format;
};

// D3DCAPS9 (subset)
struct D3DCAPS9 {
    DWORD DeviceType;
    DWORD AdapterOrdinal;
    DWORD Caps;
    DWORD Caps2;
    DWORD Caps3;
    DWORD PresentationIntervals;
    DWORD CursorCaps;
    DWORD DevCaps;
    DWORD PrimitiveMiscCaps;
    DWORD RasterCaps;
    DWORD ZCmpCaps;
    DWORD SrcBlendCaps;
    DWORD DestBlendCaps;
    DWORD AlphaCmpCaps;
    DWORD ShadeCaps;
    DWORD TextureCaps;
    DWORD TextureFilterCaps;
    DWORD CubeTextureFilterCaps;
    DWORD VolumeTextureFilterCaps;
    DWORD TextureAddressCaps;
    DWORD VolumeTextureAddressCaps;
    DWORD LineCaps;
    DWORD MaxTextureWidth;
    DWORD MaxTextureHeight;
    DWORD MaxVolumeExtent;
    DWORD MaxTextureRepeat;
    DWORD MaxTextureAspectRatio;
    DWORD MaxAnisotropy;
    DWORD MaxVertexW;
    DWORD GuardBandLeft;
    DWORD GuardBandTop;
    DWORD GuardBandRight;
    DWORD GuardBandBottom;
    DWORD ExtentsAdjust;
    DWORD StencilCaps;
    DWORD FVFCaps;
    DWORD TextureOpCaps;
    DWORD MaxTextureBlendStages;
    DWORD MaxSimultaneousTextures;
    DWORD VertexProcessingCaps;
    DWORD MaxStreams;
    DWORD MaxStreamStride;
    DWORD VertexShaderVersion;
    DWORD MaxVertexShaderConst;
    DWORD PixelShaderVersion;
    DWORD PixelShader1xMaxValue;
    DWORD DevCaps2;
    DWORD MaxNpatchTessellationLevel;
    DWORD Reserved5;
    DWORD MasterAdapterOrdinal;
    DWORD AdapterOrdinalInGroup;
    UINT NumberOfAdaptersInGroup;
    DWORD DeclTypes;
    DWORD NumSimultaneousRTs;
    DWORD StretchRectFilterCaps;
    DWORD VS20Caps;
    DWORD PS20Caps;
    DWORD PS20Caps2;
    DWORD VertexTextureFilterCaps;
    DWORD MaxVShaderInstructionsExecuted;
    DWORD MaxPShaderInstructionsExecuted;
};

// D3DDEVICE_CREATION_PARAMETERS
struct D3DDEVICE_CREATION_PARAMETERS {
    UINT AdapterOrdinal;
    D3DDEVTYPE DeviceType;
    HWND hFocusWindow;
    DWORD BehaviorFlags;
};

// D3DVIEWPORT9
struct D3DVIEWPORT9 {
    DWORD X;
    DWORD Y;
    DWORD Width;
    DWORD Height;
    FLOAT MinZ;
    FLOAT MaxZ;
};

// D3DMATRIX
struct D3DMATRIX {
    FLOAT m[4][4];
};

// D3DRENDERSTATETYPE (subset)
enum D3DRENDERSTATETYPE : DWORD {
    D3DRS_ZENABLE = 7,
    D3DRS_FILLMODE = 8,
    D3DRS_SHADEMODE = 9,
    D3DRS_ZWRITEENABLE = 14,
    D3DRS_ALPHATESTENABLE = 15,
    D3DRS_LASTPIXEL = 16,
    D3DRS_SRCBLEND = 19,
    D3DRS_DESTBLEND = 20,
    D3DRS_CULLMODE = 22,
    D3DRS_ZFUNC = 23,
    D3DRS_ALPHAREF = 24,
    D3DRS_ALPHAFUNC = 25,
    D3DRS_DITHERENABLE = 26,
    D3DRS_ALPHABLENDENABLE = 27,
    D3DRS_FOGENABLE = 28,
    D3DRS_SPECULARENABLE = 29,
    D3DRS_FOGCOLOR = 34,
    D3DRS_FOGTABLEMODE = 35,
    D3DRS_FOGSTART = 36,
    D3DRS_FOGEND = 37,
    D3DRS_FOGDENSITY = 38,
    D3DRS_RANGEFOGENABLE = 48,
    D3DRS_STENCILENABLE = 52,
    D3DRS_STENCILFAIL = 53,
    D3DRS_STENCILZFAIL = 54,
    D3DRS_STENCILPASS = 55,
    D3DRS_STENCILFUNC = 56,
    D3DRS_STENCILREF = 57,
    D3DRS_STENCILMASK = 58,
    D3DRS_STENCILWRITEMASK = 59,
    D3DRS_TEXTUREFACTOR = 60,
    D3DRS_WRAP0 = 128,
    D3DRS_CLIPPING = 136,
    D3DRS_LIGHTING = 137,
    D3DRS_AMBIENT = 139,
    D3DRS_FOGVERTEXMODE = 156,
    D3DRS_COLORVERTEX = 161,
    D3DRS_LOCALVIEWER = 162,
    D3DRS_NORMALIZENORMALS = 163,
    D3DRS_DIFFUSEMATERIALSOURCE = 166,
    D3DRS_SPECULARMATERIALSOURCE = 167,
    D3DRS_AMBIENTMATERIALSOURCE = 168,
    D3DRS_BLENDFACTOR = 171,
    D3DRS_SRGBWRITEENABLE = 176,
    D3DRS_DEPTHBIAS = 177,
    D3DRS_SEPARATEALPHABLENDENABLE = 188,
    D3DRS_SRCBLENDALPHA = 189,
    D3DRS_DESTBLENDALPHA = 190,
    D3DRS_BLENDOPALPHA = 191,
};

// D3DSAMPLERSTATETYPE
enum D3DSAMPLERSTATETYPE : DWORD {
    D3DSAMP_ADDRESSU = 1,
    D3DSAMP_ADDRESSV = 2,
    D3DSAMP_ADDRESSW = 3,
    D3DSAMP_BORDERCOLOR = 4,
    D3DSAMP_MAGFILTER = 5,
    D3DSAMP_MINFILTER = 6,
    D3DSAMP_MIPFILTER = 7,
    D3DSAMP_MIPMAPLODBIAS = 8,
    D3DSAMP_MAXMIPLEVEL = 9,
    D3DSAMP_MAXANISOTROPY = 10,
    D3DSAMP_SRGBTEXTURE = 11,
};

// D3DTEXTUREFILTERTYPE
enum D3DTEXTUREFILTERTYPE : DWORD {
    D3DTEXF_NONE = 0,
    D3DTEXF_POINT = 1,
    D3DTEXF_LINEAR = 2,
    D3DTEXF_ANISOTROPIC = 3,
};

// D3DTEXTUREADDRESS
enum D3DTEXTUREADDRESS : DWORD {
    D3DTADDRESS_WRAP = 1,
    D3DTADDRESS_MIRROR = 2,
    D3DTADDRESS_CLAMP = 3,
    D3DTADDRESS_BORDER = 4,
    D3DTADDRESS_MIRRORONCE = 5,
};

// D3DPRIMITIVETYPE
enum D3DPRIMITIVETYPE : DWORD {
    D3DPT_POINTLIST = 1,
    D3DPT_LINELIST = 2,
    D3DPT_LINESTRIP = 3,
    D3DPT_TRIANGLELIST = 4,
    D3DPT_TRIANGLESTRIP = 5,
    D3DPT_TRIANGLEFAN = 6,
};
using D3DPTRIMITIVETYPE = D3DPRIMITIVETYPE;

// D3DTRANSFORMSTATETYPE
enum D3DTRANSFORMSTATETYPE : DWORD {
    D3DTS_VIEW = 2,
    D3DTS_PROJECTION = 3,
    D3DTS_WORLD = 256,
    D3DTS_WORLD0 = 256,
    D3DTS_WORLD1 = 257,
    D3DTS_WORLD2 = 258,
    D3DTS_WORLD3 = 259,
    D3DTS_WORLD4 = 260,
    D3DTS_WORLD5 = 261,
    D3DTS_WORLD6 = 262,
    D3DTS_WORLD7 = 263,
    D3DTS_WORLD8 = 264,
    D3DTS_WORLD9 = 265,
    D3DTS_WORLD10 = 266,
    D3DTS_WORLD11 = 267,
    D3DTS_WORLD12 = 268,
    D3DTS_WORLD13 = 269,
    D3DTS_WORLD14 = 270,
    D3DTS_WORLD15 = 271,
    D3DTS_TEXTURE0 = 512,
    D3DTS_TEXTURE1 = 513,
    D3DTS_TEXTURE2 = 514,
    D3DTS_TEXTURE3 = 515,
    D3DTS_TEXTURE4 = 516,
    D3DTS_TEXTURE5 = 517,
    D3DTS_TEXTURE6 = 518,
    D3DTS_TEXTURE7 = 519,
};

// D3DSTATEBLOCKTYPE
enum D3DSTATEBLOCKTYPE : DWORD {
    D3DSBT_ALL = 1,
    D3DSBT_PIXELSTATE = 2,
    D3DSBT_VERTEXSTATE = 3,
};

// D3DQUERYTYPE
enum D3DQUERYTYPE : DWORD {
    D3DQUERYTYPE_VCACHE = 4,
    D3DQUERYTYPE_RESOURCEMANAGER = 5,
    D3DQUERYTYPE_VERTEXSTATS = 6,
    D3DQUERYTYPE_EVENT = 8,
    D3DQUERYTYPE_OCCLUSION = 9,
};

// Misc structures
struct D3DLOCKED_RECT {
    INT Pitch;
    LPVOID pBits;
};

struct D3DRECT {
    LONG x1;
    LONG y1;
    LONG x2;
    LONG y2;
};

using D3DCOLOR = DWORD;

struct D3DVERTEXELEMENT9 {
    WORD Stream;
    WORD Offset;
    BYTE Type;
    BYTE Method;
    BYTE Usage;
    BYTE UsageIndex;
};

enum D3DDECLTYPE : DWORD {
    D3DDECLTYPE_FLOAT1 = 0,
    D3DDECLTYPE_FLOAT2 = 1,
    D3DDECLTYPE_FLOAT3 = 2,
    D3DDECLTYPE_FLOAT4 = 3,
};

enum D3DDECLMETHOD : DWORD {
    D3DDECLMETHOD_DEFAULT = 0,
};

enum D3DDECLUSAGE : DWORD {
    D3DDECLUSAGE_POSITION = 0,
    D3DDECLUSAGE_BLENDWEIGHT = 1,
    D3DDECLUSAGE_BLENDINDICES = 2,
    D3DDECLUSAGE_NORMAL = 3,
    D3DDECLUSAGE_PSIZE = 4,
    D3DDECLUSAGE_TEXCOORD = 5,
    D3DDECLUSAGE_TANGENT = 6,
    D3DDECLUSAGE_BINORMAL = 7,
    D3DDECLUSAGE_POSITIONT = 10,
    D3DDECLUSAGE_COLOR = 11,
    D3DDECLUSAGE_FOG = 12,
    D3DDECLUSAGE_DEPTH = 13,
    D3DDECLUSAGE_SAMPLE = 14,
};

struct D3DCLIPSTATUS9 {
    DWORD ClipUnion;
    DWORD ClipIntersection;
};

struct D3DMATERIAL9 {
    D3DCOLOR Diffuse;
    D3DCOLOR Ambient;
    D3DCOLOR Specular;
    D3DCOLOR Emissive;
    FLOAT Power;
};

struct D3DVECTOR {
    FLOAT x, y, z;
};

struct D3DCOLORVALUE {
    FLOAT r, g, b, a;
};

struct D3DLIGHT9 {
    DWORD Type;
    D3DCOLORVALUE Diffuse;
    D3DCOLORVALUE Specular;
    D3DCOLORVALUE Ambient;
    D3DVECTOR Position;
    D3DVECTOR Direction;
    FLOAT Range;
    FLOAT Falloff;
    FLOAT Attenuation0;
    FLOAT Attenuation1;
    FLOAT Attenuation2;
    FLOAT Theta;
    FLOAT Phi;
};

enum D3DLIGHTTYPE : DWORD {
    D3DLIGHT_POINT = 1,
    D3DLIGHT_SPOT = 2,
    D3DLIGHT_DIRECTIONAL = 3,
};

struct D3DBOX {
    UINT Left;
    UINT Top;
    UINT Right;
    UINT Bottom;
    UINT Front;
    UINT Back;
};

struct D3DLOCKED_BOX {
    INT RowPitch;
    INT SlicePitch;
    LPVOID pBits;
};

struct D3DRECTPATCH_INFO {
    UINT StartVertexOffsetWidth;
    UINT StartVertexOffsetHeight;
    UINT Width;
    UINT Height;
    UINT Stride;
    DWORD BasisType;
    DWORD Degree;
};

struct D3DTRIPATCH_INFO {
    UINT StartVertexOffset;
    UINT NumVertices;
    BOOL UseIndices;
    DWORD BasisType;
    DWORD Degree;
};

enum D3DBASISTYPE : DWORD {
    D3DBASIS_BEZIER = 0,
    D3DBASIS_BSPLINE = 1,
    D3DBASIS_CATMULL_ROM = 2,
};

enum D3DDEGREETYPE : DWORD {
    D3DDEGREE_LINEAR = 1,
    D3DDEGREE_QUADRATIC = 2,
    D3DDEGREE_CUBIC = 3,
};

using RGNDATA = void*;

// ---------------------------------------------------------------------------
// COM vtables
// ---------------------------------------------------------------------------
struct IDirect3D9Vtbl {
    HRESULT (WINAPI *QueryInterface)(IDirect3D9*, REFIID, LPVOID*);
    ULONG (WINAPI *AddRef)(IDirect3D9*);
    ULONG (WINAPI *Release)(IDirect3D9*);
    HRESULT (WINAPI *RegisterSoftwareDevice)(IDirect3D9*, void*);
    UINT (WINAPI *GetAdapterCount)(IDirect3D9*);
    HRESULT (WINAPI *GetAdapterIdentifier)(IDirect3D9*, UINT, DWORD, void*);
    UINT (WINAPI *GetAdapterModeCount)(IDirect3D9*, UINT, D3DFORMAT);
    HRESULT (WINAPI *EnumAdapterModes)(IDirect3D9*, UINT, D3DFORMAT, UINT, D3DDISPLAYMODE*);
    HRESULT (WINAPI *GetAdapterDisplayMode)(IDirect3D9*, UINT, D3DDISPLAYMODE*);
    HRESULT (WINAPI *CheckDeviceType)(IDirect3D9*, UINT, D3DDEVTYPE, D3DFORMAT, D3DFORMAT, BOOL);
    HRESULT (WINAPI *CheckDeviceFormat)(IDirect3D9*, UINT, D3DDEVTYPE, D3DFORMAT, DWORD, D3DRESOURCETYPE, D3DFORMAT);
    HRESULT (WINAPI *CheckDeviceMultiSampleType)(IDirect3D9*, UINT, D3DDEVTYPE, D3DFORMAT, BOOL, D3DMULTISAMPLE_TYPE, DWORD*);
    HRESULT (WINAPI *CheckDepthStencilMatch)(IDirect3D9*, UINT, D3DDEVTYPE, D3DFORMAT, D3DFORMAT, D3DFORMAT);
    HRESULT (WINAPI *CheckDeviceFormatConversion)(IDirect3D9*, UINT, D3DDEVTYPE, D3DFORMAT, D3DFORMAT);
    HRESULT (WINAPI *GetDeviceCaps)(IDirect3D9*, UINT, D3DDEVTYPE, D3DCAPS9*);
    HMONITOR (WINAPI *GetAdapterMonitor)(IDirect3D9*, UINT);
    HRESULT (WINAPI *CreateDevice)(IDirect3D9*, UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS*, IDirect3DDevice9**);
};

struct IDirect3D9 {
    const IDirect3D9Vtbl* lpVtbl;
};

struct IDirect3DDevice9Vtbl {
    HRESULT (WINAPI *QueryInterface)(IDirect3DDevice9*, REFIID, LPVOID*);
    ULONG (WINAPI *AddRef)(IDirect3DDevice9*);
    ULONG (WINAPI *Release)(IDirect3DDevice9*);
    HRESULT (WINAPI *TestCooperativeLevel)(IDirect3DDevice9*);
    UINT (WINAPI *GetAvailableTextureMem)(IDirect3DDevice9*);
    HRESULT (WINAPI *EvictManagedResources)(IDirect3DDevice9*);
    HRESULT (WINAPI *GetDirect3D)(IDirect3DDevice9*, IDirect3D9**);
    HRESULT (WINAPI *GetDeviceCaps)(IDirect3DDevice9*, D3DCAPS9*);
    HRESULT (WINAPI *GetDisplayMode)(IDirect3DDevice9*, UINT, D3DDISPLAYMODE*);
    HRESULT (WINAPI *GetCreationParameters)(IDirect3DDevice9*, D3DDEVICE_CREATION_PARAMETERS*);
    HRESULT (WINAPI *SetCursorProperties)(IDirect3DDevice9*, UINT, UINT, IDirect3DSurface9*);
    HRESULT (WINAPI *SetCursorPosition)(IDirect3DDevice9*, INT, INT, DWORD);
    BOOL (WINAPI *ShowCursor)(IDirect3DDevice9*, BOOL);
    HRESULT (WINAPI *CreateRenderTarget)(IDirect3DDevice9*, UINT, UINT, D3DFORMAT, D3DMULTISAMPLE_TYPE, DWORD, BOOL, IDirect3DSurface9**, HANDLE*);
    HRESULT (WINAPI *CreateDepthStencilSurface)(IDirect3DDevice9*, UINT, UINT, D3DFORMAT, D3DMULTISAMPLE_TYPE, DWORD, BOOL, IDirect3DSurface9**, HANDLE*);
    HRESULT (WINAPI *CreateSurface)(IDirect3DDevice9*, UINT, UINT, D3DFORMAT, D3DMULTISAMPLE_TYPE, DWORD, BOOL, IDirect3DSurface9**, HANDLE*);
    HRESULT (WINAPI *CreateTexture)(IDirect3DDevice9*, UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DTexture9**, HANDLE*);
    HRESULT (WINAPI *CreateVolumeTexture)(IDirect3DDevice9*, UINT, UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DVolumeTexture9**, HANDLE*);
    HRESULT (WINAPI *CreateCubeTexture)(IDirect3DDevice9*, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DCubeTexture9**, HANDLE*);
    HRESULT (WINAPI *CreateVertexBuffer)(IDirect3DDevice9*, UINT, DWORD, DWORD, D3DPOOL, IDirect3DVertexBuffer9**, HANDLE*);
    HRESULT (WINAPI *CreateIndexBuffer)(IDirect3DDevice9*, UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DIndexBuffer9**, HANDLE*);
    HRESULT (WINAPI *CreateImageSurface)(IDirect3DDevice9*, UINT, UINT, D3DFORMAT, IDirect3DSurface9**);
    HRESULT (WINAPI *LockRect)(IDirect3DSurface9*, D3DLOCKED_RECT*, const D3DRECT*, DWORD);
    HRESULT (WINAPI *UnlockRect)(IDirect3DSurface9*);
    HRESULT (WINAPI *LockBox)(IDirect3DVolume9*, D3DLOCKED_BOX*, const D3DBOX*, DWORD);
    HRESULT (WINAPI *UnlockBox)(IDirect3DVolume9*);
    HRESULT (WINAPI *GetRectangle)(IDirect3DSurface9*, D3DRECT*);
    HRESULT (WINAPI *SetRenderTarget)(IDirect3DDevice9*, DWORD, IDirect3DSurface9*);
    HRESULT (WINAPI *GetRenderTarget)(IDirect3DDevice9*, DWORD, IDirect3DSurface9**);
    HRESULT (WINAPI *SetDepthStencilSurface)(IDirect3DDevice9*, IDirect3DSurface9*);
    HRESULT (WINAPI *GetDepthStencilSurface)(IDirect3DDevice9*, IDirect3DSurface9**);
    HRESULT (WINAPI *BeginScene)(IDirect3DDevice9*);
    HRESULT (WINAPI *EndScene)(IDirect3DDevice9*);
    HRESULT (WINAPI *Clear)(IDirect3DDevice9*, DWORD, const D3DRECT*, DWORD, D3DCOLOR, FLOAT, DWORD);
    HRESULT (WINAPI *SetTransform)(IDirect3DDevice9*, D3DTRANSFORMSTATETYPE, const D3DMATRIX*);
    HRESULT (WINAPI *GetTransform)(IDirect3DDevice9*, D3DTRANSFORMSTATETYPE, D3DMATRIX*);
    HRESULT (WINAPI *MultiplyTransform)(IDirect3DDevice9*, D3DTRANSFORMSTATETYPE, const D3DMATRIX*);
    HRESULT (WINAPI *SetViewport)(IDirect3DDevice9*, const D3DVIEWPORT9*);
    HRESULT (WINAPI *GetViewport)(IDirect3DDevice9*, D3DVIEWPORT9*);
    HRESULT (WINAPI *SetMaterial)(IDirect3DDevice9*, const D3DMATERIAL9*);
    HRESULT (WINAPI *GetMaterial)(IDirect3DDevice9*, D3DMATERIAL9*);
    HRESULT (WINAPI *SetLight)(IDirect3DDevice9*, DWORD, const D3DLIGHT9*);
    HRESULT (WINAPI *GetLight)(IDirect3DDevice9*, DWORD, D3DLIGHT9*);
    HRESULT (WINAPI *LightEnable)(IDirect3DDevice9*, DWORD, BOOL);
    HRESULT (WINAPI *GetLightEnable)(IDirect3DDevice9*, DWORD, BOOL*);
    HRESULT (WINAPI *SetClipPlane)(IDirect3DDevice9*, DWORD, const FLOAT*);
    HRESULT (WINAPI *GetClipPlane)(IDirect3DDevice9*, DWORD, FLOAT*);
    HRESULT (WINAPI *SetRenderState)(IDirect3DDevice9*, D3DRENDERSTATETYPE, DWORD);
    HRESULT (WINAPI *GetRenderState)(IDirect3DDevice9*, D3DRENDERSTATETYPE, DWORD*);
    HRESULT (WINAPI *CreateStateBlock)(IDirect3DDevice9*, D3DSTATEBLOCKTYPE, IDirect3DStateBlock9**);
    HRESULT (WINAPI *GetStateBlock)(IDirect3DDevice9*, DWORD, IDirect3DStateBlock9**);
    HRESULT (WINAPI *BeginStateBlock)(IDirect3DDevice9*);
    HRESULT (WINAPI *EndStateBlock)(IDirect3DDevice9*, DWORD*);
    HRESULT (WINAPI *SetClipStatus)(IDirect3DDevice9*, const D3DCLIPSTATUS9*);
    HRESULT (WINAPI *GetClipStatus)(IDirect3DDevice9*, D3DCLIPSTATUS9*);
    HRESULT (WINAPI *GetTexture)(IDirect3DDevice9*, DWORD, IDirect3DBaseTexture9**);
    HRESULT (WINAPI *SetTexture)(IDirect3DDevice9*, DWORD, IDirect3DBaseTexture9*);
    HRESULT (WINAPI *GetSamplerState)(IDirect3DDevice9*, DWORD, D3DSAMPLERSTATETYPE, DWORD*);
    HRESULT (WINAPI *SetSamplerState)(IDirect3DDevice9*, DWORD, D3DSAMPLERSTATETYPE, DWORD);
    HRESULT (WINAPI *ValidateDevice)(IDirect3DDevice9*, DWORD*);
    HRESULT (WINAPI *GetInfo)(IDirect3DDevice9*, DWORD, VOID*, DWORD);
    HRESULT (WINAPI *SetPreferredOffset)(IDirect3DDevice9*, UINT);
    HRESULT (WINAPI *SetResourcePriority)(IDirect3DDevice9*, IDirect3DResource9*, DWORD);
    HRESULT (WINAPI *PreLoad)(IDirect3DDevice9*, IDirect3DResource9*);
    HRESULT (WINAPI *SetCurrentTexturePalette)(IDirect3DDevice9*, UINT);
    HRESULT (WINAPI *GetCurrentTexturePalette)(IDirect3DDevice9*, UINT*);
    HRESULT (WINAPI *SetScissorRect)(IDirect3DDevice9*, const D3DRECT*);
    HRESULT (WINAPI *GetScissorRect)(IDirect3DDevice9*, D3DRECT*);
    HRESULT (WINAPI *SetSoftwareVertexProcessing)(IDirect3DDevice9*, BOOL);
    BOOL (WINAPI *GetSoftwareVertexProcessing)(IDirect3DDevice9*);
    HRESULT (WINAPI *SetNPatchMode)(IDirect3DDevice9*, FLOAT);
    FLOAT (WINAPI *GetNPatchMode)(IDirect3DDevice9*);
    HRESULT (WINAPI *DrawPrimitive)(IDirect3DDevice9*, D3DPRIMITIVETYPE, UINT, UINT);
    HRESULT (WINAPI *DrawIndexedPrimitive)(IDirect3DDevice9*, D3DPRIMITIVETYPE, INT, UINT, UINT, UINT, UINT);
    HRESULT (WINAPI *DrawPrimitiveUP)(IDirect3DDevice9*, D3DPTRIMITIVETYPE, UINT, const VOID*, UINT, const D3DVERTEXELEMENT9*, UINT);
    HRESULT (WINAPI *DrawIndexedPrimitiveUP)(IDirect3DDevice9*, D3DPRIMITIVETYPE, UINT, UINT, const VOID*, UINT, D3DFORMAT, const VOID*, UINT);
    HRESULT (WINAPI *ProcessVertices)(IDirect3DDevice9*, UINT, UINT, UINT, IDirect3DVertexBuffer9*, DWORD);
    HRESULT (WINAPI *CreateVertexDeclaration)(IDirect3DDevice9*, const D3DVERTEXELEMENT9*, IDirect3DVertexDeclaration9**);
    HRESULT (WINAPI *SetVertexDeclaration)(IDirect3DDevice9*, IDirect3DVertexDeclaration9*);
    HRESULT (WINAPI *GetVertexDeclaration)(IDirect3DDevice9*, IDirect3DVertexDeclaration9**);
    HRESULT (WINAPI *SetFVF)(IDirect3DDevice9*, DWORD);
    HRESULT (WINAPI *GetFVF)(IDirect3DDevice9*, DWORD*);
    HRESULT (WINAPI *CreateVertexShader)(IDirect3DDevice9*, const DWORD*, IDirect3DVertexShader9**);
    HRESULT (WINAPI *SetVertexShader)(IDirect3DDevice9*, IDirect3DVertexShader9*);
    HRESULT (WINAPI *GetVertexShader)(IDirect3DDevice9*, IDirect3DVertexShader9**);
    HRESULT (WINAPI *SetVertexShaderConstantF)(IDirect3DDevice9*, UINT, const FLOAT*, UINT);
    HRESULT (WINAPI *GetVertexShaderConstantF)(IDirect3DDevice9*, UINT, FLOAT*, UINT);
    HRESULT (WINAPI *SetVertexShaderConstantI)(IDirect3DDevice9*, UINT, const INT*, UINT);
    HRESULT (WINAPI *GetVertexShaderConstantI)(IDirect3DDevice9*, UINT, INT*, UINT);
    HRESULT (WINAPI *SetVertexShaderConstantB)(IDirect3DDevice9*, UINT, const BOOL*, UINT);
    HRESULT (WINAPI *GetVertexShaderConstantB)(IDirect3DDevice9*, UINT, BOOL*, UINT);
    HRESULT (WINAPI *SetStreamSourceFreq)(IDirect3DDevice9*, UINT, UINT);
    HRESULT (WINAPI *SetStreamSource)(IDirect3DDevice9*, UINT, IDirect3DVertexBuffer9*, UINT, UINT);
    HRESULT (WINAPI *GetStreamSource)(IDirect3DDevice9*, UINT, IDirect3DVertexBuffer9**, UINT*, UINT*);
    HRESULT (WINAPI *SetIndices)(IDirect3DDevice9*, IDirect3DIndexBuffer9*);
    HRESULT (WINAPI *GetIndices)(IDirect3DDevice9*, IDirect3DIndexBuffer9**);
    HRESULT (WINAPI *CreatePixelShader)(IDirect3DDevice9*, const DWORD*, IDirect3DPixelShader9**);
    HRESULT (WINAPI *SetPixelShader)(IDirect3DDevice9*, IDirect3DPixelShader9*);
    HRESULT (WINAPI *GetPixelShader)(IDirect3DDevice9*, IDirect3DPixelShader9**);
    HRESULT (WINAPI *SetPixelShaderConstantF)(IDirect3DDevice9*, UINT, const FLOAT*, UINT);
    HRESULT (WINAPI *GetPixelShaderConstantF)(IDirect3DDevice9*, UINT, FLOAT*, UINT);
    HRESULT (WINAPI *SetPixelShaderConstantI)(IDirect3DDevice9*, UINT, const INT*, UINT);
    HRESULT (WINAPI *GetPixelShaderConstantI)(IDirect3DDevice9*, UINT, INT*, UINT);
    HRESULT (WINAPI *SetPixelShaderConstantB)(IDirect3DDevice9*, UINT, const BOOL*, UINT);
    HRESULT (WINAPI *GetPixelShaderConstantB)(IDirect3DDevice9*, UINT, BOOL*, UINT);
    HRESULT (WINAPI *DrawRectPatch)(IDirect3DDevice9*, UINT, const D3DRECTPATCH_INFO*, const D3DRECTPATCH_INFO*);
    HRESULT (WINAPI *DrawTriPatch)(IDirect3DDevice9*, UINT, const D3DTRIPATCH_INFO*, const D3DTRIPATCH_INFO*);
    HRESULT (WINAPI *DeletePatch)(IDirect3DDevice9*, UINT);
    HRESULT (WINAPI *CreateQuery)(IDirect3DDevice9*, D3DQUERYTYPE, IDirect3DQuery9**);
    HRESULT (WINAPI *Present)(IDirect3DDevice9*, const D3DRECT*, const D3DRECT*, HWND, RGNDATA*);
};

struct IDirect3DDevice9 {
    const IDirect3DDevice9Vtbl* lpVtbl;
};

// Exported entry point
extern "C" __attribute__((ms_abi)) IDirect3D9* WINAPI Direct3DCreate9(UINT SDKVersion);
