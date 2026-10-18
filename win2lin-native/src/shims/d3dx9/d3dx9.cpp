// libwin_d3dx9.so - stub shim for d3dx9_*.dx (DirectX helper library).
// Games from 2000-2010 link against d3dx9_24..d3dx9_43. We export the most
// commonly used entry points as stubs so the PE loads.

#include "common/logging.hpp"

#include <cstdint>
#include <cstring>

using HRESULT = int32_t;
using DWORD = uint32_t;
using UINT = uint32_t;
using FLOAT = float;
using INT = int32_t;
using BOOL = int32_t;
using HANDLE = void*;
using HWND = HANDLE;
using LPCVOID = const void*;
using LPVOID = void*;
using LPCSTR = const char*;
using LPCWSTR = const wchar_t*;
using LPSTR = char*;
using LPWSTR = wchar_t*;
using HINSTANCE = HANDLE;
using HMONITOR = HANDLE;
using WORD = uint16_t;
using BYTE = uint8_t;
using D3DFORMAT = uint32_t;
using D3DPOOL = uint32_t;
using D3DCOLOR = uint32_t;
using D3DXIMAGE_FILEFORMAT = uint32_t;

struct GUID {
    DWORD Data1;
    WORD Data2;
    WORD Data3;
    BYTE Data4[8];
};
using REFCLSID = const GUID*;
using REFIID = const GUID*;
using REFGUID = const GUID*;

constexpr HRESULT D3D_OK = 0;
constexpr HRESULT D3DERR_INVALIDCALL = -2005530518;
constexpr HRESULT E_NOINTERFACE = -2147467263;
constexpr HRESULT S_OK = 0;
constexpr int TRUE = 1;
constexpr int FALSE = 0;

namespace win2lin {

#define WINAPI __attribute__((ms_abi))

// D3DXCreateTextureFromFileEx / D3DXCreateTextureFromFile (stubs)
extern "C" WINAPI HRESULT D3DXCreateTextureFromFileA(void* pDevice, LPCSTR pSrcFile, void** ppTexture) {
    (void)pDevice; (void)pSrcFile; (void)ppTexture;
    return D3D_OK;
}
extern "C" WINAPI HRESULT D3DXCreateTextureFromFileW(void* pDevice, LPCWSTR pSrcFile, void** ppTexture) {
    (void)pDevice; (void)pSrcFile; (void)ppTexture;
    return D3D_OK;
}
extern "C" WINAPI HRESULT D3DXCreateTextureFromFileInMemory(void* pDevice, LPCVOID pSrcData, UINT SrcDataSize, void** ppTexture) {
    (void)pDevice; (void)pSrcData; (void)SrcDataSize; (void)ppTexture;
    return D3D_OK;
}
extern "C" WINAPI HRESULT D3DXCreateTextureFromFileInMemoryEx(void* pDevice, LPCVOID pSrcData, UINT SrcDataSize, UINT Width, UINT Height, UINT MipLevels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, DWORD Filter, DWORD MipFilter, D3DCOLOR ColorKey, void* pSrcInfo, void* pPalette, void** ppTexture) {
    (void)pDevice; (void)pSrcData; (void)SrcDataSize; (void)Width; (void)Height; (void)MipLevels; (void)Usage; (void)Format; (void)Pool; (void)Filter; (void)MipFilter; (void)ColorKey; (void)pSrcInfo; (void)pPalette; (void)ppTexture;
    return D3D_OK;
}
extern "C" WINAPI HRESULT D3DXCreateTexture(void* pDevice, UINT Width, UINT Height, UINT MipLevels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, void** ppTexture) {
    (void)pDevice; (void)Width; (void)Height; (void)MipLevels; (void)Usage; (void)Format; (void)Pool; (void)ppTexture;
    return D3D_OK;
}

// D3DXCreateSprite
extern "C" WINAPI HRESULT D3DXCreateSprite(void* pDevice, void** ppSprite) {
    (void)pDevice; (void)ppSprite;
    return D3D_OK;
}

// D3DXLoadSurfaceFromSurface / D3DXLoadSurfaceFromMemory (stubs)
extern "C" WINAPI HRESULT D3DXLoadSurfaceFromSurface(void* pDestSurface, const void* pDestPalette, const void* pDestRect, void* pSrcSurface, const void* pSrcPalette, const void* pSrcRect, DWORD Filter, D3DCOLOR ColorKey) {
    (void)pDestSurface; (void)pDestPalette; (void)pDestRect; (void)pSrcSurface; (void)pSrcPalette; (void)pSrcRect; (void)Filter; (void)ColorKey;
    return D3D_OK;
}
extern "C" WINAPI HRESULT D3DXLoadSurfaceFromMemory(void* pDestSurface, const void* pDestPalette, const void* pDestRect, LPCVOID pSrcMemory, D3DFORMAT SrcFormat, UINT SrcPitch, const void* pSrcPalette, const void* pSrcRect, DWORD Filter, D3DCOLOR ColorKey) {
    (void)pDestSurface; (void)pDestPalette; (void)pDestRect; (void)pSrcMemory; (void)SrcFormat; (void)SrcPitch; (void)pSrcPalette; (void)pSrcRect; (void)Filter; (void)ColorKey;
    return D3D_OK;
}
extern "C" WINAPI HRESULT D3DXLoadSurfaceFromFile(void* pDestSurface, const void* pDestPalette, const void* pDestRect, LPCSTR pSrcFile, const void* pSrcPalette, const void* pSrcRect, DWORD Filter, D3DCOLOR ColorKey) {
    (void)pDestSurface; (void)pDestPalette; (void)pDestRect; (void)pSrcFile; (void)pSrcPalette; (void)pSrcRect; (void)Filter; (void)ColorKey;
    return D3D_OK;
}

// D3DXCreateFont / D3DXCreateFontIndirect
extern "C" WINAPI HRESULT D3DXCreateFontA(void* pDevice, INT Height, UINT Width, UINT Weight, UINT MipLevels, BOOL Italic, DWORD CharSet, DWORD OutputPrecision, DWORD Quality, DWORD PitchAndFamily, LPCSTR pFaceName, void** ppFont) {
    (void)pDevice; (void)Height; (void)Width; (void)Weight; (void)MipLevels; (void)Italic; (void)CharSet; (void)OutputPrecision; (void)Quality; (void)PitchAndFamily; (void)pFaceName; (void)ppFont;
    return D3D_OK;
}
extern "C" WINAPI HRESULT D3DXCreateFontW(void* pDevice, INT Height, UINT Width, UINT Weight, UINT MipLevels, BOOL Italic, DWORD CharSet, DWORD OutputPrecision, DWORD Quality, DWORD PitchAndFamily, LPCWSTR pFaceName, void** ppFont) {
    (void)pDevice; (void)Height; (void)Width; (void)Weight; (void)MipLevels; (void)Italic; (void)CharSet; (void)OutputPrecision; (void)Quality; (void)PitchAndFamily; (void)pFaceName; (void)ppFont;
    return D3D_OK;
}
extern "C" WINAPI HRESULT D3DXCreateFontIndirectA(void* pDevice, const void* pDesc, void** ppFont) {
    (void)pDevice; (void)pDesc; (void)ppFont;
    return D3D_OK;
}
extern "C" WINAPI HRESULT D3DXCreateFontIndirectW(void* pDevice, const void* pDesc, void** ppFont) {
    (void)pDevice; (void)pDesc; (void)ppFont;
    return D3D_OK;
}

// D3DXCreateMesh / D3DXCreateMeshFVF
extern "C" WINAPI HRESULT D3DXCreateMeshFVF(DWORD NumFaces, DWORD NumVertices, DWORD Options, DWORD FVF, void* pDevice, void** ppMesh) {
    (void)NumFaces; (void)NumVertices; (void)Options; (void)FVF; (void)pDevice; (void)ppMesh;
    return D3D_OK;
}
extern "C" WINAPI HRESULT D3DXCreateMesh(DWORD NumFaces, DWORD NumVertices, DWORD Options, const void* pDecl, void* pDevice, void** ppMesh) {
    (void)NumFaces; (void)NumVertices; (void)Options; (void)pDecl; (void)pDevice; (void)ppMesh;
    return D3D_OK;
}

// D3DXComputeNormals / D3DXIntersect
extern "C" WINAPI HRESULT D3DXComputeNormals(void* pMesh, void* pAdjacency, DWORD Flags) {
    (void)pMesh; (void)pAdjacency; (void)Flags;
    return D3D_OK;
}
extern "C" WINAPI HRESULT D3DXIntersect(void* pMesh, const void* pRayPos, const void* pRayDir, BOOL* pHit, DWORD* pFaceIndex, FLOAT* pU, FLOAT* pV, FLOAT* pDist, void** ppAllHits, DWORD* pCount) {
    (void)pMesh; (void)pRayPos; (void)pRayDir; (void)pHit; (void)pFaceIndex; (void)pU; (void)pV; (void)pDist; (void)ppAllHits; (void)pCount;
    return D3D_OK;
}

// D3DXGetImageInfoFromFile / D3DXGetImageInfoFromFileInMemory
extern "C" WINAPI HRESULT D3DXGetImageInfoFromFileA(LPCSTR pFilename, void* pImgInfo) {
    (void)pFilename; (void)pImgInfo;
    return D3D_OK;
}
extern "C" WINAPI HRESULT D3DXGetImageInfoFromFileW(LPCWSTR pFilename, void* pImgInfo) {
    (void)pFilename; (void)pImgInfo;
    return D3D_OK;
}
extern "C" WINAPI HRESULT D3DXGetImageInfoFromFileInMemory(LPCVOID pSrcData, UINT SrcDataSize, void* pImgInfo) {
    (void)pSrcData; (void)SrcDataSize; (void)pImgInfo;
    return D3D_OK;
}

// D3DXSaveSurfaceToFile / D3DXSaveTextureToFile
extern "C" WINAPI HRESULT D3DXSaveSurfaceToFileA(LPCSTR pDestFile, D3DXIMAGE_FILEFORMAT DestFormat, void* pSrcSurface, const void* pSrcRect, const void* pSrcRect2) {
    (void)pDestFile; (void)DestFormat; (void)pSrcSurface; (void)pSrcRect; (void)pSrcRect2;
    return D3D_OK;
}
extern "C" WINAPI HRESULT D3DXSaveSurfaceToFileW(LPCWSTR pDestFile, D3DXIMAGE_FILEFORMAT DestFormat, void* pSrcSurface, const void* pSrcRect, const void* pSrcRect2) {
    (void)pDestFile; (void)DestFormat; (void)pSrcSurface; (void)pSrcRect; (void)pSrcRect2;
    return D3D_OK;
}
extern "C" WINAPI HRESULT D3DXSaveTextureToFileA(LPCSTR pDestFile, D3DXIMAGE_FILEFORMAT DestFormat, void* pSrcTexture, const void* pSrcRect) {
    (void)pDestFile; (void)DestFormat; (void)pSrcTexture; (void)pSrcRect;
    return D3D_OK;
}
extern "C" WINAPI HRESULT D3DXSaveTextureToFileW(LPCWSTR pDestFile, D3DXIMAGE_FILEFORMAT DestFormat, void* pSrcTexture, const void* pSrcRect) {
    (void)pDestFile; (void)DestFormat; (void)pSrcTexture; (void)pSrcRect;
    return D3D_OK;
}

// D3DXCreateEffect / D3DXCreateEffectFromFile
extern "C" WINAPI HRESULT D3DXCreateEffect(void* pDevice, LPCVOID pSrcData, UINT SrcDataSize, void* pDefines, void* pInclude, DWORD Flags, void* pPool, void** ppEffect, void** ppErrors) {
    (void)pDevice; (void)pSrcData; (void)SrcDataSize; (void)pDefines; (void)pInclude; (void)Flags; (void)pPool; (void)ppEffect; (void)ppErrors;
    return D3D_OK;
}
extern "C" WINAPI HRESULT D3DXCreateEffectFromFileA(void* pDevice, LPCSTR pSrcFile, void* pDefines, void* pInclude, DWORD Flags, void* pPool, void** ppEffect, void** ppErrors) {
    (void)pDevice; (void)pSrcFile; (void)pDefines; (void)pInclude; (void)Flags; (void)pPool; (void)ppEffect; (void)ppErrors;
    return D3D_OK;
}
extern "C" WINAPI HRESULT D3DXCreateEffectFromFileW(void* pDevice, LPCWSTR pSrcFile, void* pDefines, void* pInclude, DWORD Flags, void* pPool, void** ppEffect, void** ppErrors) {
    (void)pDevice; (void)pSrcFile; (void)pDefines; (void)pInclude; (void)Flags; (void)pPool; (void)ppEffect; (void)ppErrors;
    return D3D_OK;
}

// D3DXCreateMatrix / D3DXMatrixIdentity / etc
extern "C" WINAPI void* D3DXMatrixIdentity(void* pOut) { return pOut; }
extern "C" WINAPI void* D3DXMatrixMultiply(void* pOut, const void* pM1, const void* pM2) { return pOut; }
extern "C" WINAPI void* D3DXMatrixTranslation(void* pOut, FLOAT x, FLOAT y, FLOAT z) { return pOut; }
extern "C" WINAPI void* D3DXMatrixScaling(void* pOut, FLOAT sx, FLOAT sy, FLOAT sz) { return pOut; }
extern "C" WINAPI void* D3DXMatrixRotationX(void* pOut, FLOAT angle) { return pOut; }
extern "C" WINAPI void* D3DXMatrixRotationY(void* pOut, FLOAT angle) { return pOut; }
extern "C" WINAPI void* D3DXMatrixRotationZ(void* pOut, FLOAT angle) { return pOut; }
extern "C" WINAPI void* D3DXMatrixRotationYawPitchRoll(void* pOut, FLOAT yaw, FLOAT pitch, FLOAT roll) { return pOut; }
extern "C" WINAPI void* D3DXMatrixLookAtLH(void* pOut, const void* pEye, const void* pAt, const void* pUp) { return pOut; }
extern "C" WINAPI void* D3DXMatrixPerspectiveFovLH(void* pOut, FLOAT fovy, FLOAT aspect, FLOAT zn, FLOAT zf) { return pOut; }
extern "C" WINAPI void* D3DXMatrixPerspectiveRH(void* pOut, FLOAT w, FLOAT h, FLOAT zn, FLOAT zf) { return pOut; }
extern "C" WINAPI void* D3DXMatrixOrthoLH(void* pOut, FLOAT w, FLOAT h, FLOAT zn, FLOAT zf) { return pOut; }
extern "C" WINAPI void* D3DXMatrixOrthoRH(void* pOut, FLOAT w, FLOAT h, FLOAT zn, FLOAT zf) { return pOut; }

// D3DXVec3 / D3DXVec4 helpers
extern "C" WINAPI void* D3DXVec3TransformCoord(void* pOut, const void* pV, const void* pM) { return pOut; }
extern "C" WINAPI void* D3DXVec3TransformNormal(void* pOut, const void* pV, const void* pM) { return pOut; }
extern "C" WINAPI void* D3DXVec3Cross(void* pOut, const void* pV1, const void* pV2) { return pOut; }
extern "C" WINAPI FLOAT D3DXVec3Dot(const void* pV1, const void* pV2) { return 0.0f; }
extern "C" WINAPI FLOAT D3DXVec3Length(const void* pV) { return 0.0f; }
extern "C" WINAPI void* D3DXVec3Normalize(void* pOut, const void* pV) { return pOut; }
extern "C" WINAPI void* D3DXVec3Lerp(void* pOut, const void* pV1, const void* pV2, FLOAT s) { return pOut; }

// D3DXColor
extern "C" WINAPI DWORD D3DXColorAdjustSaturation(D3DCOLOR Color, FLOAT s) { return Color; }
extern "C" WINAPI void D3DXColorAdjustContrast(D3DCOLOR* pOut, D3DCOLOR Color, FLOAT contrast) { (void)pOut; (void)Color; (void)contrast; }

// D3DXGetDriverLevel / D3DXGetDriverVersion
extern "C" WINAPI UINT D3DXGetDriverLevel(void* pDevice) { return 0; }
extern "C" WINAPI HRESULT D3DXGetDriverVersion(void* pDevice, void* pVersion) { return D3D_OK; }

// D3DXCheckVersion / D3DXGetVersionString
extern "C" WINAPI BOOL D3DXCheckVersion(DWORD D3D_SDK_VERSION, DWORD D3DX_SDK_VERSION) { return TRUE; }
extern "C" WINAPI LPCSTR D3DXGetVersionString(void) { return "win2lin-d3dx9-stub"; }

// D3DXRegisterEffect / D3DXGetTechniqueFromHandle / etc (stubs for effect framework)
extern "C" WINAPI HRESULT D3DXRegisterEffect(void* pEffect, void* pPools, UINT NumPools) { return D3D_OK; }
extern "C" WINAPI HRESULT D3DXUnregisterEffect(void* pEffect) { return D3D_OK; }
extern "C" WINAPI void* D3DXGetTechniqueFromHandle(void* hTechnique) { return hTechnique; }
extern "C" WINAPI void* D3DXGetAnnotationFromHandle(void* hAnnotation) { return hAnnotation; }
extern "C" WINAPI void* D3DXGetPassFromHandle(void* hPass) { return hPass; }

// D3DXFindValidTechnique / D3DXFindNextValidTechnique
extern "C" WINAPI void* D3DXFindValidTechnique(void* pEffect, void* pTechnique) { return pTechnique; }
extern "C" WINAPI void* D3DXFindNextValidTechnique(void* pEffect, void* pTechnique) { return pTechnique; }

// D3DXCreateBuffer / D3DXCreateString / D3DXCreateTokenizedProgram
extern "C" WINAPI HRESULT D3DXCreateBuffer(UINT SizeBytes, void** ppBuffer) { return D3D_OK; }
extern "C" WINAPI HRESULT D3DXCreateString(LPCSTR pString, void** ppBuffer) { return D3D_OK; }

// D3DXCreateEffectPool / D3DXCreateEffectFromResource / etc
extern "C" WINAPI HRESULT D3DXCreateEffectPool(void* pDevice, DWORD Flags, void* pPool) { return D3D_OK; }
extern "C" WINAPI HRESULT D3DXCreateEffectFromResourceA(void* pDevice, void* hSrcModule, LPCSTR pSrcResource, void* pDefines, void* pInclude, DWORD Flags, void* pPool, void** ppEffect, void** ppErrors) { return D3D_OK; }
extern "C" WINAPI HRESULT D3DXCreateEffectFromResourceW(void* pDevice, void* hSrcModule, LPCWSTR pSrcResource, void* pDefines, void* pInclude, DWORD Flags, void* pPool, void** ppEffect, void** ppErrors) { return D3D_OK; }

// Missing ordinals / misc
extern "C" WINAPI HRESULT D3DXLoadMeshFromXA(LPCSTR pFilename, DWORD Options, void* pDevice, void** ppAdjacency, void** ppMaterials, void** ppEffectInstances, DWORD* pNumMaterials, void** ppMesh) { return D3D_OK; }
extern "C" WINAPI HRESULT D3DXLoadMeshFromXW(LPCWSTR pFilename, DWORD Options, void* pDevice, void** ppAdjacency, void** ppMaterials, void** ppEffectInstances, DWORD* pNumMaterials, void** ppMesh) { return D3D_OK; }
extern "C" WINAPI HRESULT D3DXLoadMeshFromXInMemory(LPCVOID pMemory, DWORD SizeofMemory, DWORD Options, void* pDevice, void** ppAdjacency, void** ppMaterials, void** ppEffectInstances, DWORD* pNumMaterials, void** ppMesh) { return D3D_OK; }
extern "C" WINAPI HRESULT D3DXLoadMeshFromXInMemoryEx(LPCVOID pMemory, DWORD SizeofMemory, DWORD Options, D3DPOOL Pool, void* pDevice, void** ppAdjacency, void** ppMaterials, void** ppEffectInstances, DWORD* pNumMaterials, void** ppMesh) { return D3D_OK; }
extern "C" WINAPI HRESULT D3DXLoadMeshFromXof(void* pXOfFile, DWORD Options, void* pDevice, void** ppAdjacency, void** ppMaterials, void** ppEffectInstances, DWORD* pNumMaterials, void** ppMesh) { return D3D_OK; }
extern "C" WINAPI HRESULT D3DXLoadMeshFromXofInMemory(LPCVOID pMemory, DWORD SizeofMemory, DWORD Options, void* pDevice, void** ppAdjacency, void** ppMaterials, void** ppEffectInstances, DWORD* pNumMaterials, void** ppMesh) { return D3D_OK; }
extern "C" WINAPI HRESULT D3DXLoadMeshFromXofInMemoryEx(LPCVOID pMemory, DWORD SizeofMemory, DWORD Options, D3DPOOL Pool, void* pDevice, void** ppAdjacency, void** ppMaterials, void** ppEffectInstances, DWORD* pNumMaterials, void** ppMesh) { return D3D_OK; }

// D3DXDeclaratorFromFVF / D3DXGetFVFVertexSize
extern "C" WINAPI DWORD D3DXGetFVFVertexSize(DWORD FVF) { return 0; }
extern "C" WINAPI HRESULT D3DXDeclaratorFromFVF(DWORD FVF, void* pDecl) { return D3D_OK; }

// D3DXConvertToIndexedBlendedMesh / etc
extern "C" WINAPI HRESULT D3DXConvertToIndexedBlendedMesh(void* pMesh, DWORD Options, DWORD MaxFaceGroups, DWORD MaxBones, void* pAdjacencyIn, void* pAdjacencyOut, void* ppBoneCombinationTable, void** ppMesh) { return D3D_OK; }
extern "C" WINAPI HRESULT D3DXCreateNPatchMesh(void* pMesh, void* pAdjacency, DWORD Options, void** ppNPatchMesh) { return D3D_OK; }

// D3DXSplitMesh / D3DXOptimizeMesh / D3DXOptimizeVertices / D3DXOptimizeIndices / D3DXOrderVertexGroups / D3DXGeneratePMesh / D3DXCreatePMesh / D3DXCreateSPMesh / D3DXCreateSkinMesh
extern "C" WINAPI HRESULT D3DXSplitMesh(void* pMeshIn, DWORD* pFaceRemap, void** ppMeshOut) { return D3D_OK; }
extern "C" WINAPI HRESULT D3DXOptimizeMesh(void* pMesh, DWORD Flags, void* pAdjacencyOut, void* pAdjacencyIn, void** ppMeshOpt) { return D3D_OK; }
extern "C" WINAPI HRESULT D3DXOptimizeVertices(void* pMesh, DWORD* pVertexRemap, void** ppMeshOpt) { return D3D_OK; }
extern "C" WINAPI HRESULT D3DXOptimizeIndices(void* pMesh, DWORD* pIndexRemap, void** ppMeshOpt) { return D3D_OK; }
extern "C" WINAPI HRESULT D3DXOrderVertexGroups(void* pMesh, DWORD* pGroupRemap, void** ppMeshOpt) { return D3D_OK; }
extern "C" WINAPI HRESULT D3DXGeneratePMesh(void* pMesh, void* pAdjacency, DWORD MaxVertexIndex, DWORD MinVertexIndex, DWORD Options, void** ppMesh) { return D3D_OK; }
extern "C" WINAPI HRESULT D3DXCreatePMesh(void* pMesh, void* pAdjacency, DWORD MaxVertexIndex, DWORD MinVertexIndex, DWORD Options, void** ppMesh) { return D3D_OK; }
extern "C" WINAPI HRESULT D3DXCreateSPMesh(void* pMesh, void* pAdjacency, DWORD Options, void** ppMesh) { return D3D_OK; }
extern "C" WINAPI HRESULT D3DXCreateSkinMesh(DWORD NumVertices, DWORD NumFaces, DWORD Options, DWORD MaxVertexInfluences, DWORD NumBoneInfluences, void* pDevice, void** ppMesh) { return D3D_OK; }
extern "C" WINAPI HRESULT D3DXCreateSkinMeshFromXof(void* pXOfFile, DWORD Options, void* pDevice, void** ppAdjacency, void** ppMaterials, void** ppEffectInstances, DWORD* pNumMaterials, void** ppMesh) { return D3D_OK; }
extern "C" WINAPI HRESULT D3DXCreateSkinMeshFromFileA(LPCSTR pFilename, DWORD Options, void* pDevice, void** ppAdjacency, void** ppMaterials, void** ppEffectInstances, DWORD* pNumMaterials, void** ppMesh) { return D3D_OK; }
extern "C" WINAPI HRESULT D3DXCreateSkinMeshFromFileW(LPCWSTR pFilename, DWORD Options, void* pDevice, void** ppAdjacency, void** ppMaterials, void** ppEffectInstances, DWORD* pNumMaterials, void** ppMesh) { return D3D_OK; }

// D3DXComputeBoundingBox / D3DXComputeBoundingSphere
extern "C" WINAPI HRESULT D3DXComputeBoundingBox(const void* pFirstPosition, DWORD NumVertices, DWORD dwPositionStride, void* pMin, void* pMax) { return D3D_OK; }
extern "C" WINAPI HRESULT D3DXComputeBoundingSphere(const void* pFirstPosition, DWORD NumVertices, DWORD dwPositionStride, void* pCenter, FLOAT* pRadius) { return D3D_OK; }

// D3DXCreateVolumeTexture / D3DXCreateCubeTexture
extern "C" WINAPI HRESULT D3DXCreateVolumeTexture(void* pDevice, UINT Width, UINT Height, UINT Depth, UINT MipLevels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, void** ppVolumeTexture) { return D3D_OK; }
extern "C" WINAPI HRESULT D3DXCreateCubeTexture(void* pDevice, UINT EdgeLength, UINT MipLevels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, void** ppCubeTexture) { return D3D_OK; }

// D3DXCreateVolumeTextureFromFile / D3DXCreateCubeTextureFromFile
extern "C" WINAPI HRESULT D3DXCreateVolumeTextureFromFileA(void* pDevice, LPCSTR pSrcFile, void** ppVolumeTexture) { return D3D_OK; }
extern "C" WINAPI HRESULT D3DXCreateVolumeTextureFromFileW(void* pDevice, LPCWSTR pSrcFile, void** ppVolumeTexture) { return D3D_OK; }
extern "C" WINAPI HRESULT D3DXCreateCubeTextureFromFileA(void* pDevice, LPCSTR pSrcFile, void** ppCubeTexture) { return D3D_OK; }
extern "C" WINAPI HRESULT D3DXCreateCubeTextureFromFileW(void* pDevice, LPCWSTR pSrcFile, void** ppCubeTexture) { return D3D_OK; }

// D3DXCreateVolumeTextureFromFileInMemory / D3DXCreateCubeTextureFromFileInMemory
extern "C" WINAPI HRESULT D3DXCreateVolumeTextureFromFileInMemory(void* pDevice, LPCVOID pSrcData, UINT SrcDataSize, void** ppVolumeTexture) { return D3D_OK; }
extern "C" WINAPI HRESULT D3DXCreateCubeTextureFromFileInMemory(void* pDevice, LPCVOID pSrcData, UINT SrcDataSize, void** ppCubeTexture) { return D3D_OK; }

// D3DXSaveVolumeTextureToFile / D3DXSaveCubeTextureToFile
extern "C" WINAPI HRESULT D3DXSaveVolumeTextureToFileA(LPCSTR pDestFile, D3DXIMAGE_FILEFORMAT DestFormat, void* pSrcVolumeTexture, const void* pSrcBox) { return D3D_OK; }
extern "C" WINAPI HRESULT D3DXSaveVolumeTextureToFileW(LPCWSTR pDestFile, D3DXIMAGE_FILEFORMAT DestFormat, void* pSrcVolumeTexture, const void* pSrcBox) { return D3D_OK; }
extern "C" WINAPI HRESULT D3DXSaveCubeTextureToFileA(LPCSTR pDestFile, D3DXIMAGE_FILEFORMAT DestFormat, void* pSrcCubeTexture, const void* pSrcFaceRects) { return D3D_OK; }
extern "C" WINAPI HRESULT D3DXSaveCubeTextureToFileW(LPCWSTR pDestFile, D3DXIMAGE_FILEFORMAT DestFormat, void* pSrcCubeTexture, const void* pSrcFaceRects) { return D3D_OK; }

// D3DXCreateVolumeTextureFromResource / D3DXCreateCubeTextureFromResource
extern "C" WINAPI HRESULT D3DXCreateVolumeTextureFromResourceA(void* pDevice, void* hSrcModule, LPCSTR pSrcResource, void** ppVolumeTexture) { return D3D_OK; }
extern "C" WINAPI HRESULT D3DXCreateVolumeTextureFromResourceW(void* pDevice, void* hSrcModule, LPCWSTR pSrcResource, void** ppVolumeTexture) { return D3D_OK; }
extern "C" WINAPI HRESULT D3DXCreateCubeTextureFromResourceA(void* pDevice, void* hSrcModule, LPCSTR pSrcResource, void** ppCubeTexture) { return D3D_OK; }
extern "C" WINAPI HRESULT D3DXCreateCubeTextureFromResourceW(void* pDevice, void* hSrcModule, LPCWSTR pSrcResource, void** ppCubeTexture) { return D3D_OK; }

// D3DXCheckTextureRequirements / D3DXSaveTextureToFileInMemory
extern "C" WINAPI HRESULT D3DXCheckTextureRequirements(void* pDevice, void** ppTexture, DWORD* pUsage, D3DPOOL* pPool, D3DFORMAT* pFormat) { return D3D_OK; }
extern "C" WINAPI HRESULT D3DXSaveTextureToFileInMemory(void** ppDestBuffer, D3DXIMAGE_FILEFORMAT DestFormat, void* pSrcTexture, const void* pSrcRect) { return D3D_OK; }
extern "C" WINAPI HRESULT D3DXSaveSurfaceToFileInMemory(void** ppDestBuffer, D3DXIMAGE_FILEFORMAT DestFormat, void* pSrcSurface, const void* pSrcRect) { return D3D_OK; }

// D3DXCreateTextureFromResource / D3DXCreateTextureFromResourceEx
extern "C" WINAPI HRESULT D3DXCreateTextureFromResourceA(void* pDevice, void* hSrcModule, LPCSTR pSrcResource, void** ppTexture) { return D3D_OK; }
extern "C" WINAPI HRESULT D3DXCreateTextureFromResourceW(void* pDevice, void* hSrcModule, LPCWSTR pSrcResource, void** ppTexture) { return D3D_OK; }
extern "C" WINAPI HRESULT D3DXCreateTextureFromResourceExA(void* pDevice, void* hSrcModule, LPCSTR pSrcResource, UINT Width, UINT Height, UINT MipLevels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, DWORD Filter, DWORD MipFilter, D3DCOLOR ColorKey, void* pSrcInfo, void* pPalette, void** ppTexture) { return D3D_OK; }
extern "C" WINAPI HRESULT D3DXCreateTextureFromResourceExW(void* pDevice, void* hSrcModule, LPCWSTR pSrcResource, UINT Width, UINT Height, UINT MipLevels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, DWORD Filter, DWORD MipFilter, D3DCOLOR ColorKey, void* pSrcInfo, void* pPalette, void** ppTexture) { return D3D_OK; }

// D3DXCreateVolumeTextureFromResource / D3DXCreateCubeTextureFromResource (already defined above)
// D3DXCreateTextureFromFileEx
extern "C" WINAPI HRESULT D3DXCreateTextureFromFileExA(void* pDevice, LPCSTR pSrcFile, UINT Width, UINT Height, UINT MipLevels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, DWORD Filter, DWORD MipFilter, D3DCOLOR ColorKey, void* pSrcInfo, void* pPalette, void** ppTexture) {
    (void)pDevice; (void)pSrcFile; (void)Width; (void)Height; (void)MipLevels; (void)Usage; (void)Format; (void)Pool; (void)Filter; (void)MipFilter; (void)ColorKey; (void)pSrcInfo; (void)pPalette; (void)ppTexture;
    return D3D_OK;
}
extern "C" WINAPI HRESULT D3DXCreateTextureFromFileExW(void* pDevice, LPCWSTR pSrcFile, UINT Width, UINT Height, UINT MipLevels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, DWORD Filter, DWORD MipFilter, D3DCOLOR ColorKey, void* pSrcInfo, void* pPalette, void** ppTexture) {
    (void)pDevice; (void)pSrcFile; (void)Width; (void)Height; (void)MipLevels; (void)Usage; (void)Format; (void)Pool; (void)Filter; (void)MipFilter; (void)ColorKey; (void)pSrcInfo; (void)pPalette; (void)ppTexture;
    return D3D_OK;
}

// D3DXCreateTextureFromFileExInMemory
extern "C" WINAPI HRESULT D3DXCreateTextureFromFileExInMemory(void* pDevice, LPCVOID pSrcData, UINT SrcDataSize, UINT Width, UINT Height, UINT MipLevels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, DWORD Filter, DWORD MipFilter, D3DCOLOR ColorKey, void* pSrcInfo, void* pPalette, void** ppTexture) {
    (void)pDevice; (void)pSrcData; (void)SrcDataSize; (void)Width; (void)Height; (void)MipLevels; (void)Usage; (void)Format; (void)Pool; (void)Filter; (void)MipFilter; (void)ColorKey; (void)pSrcInfo; (void)pPalette; (void)ppTexture;
    return D3D_OK;
}

// D3DXLoadMeshHierarchyFromX / D3DXCreateFrame / D3DXCreateMeshContainer / etc
extern "C" WINAPI HRESULT D3DXLoadMeshHierarchyFromXA(LPCSTR pFilename, DWORD Options, void* pDevice, void* pLoadHierarchy, void* pLoadMesh, void** ppHierarchy) { return D3D_OK; }
extern "C" WINAPI HRESULT D3DXLoadMeshHierarchyFromXW(LPCWSTR pFilename, DWORD Options, void* pDevice, void* pLoadHierarchy, void* pLoadMesh, void** ppHierarchy) { return D3D_OK; }
extern "C" WINAPI HRESULT D3DXLoadMeshHierarchyFromXInMemory(LPCVOID pMemory, DWORD SizeofMemory, DWORD Options, void* pDevice, void* pLoadHierarchy, void* pLoadMesh, void** ppHierarchy) { return D3D_OK; }
extern "C" WINAPI HRESULT D3DXLoadMeshHierarchyFromXInMemoryEx(LPCVOID pMemory, DWORD SizeofMemory, DWORD Options, D3DPOOL Pool, void* pDevice, void* pLoadHierarchy, void* pLoadMesh, void** ppHierarchy) { return D3D_OK; }
extern "C" WINAPI HRESULT D3DXLoadMeshHierarchyFromXof(void* pXOfFile, DWORD Options, void* pDevice, void* pLoadHierarchy, void* pLoadMesh, void** ppHierarchy) { return D3D_OK; }
extern "C" WINAPI HRESULT D3DXCreateFrameA(LPCSTR pName, void** ppNewFrame) { return D3D_OK; }
extern "C" WINAPI HRESULT D3DXCreateFrameW(LPCWSTR pName, void** ppNewFrame) { return D3D_OK; }
extern "C" WINAPI HRESULT D3DXCreateMeshContainerA(LPCSTR pName, const void* pMeshData, const void* pMaterials, void* pEffectInstances, DWORD NumMaterials, const void* pAdjacency, void* pSkinInfo, void** ppNewMeshContainer) { return D3D_OK; }
extern "C" WINAPI HRESULT D3DXCreateMeshContainerW(LPCWSTR pName, const void* pMeshData, const void* pMaterials, void* pEffectInstances, DWORD NumMaterials, const void* pAdjacency, void* pSkinInfo, void** ppNewMeshContainer) { return D3D_OK; }

// D3DXGetFVFVertexSize already defined

} // namespace win2lin
