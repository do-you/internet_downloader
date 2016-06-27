#ifndef UTILITY_H
#define UTILITY_H

#include <WinSock2.h>
#include <string>
#include <stdint.h>
#include <ws2tcpip.h>

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

bool util_utf8_to_acp(std::string utf8_str, std::string &outstr);

bool util_acp_to_utf8(std::string acp_str, std::string &outstr);

addrinfo* util_getaddrinfo(char const * host, char const * port, int family, int socktype, int protocol);

char to_hex(uint8_t x);

std::string decimal_to_hex(uint64_t x);
uint64_t hex_to_decimal(std::string str);

#endif // UTILITY_H

