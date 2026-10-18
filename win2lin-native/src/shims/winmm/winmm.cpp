// libwin_winmm.so - stub shim for winmm.dll (multimedia: time, joystick, mixer).

#include "common/logging.hpp"

#include <cstdint>

using UINT = uint32_t;
using DWORD = uint32_t;
using MMRESULT = UINT;
using LPVOID = void*;
using LPCSTR = const char*;
using LPCWSTR = const wchar_t*;
using LPTSTR = char*;
using LPWSTR = wchar_t*;
using UINT_PTR = uintptr_t;
using DWORD_PTR = uintptr_t;
using BOOL = int32_t;
using HANDLE = void*;
using LPTIMECAPS = void*;
using LPTIMECALLBACK = void*;
using LPMMTIME = void*;
using LPMIDIHDR = void*;
using LPWAVEHDR = void*;
using HMMIO = void*;
using HWAVEOUT = void*;
using HWAVEIN = void*;
using HMIDIOUT = void*;
using HMIDIIN = void*;
using HJOY = void*;
using HMCIDEVICE = void*;
using LPMCI_OPEN_PARMSA = void*;
using LPMCI_STATUS_PARMS = void*;
using LPMCI_PLAY_PARMS = void*;
using LPJOYINFO = void*;
using LPJOYINFOEX = void*;
using LPJOYCAPSA = void*;
using LPJOYCAPSW = void*;
using LPWAVEFORMATEX = void*;
using LPMMIOINFO = void*;
using MCIDEVICEID = uint32_t;
using LPHWAVEOUT = void*;

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif
using LPJOYINFO = void*;
using LPJOYINFOEX = void*;
using LPJOYCAPSA = void*;
using LPJOYCAPSW = void*;
using LPWAVEFORMATEX = void*;
using LPMMIOINFO = void*;
using MCIDEVICEID = uint32_t;

namespace win2lin {

#define WINAPI __attribute__((ms_abi))

extern "C" WINAPI MMRESULT timeGetDevCaps(LPTIMECAPS ptc, UINT cbtc) {
    (void)ptc; (void)cbtc;
    return 0;
}
extern "C" WINAPI DWORD timeGetTime(void) { return 0; }
extern "C" WINAPI UINT timeBeginPeriod(UINT uPeriod) { (void)uPeriod; return 0; }
extern "C" WINAPI UINT timeEndPeriod(UINT uPeriod) { (void)uPeriod; return 0; }
extern "C" WINAPI DWORD timeSetEvent(UINT uDelay, UINT uResolution, LPTIMECALLBACK lpTimeProc, DWORD_PTR dwUser, UINT uFlags) {
    (void)uDelay; (void)uResolution; (void)lpTimeProc; (void)dwUser; (void)uFlags;
    return 0;
}
extern "C" WINAPI BOOL timeKillEvent(UINT uTimerID) { (void)uTimerID; return TRUE; }

// Joystick
extern "C" WINAPI MMRESULT joyGetNumDevs(void) { return 0; }
extern "C" WINAPI MMRESULT joyGetPos(UINT uJoyID, LPJOYINFO pji) {
    (void)uJoyID; (void)pji;
    return 0;
}
extern "C" WINAPI MMRESULT joyGetPosEx(UINT uJoyID, LPJOYINFOEX pji) {
    (void)uJoyID; (void)pji;
    return 0;
}
extern "C" WINAPI MMRESULT joyGetDevCapsA(UINT uJoyID, LPJOYCAPSA pjc, UINT cbjc) {
    (void)uJoyID; (void)pjc; (void)cbjc;
    return 0;
}
extern "C" WINAPI MMRESULT joyGetDevCapsW(UINT uJoyID, LPJOYCAPSW pjc, UINT cbjc) {
    (void)uJoyID; (void)pjc; (void)cbjc;
    return 0;
}

// Wave
extern "C" WINAPI MMRESULT waveOutGetNumDevs(void) { return 0; }
extern "C" WINAPI MMRESULT waveOutOpen(LPHWAVEOUT phwo, UINT uDeviceID, LPWAVEFORMATEX pwfx, DWORD_PTR dwCallback, DWORD_PTR dwInstance, DWORD fdwOpen) {
    (void)phwo; (void)uDeviceID; (void)pwfx; (void)dwCallback; (void)dwInstance; (void)fdwOpen;
    return 0;
}
extern "C" WINAPI MMRESULT waveOutClose(HWAVEOUT hwo) { (void)hwo; return 0; }
extern "C" WINAPI MMRESULT waveOutWrite(HWAVEOUT hwo, LPWAVEHDR pwh, UINT cbwh) {
    (void)hwo; (void)pwh; (void)cbwh;
    return 0;
}
extern "C" WINAPI MMRESULT waveInGetNumDevs(void) { return 0; }

// MIDI
extern "C" WINAPI MMRESULT midiOutGetNumDevs(void) { return 0; }
extern "C" WINAPI MMRESULT midiInGetNumDevs(void) { return 0; }

// MCI
extern "C" WINAPI DWORD mciSendStringA(LPCSTR lpstrCommand, LPTSTR lpstrReturnString, UINT uReturnLength, HANDLE hwndCallback) {
    (void)lpstrCommand; (void)lpstrReturnString; (void)uReturnLength; (void)hwndCallback;
    return 0;
}
extern "C" WINAPI DWORD mciSendStringW(LPCWSTR lpstrCommand, LPWSTR lpstrReturnString, UINT uReturnLength, HANDLE hwndCallback) {
    (void)lpstrCommand; (void)lpstrReturnString; (void)uReturnLength; (void)hwndCallback;
    return 0;
}
extern "C" WINAPI DWORD mciSendCommandA(MCIDEVICEID IDDevice, UINT uMsg, DWORD fdwCommand, DWORD_PTR dwParam) {
    (void)IDDevice; (void)uMsg; (void)fdwCommand; (void)dwParam;
    return 0;
}
extern "C" WINAPI DWORD mciSendCommandW(MCIDEVICEID IDDevice, UINT uMsg, DWORD fdwCommand, DWORD_PTR dwParam) {
    (void)IDDevice; (void)uMsg; (void)fdwCommand; (void)dwParam;
    return 0;
}

// Misc
extern "C" WINAPI DWORD mmioGetInfo(HMMIO hmmio, LPMMIOINFO pmmioinfo, UINT wFlags) {
    (void)hmmio; (void)pmmioinfo; (void)wFlags;
    return 0;
}

} // namespace win2lin
