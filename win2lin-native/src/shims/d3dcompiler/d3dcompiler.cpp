// libwin_d3dcompiler.so - stub shim for d3dcompiler_*.dll (HLSL compiler).
// Used by games with compiled HLSL shaders. Stubbed out - no compilation.

#include "common/logging.hpp"

#include <cstdint>

using HRESULT = int32_t;
using DWORD = uint32_t;
using UINT = uint32_t;
using LPCVOID = const void*;
using LPVOID = void*;
using LPCSTR = const char*;
using LPCWSTR = const wchar_t*;
using SIZE_T = uint64_t;


using BOOL = int32_t;
using WORD = uint16_t;
using BYTE = uint8_t;
using REFIID = const struct GUID*;
struct GUID { DWORD Data1; WORD Data2; WORD Data3; BYTE Data4[8]; };
using D3D_BLOB_PART = uint32_t;

namespace win2lin {

#define WINAPI __attribute__((ms_abi))

extern "C" WINAPI HRESULT D3DCompile(LPCVOID pSrcData, SIZE_T SrcDataSize, LPCSTR pFileName, void* pDefines, void* pInclude, LPCSTR pEntrypoint, LPCSTR pTarget, DWORD Flags1, DWORD Flags2, void** ppCode, void** ppErrorMsgs) {
    (void)pSrcData; (void)SrcDataSize; (void)pFileName; (void)pDefines; (void)pInclude; (void)pEntrypoint; (void)pTarget; (void)Flags1; (void)Flags2; (void)ppCode; (void)ppErrorMsgs;
    return -2147467259; // E_FAIL
}
extern "C" WINAPI HRESULT D3DCompileFromFile(LPCWSTR pFileName, void* pDefines, void* pInclude, LPCSTR pEntrypoint, LPCSTR pTarget, DWORD Flags1, DWORD Flags2, void** ppCode, void** ppErrorMsgs) {
    (void)pFileName; (void)pDefines; (void)pInclude; (void)pEntrypoint; (void)pTarget; (void)Flags1; (void)Flags2; (void)ppCode; (void)ppErrorMsgs;
    return -2147467259;
}
extern "C" WINAPI HRESULT D3DPrecompile(LPCVOID pSrcData, SIZE_T SrcDataSize, LPCSTR pFileName, void* pDefines, void* pInclude, LPCSTR pEntrypoint, LPCSTR pTarget, DWORD Flags1, DWORD Flags2, void** ppCode, void** ppErrorMsgs) {
    (void)pSrcData; (void)SrcDataSize; (void)pFileName; (void)pDefines; (void)pInclude; (void)pEntrypoint; (void)pTarget; (void)Flags1; (void)Flags2; (void)ppCode; (void)ppErrorMsgs;
    return -2147467259;
}
extern "C" WINAPI HRESULT D3DDisassemble(void* pShader, UINT ShaderSize, BOOL EnableColorCode, LPCSTR pComments, void** ppDisassembly) {
    (void)pShader; (void)ShaderSize; (void)EnableColorCode; (void)pComments; (void)ppDisassembly;
    return -2147467259;
}
extern "C" WINAPI HRESULT D3DDisassembleEx(void* pShader, UINT ShaderSize, DWORD Flags, void* pDisassembler, void** ppDisassembly) {
    (void)pShader; (void)ShaderSize; (void)Flags; (void)pDisassembler; (void)ppDisassembly;
    return -2147467259;
}
extern "C" WINAPI HRESULT D3DGetBlobPart(void* pBlob, D3D_BLOB_PART Part, UINT Flags, void* pOutputData, SIZE_T* pOutputSize) {
    (void)pBlob; (void)Part; (void)Flags; (void)pOutputData; (void)pOutputSize;
    return -2147467259;
}
extern "C" WINAPI HRESULT D3DStripShader(void* pShader, UINT StripFlags, void** ppStrippedShader, SIZE_T* pStrippedShaderSizeInBytes) {
    (void)pShader; (void)StripFlags; (void)ppStrippedShader; (void)pStrippedShaderSizeInBytes;
    return -2147467259;
}
extern "C" WINAPI HRESULT D3DReflect(void* pSrcData, SIZE_T SrcDataSize, REFIID pInterface, void** ppInterface) {
    (void)pSrcData; (void)SrcDataSize; (void)pInterface; (void)ppInterface;
    return -2147467259;
}
extern "C" WINAPI HRESULT D3DCreateBlob(SIZE_T Size, void** ppBlob) {
    (void)Size; (void)ppBlob;
    return -2147467259;
}
extern "C" WINAPI HRESULT D3DReadFileToBlob(LPCWSTR pFileName, void** ppBlob) {
    (void)pFileName; (void)ppBlob;
    return -2147467259;
}
extern "C" WINAPI HRESULT D3DWriteBlobToFile(void* pBlob, LPCWSTR pFileName, BOOL bOverwrite) {
    (void)pBlob; (void)pFileName; (void)bOverwrite;
    return -2147467259;
}

} // namespace win2lin
