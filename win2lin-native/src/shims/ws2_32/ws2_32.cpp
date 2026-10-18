// libwin_ws2_32.so - stub shim for ws2_32.dll (Windows Sockets).
// Games from 2000-2010 rarely need real networking in single-player; stubs
// return safe defaults so initialization does not fail.

#include "common/logging.hpp"

#include <cstdint>

using SOCKET = uintptr_t;
using sockaddr = void;
using LPWSADATA = void*;
using LPFN_CONDITIONPROC = void*;

using DWORD = uint32_t;
using INT = int32_t;
using CHAR = char;
using LPSTR = char*;
using LPCSTR = const char*;
using HANDLE = void*;
using BOOL = int32_t;
using WSAEVENT = HANDLE;
using LPWSAPROTOCOL_INFOA = void*;
using GUID = void*;
using LPGUID = GUID*;
using LPVOID = void*;
using LPDWORD = DWORD*;
using LPSOCKET_ADDRESS = void*;
using DWORD_PTR = uint64_t;
using WORD = uint16_t;

namespace win2lin {

#define WINAPI __attribute__((ms_abi))

extern "C" WINAPI int WSAStartup(WORD wVersionRequired, LPWSADATA lpWSAData) {
    (void)wVersionRequired; (void)lpWSAData;
    return 0;
}
extern "C" WINAPI int WSACleanup(void) { return 0; }
extern "C" WINAPI SOCKET socket(int af, int type, int protocol) {
    (void)af; (void)type; (void)protocol;
    return (SOCKET)-1; // invalid socket
}
extern "C" WINAPI int bind(SOCKET s, const sockaddr* addr, int namelen) {
    (void)s; (void)addr; (void)namelen;
    return -1;
}
extern "C" WINAPI int listen(SOCKET s, int backlog) {
    (void)s; (void)backlog;
    return -1;
}
extern "C" WINAPI SOCKET accept(SOCKET s, sockaddr* addr, int* addrlen) {
    (void)s; (void)addr; (void)addrlen;
    return (SOCKET)-1;
}
extern "C" WINAPI int connect(SOCKET s, const sockaddr* name, int namelen) {
    (void)s; (void)name; (void)namelen;
    return -1;
}
extern "C" WINAPI int closesocket(SOCKET s) {
    (void)s;
    return 0;
}
extern "C" WINAPI int send(SOCKET s, const char* buf, int len, int flags) {
    (void)s; (void)buf; (void)len; (void)flags;
    return 0;
}
extern "C" WINAPI int recv(SOCKET s, char* buf, int len, int flags) {
    (void)s; (void)buf; (void)len; (void)flags;
    return 0;
}
extern "C" WINAPI int WSAGetLastError(void) { return 0; }
extern "C" WINAPI void WSASetLastError(int iError) { (void)iError; }
extern "C" WINAPI char* inet_ntoa(void* in) {
    (void)in;
    return (char*)"0.0.0.0";
}
extern "C" WINAPI unsigned long inet_addr(const char* cp) {
    (void)cp;
    return 0;
}
extern "C" WINAPI int ioctlsocket(SOCKET s, long cmd, u_long* argp) {
    (void)s; (void)cmd; (void)argp;
    return 0;
}
extern "C" WINAPI int select(int nfds, fd_set* readfds, fd_set* writefds, fd_set* exceptfds, const timeval* timeout) {
    (void)nfds; (void)readfds; (void)writefds; (void)exceptfds; (void)timeout;
    return 0;
}
extern "C" WINAPI int WSAIoctl(SOCKET s, DWORD dwIoControlCode, LPVOID lpvInBuffer, DWORD cbInBuffer, LPVOID lpvOutBuffer, DWORD cbOutBuffer, LPDWORD lpcbBytesReturned, LPVOID lpOverlapped, LPVOID lpCompletionRoutine) {
    (void)s; (void)dwIoControlCode; (void)lpvInBuffer; (void)cbInBuffer; (void)lpvOutBuffer; (void)cbOutBuffer; (void)lpcbBytesReturned; (void)lpOverlapped; (void)lpCompletionRoutine;
    return -1;
}
extern "C" WINAPI int WSAEventSelect(SOCKET s, WSAEVENT hEventObject, long lNetworkEvents) {
    (void)s; (void)hEventObject; (void)lNetworkEvents;
    return 0;
}

} // namespace win2lin
