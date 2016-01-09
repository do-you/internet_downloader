#pragma once

#include <WinSock2.h>
#include <windows.h>
#include <string>

void util_err_exit(char const * ErrMsg);

int util_geterror();

void util_errno_exit(char const * ErrMsg);

void util_w32_init();

bool util_utf8_to_acp(std::string utf8_str, std::string &outstr);

addrinfo * util_getaddrinfo(char const * host, char const * port, int family, int socktype, int protocol);
