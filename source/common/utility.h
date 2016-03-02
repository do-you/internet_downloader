#ifndef UTILITY_H
#define UTILITY_H

#include <WinSock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <string>

#ifdef _WIN32_WINNT
#define util_buf WSABUF
#endif

#ifdef _DEBUG_INFO
#define _DEBUG_OUT printf
#else
#define _DEBUG_OUT
#endif

void util_err_exit(char const * ErrMsg);

int util_geterror();

void util_errno_exit(char const * ErrMsg);

void util_w32_init();

bool util_utf8_to_acp(std::string utf8_str, std::string &outstr);

addrinfo* util_getaddrinfo(char const * host, char const * port, int family, int socktype, int protocol);

#endif // UTILITY_H

