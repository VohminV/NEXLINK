// libwin_iphlpapi.so - stub shim for iphlpapi.dll (IP Helper).

#include "common/logging.hpp"

#include <cstdint>

using DWORD = uint32_t;
using ULONG = uint32_t;
using UINT = uint32_t;
using PULONG = ULONG*;
using PVOID = void*;
using LPVOID = void*;
using LPCVOID = const void*;
using LPWSTR = wchar_t*;
using PIP_ADAPTER_ADDRESSES = void*;
using PIP_ADAPTER_INFO = void*;
using PIP_FORWARD_ROW = void*;
using PIP_MEDIA = void*;
using PMIB_IFROW = void*;
using PMIB_IFTABLE = void*;
using PMIB_TCPROW = void*;
using PMIB_TCPTABLE = void*;
using PMIB_UDPROW = void*;
using PMIB_UDPTABLE = void*;
using PVOID = void*;


using BOOL = int32_t;
using PMIB_IPADDRTABLE = void*;
using PMIB_IPFORWARDTABLE = void*;
using PMIB_TCPTABLE = void*;
using PMIB_UDPTABLE = void*;
using PMIB_IFROW = void*;
using PMIB_IFTABLE = void*;
using PFIXED_INFO = void*;
using PMIB_IPFORWARDROW = void*;

namespace win2lin {

#define WINAPI __attribute__((ms_abi))

extern "C" WINAPI DWORD GetAdaptersInfo(PIP_ADAPTER_INFO pAdapterInfo, PULONG pOutBufLen) {
    (void)pAdapterInfo; (void)pOutBufLen;
    return 0;
}
extern "C" WINAPI DWORD GetAdaptersAddresses(ULONG Family, DWORD Flags, PVOID Reserved, PIP_ADAPTER_ADDRESSES pAdapterAddresses, PULONG pOutBufLen) {
    (void)Family; (void)Flags; (void)Reserved; (void)pAdapterAddresses; (void)pOutBufLen;
    return 0;
}
extern "C" WINAPI DWORD GetIpAddrTable(PMIB_IPADDRTABLE pIpAddrTable, PULONG pdwSize, BOOL bOrder) {
    (void)pIpAddrTable; (void)pdwSize; (void)bOrder;
    return 0;
}
extern "C" WINAPI DWORD GetIpForwardTable(PMIB_IPFORWARDTABLE pIpForwardTable, PULONG pdwSize, BOOL bOrder) {
    (void)pIpForwardTable; (void)pdwSize; (void)bOrder;
    return 0;
}
extern "C" WINAPI DWORD GetTcpTable(PMIB_TCPTABLE pTcpTable, PULONG pdwSize, BOOL bOrder) {
    (void)pTcpTable; (void)pdwSize; (void)bOrder;
    return 0;
}
extern "C" WINAPI DWORD GetUdpTable(PMIB_UDPTABLE pUdpTable, PULONG pdwSize, BOOL bOrder) {
    (void)pUdpTable; (void)pdwSize; (void)bOrder;
    return 0;
}
extern "C" WINAPI DWORD GetBestInterface(DWORD dwDestAddr, DWORD* pdwBestIfIndex) {
    (void)dwDestAddr; (void)pdwBestIfIndex;
    return 0;
}
extern "C" WINAPI DWORD GetBestRoute(DWORD dwDestAddr, DWORD dwSourceAddr, DWORD* pdwBestIfIndex, PMIB_IPFORWARDROW pBestRoute) {
    (void)dwDestAddr; (void)dwSourceAddr; (void)pdwBestIfIndex; (void)pBestRoute;
    return 0;
}
extern "C" WINAPI DWORD GetIfEntry(PMIB_IFROW pIfRow) {
    (void)pIfRow;
    return 0;
}
extern "C" WINAPI DWORD GetIfTable(PMIB_IFTABLE pIfTable, PULONG pdwSize, BOOL bOrder) {
    (void)pIfTable; (void)pdwSize; (void)bOrder;
    return 0;
}
extern "C" WINAPI DWORD GetNetworkParams(PFIXED_INFO pFixedInfo, PULONG pOutBufLen) {
    (void)pFixedInfo; (void)pOutBufLen;
    return 0;
}
extern "C" WINAPI DWORD GetAdapterIndex(PVOID lpAddress, PULONG pIfIndex) {
    (void)lpAddress; (void)pIfIndex;
    return 0;
}
extern "C" WINAPI DWORD GetAdapterName(LPVOID lpAddress, LPWSTR pAdapterName, PULONG pAdapterNameLen) {
    (void)lpAddress; (void)pAdapterName; (void)pAdapterNameLen;
    return 0;
}
extern "C" WINAPI DWORD GetNumberOfInterfaces(DWORD* pdwNumIf) {
    (void)pdwNumIf;
    return 0;
}
extern "C" WINAPI DWORD GetPerAdapterInfo(UINT IfIndex, PVOID pPerAdapterInfo, PULONG pOutBufLen) {
    (void)IfIndex; (void)pPerAdapterInfo; (void)pOutBufLen;
    return 0;
}

using BOOL = int32_t;
using PMIB_IPADDRTABLE = void*;
using PMIB_IPFORWARDTABLE = void*;
using PMIB_TCPTABLE = void*;
using PMIB_UDPTABLE = void*;
using PMIB_IFROW = void*;
using PMIB_IFTABLE = void*;
using PFIXED_INFO = void*;
using PMIB_IPFORWARDROW = void*;

} // namespace win2lin
