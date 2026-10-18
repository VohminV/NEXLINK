// libwin_dinput8.so - stub shim for dinput8.dll (DirectInput).

#include "common/logging.hpp"

#include <cstdint>

using HINSTANCE = void*;
using HWND = void*;
using REFIID = void*;
using LPVOID = void*;
using HRESULT = int32_t;
using DWORD = uint32_t;
using LPCDIINSTANCE = void*;
using LPDIENUMDEVICESCALLBACKA = void*;
using LPDIENUMDEVICESCALLBACKW = void*;
using LPVOID2 = void*;
using LPDIRECTINPUT8A = void*;
using LPDIRECTINPUT8W = void*;
using LPDIRECTINPUTDEVICE8 = void*;
using LPDIENUMOBJECTSCALLBACKA = void*;
using LPDIENUMOBJECTSCALLBACKW = void*;
using LPDIDATAFORMAT = void*;
using LPDIPROPERTY = void*;
using LPDIDEVICEOBJECTINSTANCEA = void*;
using LPDIDEVICEOBJECTINSTANCEW = void*;
using DIPROPDWORD = void*;
using DIPROPGUIDANDPATH = void*;
using DIDEVCAPS = void*;
using DIDEVCAPS2 = void*;
using DIDEVICEINSTANCEA = void*;
using DIDEVICEINSTANCEW = void*;
using DIDEVICEOBJECTINSTANCEA = void*;
using DIDEVICEOBJECTINSTANCEW = void*;
using DIDATA = void*;
using DIDEVICEOBJECTDATA = void*;
using DIPROPDWORD = void*;
using DIPROPHEADER = void*;
using DIPROPSTRING = void*;
using DIPROPGUIDANDPATH = void*;
using DIPROPBUFFER = void*;
using DIPROPRANGE = void*;
using DIPROPCAL = void*;
using DIPROPCALPOV = void*;
using DIPROPCALPOS = void*;
using LPUNKNOWN = void*;

namespace win2lin {

#define WINAPI __attribute__((ms_abi))

extern "C" WINAPI HRESULT DirectInput8Create(HINSTANCE hinst, DWORD dwVersion, REFIID riidltf, LPVOID* ppvOut, LPUNKNOWN punkOuter) {
    (void)hinst; (void)dwVersion; (void)riidltf; (void)ppvOut; (void)punkOuter;
    return -2147467263; // E_NOINTERFACE
}

// IDirectInput8 methods (exported ordinally / by name where possible)
extern "C" WINAPI HRESULT DIEnumDevicesBySemanticsA(...) { return 0; }
extern "C" WINAPI HRESULT DIEnumDevicesBySemanticsW(...) { return 0; }
extern "C" WINAPI HRESULT DllCanUnloadNow(void) { return 0; }
extern "C" WINAPI HRESULT DllGetClassObject(...) { return 0; }

} // namespace win2lin
