// libwin_dsound.so - stub shim for dsound.dll (DirectSound).

#include "common/logging.hpp"

#include <cstdint>

using HINSTANCE = void*;
using HWND = void*;
using REFCLSID = void*;
using REFIID = void*;
using LPVOID = void*;
using HRESULT = int32_t;
using DWORD = uint32_t;
using LPCLSID = void*;
using LPCGUID = void*;
using LPUNKNOWN = void*;
using LPDIRECTSOUND = void*;
using LPDIRECTSOUND8 = void*;
using LPDIRECTSOUNDCAPTURE = void*;
using LPDIRECTSOUNDCAPTURE8 = void*;
using LPCDSBUFFERDESC = void*;
using LPCDSCBUFFERDESC = void*;
using LPDSEFFECTDESCRIPTOR = void*;
using LPDSPROPERTY = void*;
using LPDIRECTSOUNDFXCHORUS = void*;
using LPDIRECTSOUNDFXCOMPRESSOR = void*;
using LPDIRECTSOUNDFXDISTORTION = void*;
using LPDIRECTSOUNDFXECHO = void*;
using LPDIRECTSOUNDFXFLANGER = void*;
using LPDIRECTSOUNDFXGARGLE = void*;
using LPDIRECTSOUNDFXI3DL2REVERB = void*;
using LPDIRECTSOUNDFXPARAMEQ = void*;
using LPDIRECTSOUNDFXWAVESREVERB = void*;

namespace win2lin {

#define WINAPI __attribute__((ms_abi))

extern "C" WINAPI HRESULT DirectSoundCreate(LPCLSID pcGuidDevice, LPDIRECTSOUND* ppDS, LPUNKNOWN pUnkOuter) {
    (void)pcGuidDevice; (void)ppDS; (void)pUnkOuter;
    return 0;
}
extern "C" WINAPI HRESULT DirectSoundCreate8(LPCGUID pcGuidDevice, LPDIRECTSOUND8* ppDS8, LPUNKNOWN pUnkOuter) {
    (void)pcGuidDevice; (void)ppDS8; (void)pUnkOuter;
    return 0;
}
extern "C" WINAPI HRESULT DirectSoundCaptureCreate(LPCGUID pcGuidDevice, LPDIRECTSOUNDCAPTURE* ppDSC, LPUNKNOWN pUnkOuter) {
    (void)pcGuidDevice; (void)ppDSC; (void)pUnkOuter;
    return 0;
}
extern "C" WINAPI HRESULT DirectSoundCaptureCreate8(LPCGUID pcGuidDevice, LPDIRECTSOUNDCAPTURE8* ppDSC8, LPUNKNOWN pUnkOuter) {
    (void)pcGuidDevice; (void)ppDSC8; (void)pUnkOuter;
    return 0;
}
extern "C" WINAPI HRESULT DllCanUnloadNow(void) { return 0; }
extern "C" WINAPI HRESULT DllGetClassObject(...) { return 0; }

} // namespace win2lin
