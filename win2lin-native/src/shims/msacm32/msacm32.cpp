// libwin_msacm32.so - stub shim for msacm32.dll (Audio Compression Manager).

#include "common/logging.hpp"

#include <cstdint>

using HRESULT = int32_t;
using DWORD = uint32_t;
using UINT = uint32_t;
using MMRESULT = UINT;
using HANDLE = void*;
using HACMSTREAM = HANDLE;
using HACMDRIVER = HANDLE;
using HACMOBJ = HANDLE;
using LPVOID = void*;
using LPCVOID = const void*;
using LPMMCKINFO = void*;
using LPWAVEFORMATEX = void*;
using LPACMDRIVERDETAILS = void*;
using LPACMFORMATDETAILS = void*;
using LPACMFORMATTAGDETAILS = void*;
using LPACMFILTERTAGDETAILS = void*;
using LPACMFILTERDETAILS = void*;
using LPACMSTREAMHEADER = void*;


using LPDWORD = DWORD*;
using DWORD_PTR = uintptr_t;
using UINT_PTR = uintptr_t;
using BYTE = uint8_t;
using LPWAVEFILTER = void*;
using ACMDRIVERENUMCBPFN = void*;
using ACMFORMATENUMCBPFN = void*;
using ACMFORMATTAGENUMCBPFN = void*;
using HACMDRIVERID = HANDLE;
using LPACMDRIVERDETAILSA = void*;
using LPACMDRIVERDETAILSW = void*;
using LPACMFORMATCHOOSEA = void*;
using LPACMFORMATCHOOSEW = void*;
using LPACMFILTERCHOOSEA = void*;
using LPACMFILTERCHOOSEW = void*;
using LPACMFORMATTAGDETAILSA = void*;
using LPACMFORMATTAGDETAILSW = void*;
using LPACMFORMATDETAILSA = void*;
using LPACMFORMATDETAILSW = void*;
using LPACMFILTERDETAILSA = void*;
using LPACMFILTERDETAILSW = void*;
using LPACMFILTERTAGDETAILSA = void*;
using LPACMFILTERTAGDETAILSW = void*;

namespace win2lin {

#define WINAPI __attribute__((ms_abi))

extern "C" WINAPI MMRESULT acmStreamOpen(HACMSTREAM* phas, HACMDRIVER had, LPWAVEFORMATEX pwfxSrc, LPWAVEFORMATEX pwfxDst, LPWAVEFILTER pwfltr, DWORD dwCallback, DWORD dwInstance, DWORD fdwOpen) {
    (void)phas; (void)had; (void)pwfxSrc; (void)pwfxDst; (void)pwfltr; (void)dwCallback; (void)dwInstance; (void)fdwOpen;
    return 0;
}
extern "C" WINAPI MMRESULT acmStreamClose(HACMSTREAM has, DWORD fdwClose) {
    (void)has; (void)fdwClose;
    return 0;
}
extern "C" WINAPI MMRESULT acmStreamSize(HACMSTREAM has, DWORD cbInput, LPDWORD lpdwOutputBytes, DWORD fdwSize) {
    (void)has; (void)cbInput; (void)lpdwOutputBytes; (void)fdwSize;
    return 0;
}
extern "C" WINAPI MMRESULT acmStreamConvert(HACMSTREAM has, LPACMSTREAMHEADER pahdr, DWORD fdwConvert) {
    (void)has; (void)pahdr; (void)fdwConvert;
    return 0;
}
extern "C" WINAPI MMRESULT acmStreamPrepareHeader(HACMSTREAM has, LPACMSTREAMHEADER pahdr, DWORD fdwPrepare) {
    (void)has; (void)pahdr; (void)fdwPrepare;
    return 0;
}
extern "C" WINAPI MMRESULT acmStreamUnprepareHeader(HACMSTREAM has, LPACMSTREAMHEADER pahdr, DWORD fdwUnprepare) {
    (void)has; (void)pahdr; (void)fdwUnprepare;
    return 0;
}
extern "C" WINAPI MMRESULT acmStreamReset(HACMSTREAM has, DWORD fdwReset) {
    (void)has; (void)fdwReset;
    return 0;
}
extern "C" WINAPI MMRESULT acmDriverEnum(ACMDRIVERENUMCBPFN fnCallback, DWORD dwInstance, DWORD fdwEnum) {
    (void)fnCallback; (void)dwInstance; (void)fdwEnum;
    return 0;
}
extern "C" WINAPI MMRESULT acmDriverOpen(HACMDRIVER* phad, UINT_PTR uDeviceID, DWORD fdwOpen) {
    (void)phad; (void)uDeviceID; (void)fdwOpen;
    return 0;
}
extern "C" WINAPI MMRESULT acmDriverClose(HACMDRIVER had, DWORD fdwClose) {
    (void)had; (void)fdwClose;
    return 0;
}
extern "C" WINAPI MMRESULT acmDriverMessage(HACMDRIVER had, UINT uMsg, DWORD_PTR dwParam1, DWORD_PTR dwParam2) {
    (void)had; (void)uMsg; (void)dwParam1; (void)dwParam2;
    return 0;
}
extern "C" WINAPI MMRESULT acmDriverPriority(HACMDRIVERID hadid, DWORD dwPriority, DWORD fdwPriority) {
    (void)hadid; (void)dwPriority; (void)fdwPriority;
    return 0;
}
extern "C" WINAPI MMRESULT acmDriverDetailsA(HACMDRIVERID hadid, LPACMDRIVERDETAILSA padd, DWORD cbAddDetails, DWORD fdwDetails) {
    (void)hadid; (void)padd; (void)cbAddDetails; (void)fdwDetails;
    return 0;
}
extern "C" WINAPI MMRESULT acmDriverDetailsW(HACMDRIVERID hadid, LPACMDRIVERDETAILSW padd, DWORD cbAddDetails, DWORD fdwDetails) {
    (void)hadid; (void)padd; (void)cbAddDetails; (void)fdwDetails;
    return 0;
}
extern "C" WINAPI MMRESULT acmFormatEnum(HACMOBJ hao, LPACMFORMATDETAILS pad, ACMFORMATENUMCBPFN fnCallback, DWORD dwInstance, DWORD fdwEnum) {
    (void)hao; (void)pad; (void)fnCallback; (void)dwInstance; (void)fdwEnum;
    return 0;
}
extern "C" WINAPI MMRESULT acmFormatTagEnum(HACMOBJ hao, LPACMFORMATTAGDETAILS patfd, ACMFORMATTAGENUMCBPFN fnCallback, DWORD dwInstance, DWORD fdwEnum) {
    (void)hao; (void)patfd; (void)fnCallback; (void)dwInstance; (void)fdwEnum;
    return 0;
}
extern "C" WINAPI MMRESULT acmFormatChooseA(LPACMFORMATCHOOSEA pafc) {
    (void)pafc;
    return 0;
}
extern "C" WINAPI MMRESULT acmFormatChooseW(LPACMFORMATCHOOSEW pafc) {
    (void)pafc;
    return 0;
}
extern "C" WINAPI MMRESULT acmFormatTagDetailsA(HACMOBJ hao, UINT uFormatTag, LPACMFORMATTAGDETAILS patfd, DWORD cbFormatTagDetails, DWORD fdwDetails) {
    (void)hao; (void)uFormatTag; (void)patfd; (void)cbFormatTagDetails; (void)fdwDetails;
    return 0;
}
extern "C" WINAPI MMRESULT acmFormatTagDetailsW(HACMOBJ hao, UINT uFormatTag, LPACMFORMATTAGDETAILSW patfd, DWORD cbFormatTagDetails, DWORD fdwDetails) {
    (void)hao; (void)uFormatTag; (void)patfd; (void)cbFormatTagDetails; (void)fdwDetails;
    return 0;
}
extern "C" WINAPI MMRESULT acmFormatDetailsA(HACMOBJ hao, LPACMFORMATDETAILS pad, DWORD cbFormatDetails, DWORD fdwDetails) {
    (void)hao; (void)pad; (void)cbFormatDetails; (void)fdwDetails;
    return 0;
}
extern "C" WINAPI MMRESULT acmFormatDetailsW(HACMOBJ hao, LPACMFORMATDETAILSW pad, DWORD cbFormatDetails, DWORD fdwDetails) {
    (void)hao; (void)pad; (void)cbFormatDetails; (void)fdwDetails;
    return 0;
}
extern "C" WINAPI MMRESULT acmFilterChooseA(LPACMFILTERCHOOSEA pafc) {
    (void)pafc;
    return 0;
}
extern "C" WINAPI MMRESULT acmFilterChooseW(LPACMFILTERCHOOSEW pafc) {
    (void)pafc;
    return 0;
}
extern "C" WINAPI MMRESULT acmFilterDetailsA(HACMOBJ hao, LPACMFILTERDETAILS pad, DWORD cbFilterDetails, DWORD fdwDetails) {
    (void)hao; (void)pad; (void)cbFilterDetails; (void)fdwDetails;
    return 0;
}
extern "C" WINAPI MMRESULT acmFilterDetailsW(HACMOBJ hao, LPACMFILTERDETAILSW pad, DWORD cbFilterDetails, DWORD fdwDetails) {
    (void)hao; (void)pad; (void)cbFilterDetails; (void)fdwDetails;
    return 0;
}
extern "C" WINAPI MMRESULT acmFilterTagDetailsA(HACMOBJ hao, UINT uFilterTag, LPACMFILTERTAGDETAILS patfd, DWORD cbFilterTagDetails, DWORD fdwDetails) {
    (void)hao; (void)uFilterTag; (void)patfd; (void)cbFilterTagDetails; (void)fdwDetails;
    return 0;
}
extern "C" WINAPI MMRESULT acmFilterTagDetailsW(HACMOBJ hao, UINT uFilterTag, LPACMFILTERTAGDETAILSW patfd, DWORD cbFilterTagDetails, DWORD fdwDetails) {
    (void)hao; (void)uFilterTag; (void)patfd; (void)cbFilterTagDetails; (void)fdwDetails;
    return 0;
}
extern "C" WINAPI MMRESULT acmGetVersion(LPDWORD lpdwVersion) {
    (void)lpdwVersion;
    return 0;
}
extern "C" WINAPI MMRESULT acmMetrics(HACMOBJ hao, UINT uMetric, LPVOID pMetric) {
    (void)hao; (void)uMetric; (void)pMetric;
    return 0;
}
extern "C" WINAPI MMRESULT acmSuggest(LPWAVEFORMATEX pwfxSrc, LPWAVEFORMATEX pwfxDst, DWORD cbwfx, DWORD fdwSuggest) {
    (void)pwfxSrc; (void)pwfxDst; (void)cbwfx; (void)fdwSuggest;
    return 0;
}

using LPDWORD = DWORD*;
using DWORD_PTR = uintptr_t;
using UINT_PTR = uintptr_t;
using BYTE = uint8_t;
using LPWAVEFILTER = void*;
using ACMDRIVERENUMCBPFN = void*;
using ACMFORMATENUMCBPFN = void*;
using ACMFORMATTAGENUMCBPFN = void*;
using HACMDRIVERID = HANDLE;
using LPACMDRIVERDETAILSA = void*;
using LPACMDRIVERDETAILSW = void*;
using LPACMFORMATCHOOSERA = void*;
using LPACMFORMATCHOOSEW = void*;
using LPACMFILTERCHOOSEA = void*;
using LPACMFILTERCHOOSEW = void*;
using LPACMFORMATTAGDETAILSA = void*;
using LPACMFORMATTAGDETAILSW = void*;
using LPACMFORMATDETAILSA = void*;
using LPACMFORMATDETAILSW = void*;
using LPACMFILTERDETAILSA = void*;
using LPACMFILTERDETAILSW = void*;
using LPACMFILTERTAGDETAILSA = void*;
using LPACMFILTERTAGDETAILSW = void*;

} // namespace win2lin
