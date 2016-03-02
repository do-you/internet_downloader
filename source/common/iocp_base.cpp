#include <WinSock2.h>
#include <Ws2tcpip.h>
#include <Mswsock.h>
#include <windows.h>
#include <thread>
#include <boost/system/system_error.hpp>
#include <boost/system/error_code.hpp>
#include "iocp_base.h"
#include "utility.h"

GUID GuidAcceptEx = WSAID_ACCEPTEX;
GUID GuidGetAcceptExSockaddrs = WSAID_GETACCEPTEXSOCKADDRS;
GUID GuidConnectEx = WSAID_CONNECTEX;

HANDLE iocp;
std::thread *workers;
int nthreads;

void worker()
{
	int bRet;
	DWORD dwBytesTransfered = 0;
	int key;
	OVERLAPPED *over = NULL;
	overlapped_base *ovlapbase;

	while (true)
	{
		bRet = GetQueuedCompletionStatus(iocp, &dwBytesTransfered, (PULONG_PTR)&key, &over, INFINITE);
		if (key == -1)
			break;

		if (!bRet && GetLastError() != ERROR_NETNAME_DELETED)
			util_errno_exit("GetQueuedCompletionStatus fail:");

		ovlapbase = CONTAINING_RECORD(over, overlapped_base, overlapped);
		if (ovlapbase->remind_me != nullptr)
			ovlapbase->remind_me->complete_callback(key, dwBytesTransfered, ovlapbase);
	}
}


struct
{
	LPFN_ACCEPTEX AcceptEx;
	LPFN_GETACCEPTEXSOCKADDRS GetAcceptExSockaddrs;
	LPFN_CONNECTEX ConnectEx;
}win32_ext_func;

void init_extent_func()
{
	bool isfirstget = true;

	if (isfirstget)
	{
		isfirstget = false;

		SOCKET tempSocket = socket(AF_INET, SOCK_STREAM, 0);
		DWORD dwBytes = 0;
		int iResult;
		if (tempSocket == SOCKET_ERROR)
			util_errno_exit("socket fail:");

		iResult = WSAIoctl(tempSocket, SIO_GET_EXTENSION_FUNCTION_POINTER,
			&GuidAcceptEx, sizeof(GuidAcceptEx),
			&win32_ext_func.AcceptEx, sizeof(LPFN_ACCEPTEX),
			&dwBytes, NULL, NULL);
		if (iResult == SOCKET_ERROR)
			util_errno_exit("get_extent_func fail:");

		iResult = WSAIoctl(tempSocket, SIO_GET_EXTENSION_FUNCTION_POINTER,
			&GuidGetAcceptExSockaddrs, sizeof(GuidGetAcceptExSockaddrs),
			&win32_ext_func.GetAcceptExSockaddrs, sizeof(LPFN_GETACCEPTEXSOCKADDRS),
			&dwBytes, NULL, NULL);
		if (iResult == SOCKET_ERROR)
			util_errno_exit("get_extent_func fail:");

		iResult = WSAIoctl(tempSocket, SIO_GET_EXTENSION_FUNCTION_POINTER,
			&GuidConnectEx, sizeof(GuidConnectEx),
			&win32_ext_func.ConnectEx, sizeof(LPFN_CONNECTEX),
			&dwBytes, NULL, NULL);
		if (iResult == SOCKET_ERROR)
			util_errno_exit("get_extent_func fail:");
	}
}
void win32_init()
{
	WSADATA wsaData;
	int iResult;

	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0)
		util_errno_exit("WSAStartup failed:");

	if ((iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0)) == NULL)
		util_errno_exit("CreateIoCompletionPort fail:");

	nthreads = std::thread::hardware_concurrency() * 2;
	workers = new std::thread[nthreads];
	for (int i = 0; i < nthreads; i++)
		workers[i] = std::thread(worker);
}

void win32_loop()
{
	for (int i = 0; i < nthreads; i++)
		workers[i].join();

	delete[] workers;
	WSACleanup();
}


void bind_to_iocp(HANDLE fd, int key)
{
	if (iocp != CreateIoCompletionPort(fd, iocp, key, 0))
		util_errno_exit("fail to bind socket to iocp:");
}

void async_writefile(HANDLE fd, overlapped_base &sess, char *buffer, int len)
{
	if (0 == WriteFile(fd, buffer, len, NULL, (OVERLAPPED *)&sess.overlapped) && GetLastError() != ERROR_IO_PENDING)
		util_errno_exit("WriteFile fail:");
}

bool connect_to_server(SOCKET fd, uri_info &info)
{
	int returnval;
	addrinfo *addr;

	addr = util_getaddrinfo(info.host.c_str(), info.port.c_str(), AF_INET, SOCK_STREAM, 0);
	if (addr == NULL)
		util_errno_exit("getaddrinfo fail:");

	do
	{
		returnval = connect(fd, addr->ai_addr, addr->ai_addrlen);
		if (returnval == 0)
		{
			freeaddrinfo(addr);
			return true;
		}
	} while ((addr = addr->ai_next) != NULL);

	freeaddrinfo(addr);
	return false;
}

void async_read(SOCKET fd, overlapped_base &sess, char *buffer, int len)
{
	DWORD Flags = 0;
	WSABUF buf;

	buf.buf = buffer;
	buf.len = len;

	if (SOCKET_ERROR == WSARecv(fd, &buf, 1, NULL, &Flags, (OVERLAPPED *)&sess.overlapped, NULL) && WSAGetLastError() != WSA_IO_PENDING)
		throw boost::system::system_error(boost::system::error_code(::WSAGetLastError(), boost::system::system_category()));
}