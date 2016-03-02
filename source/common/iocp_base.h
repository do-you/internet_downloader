#pragma once

#include <WinSock2.h>
#include <Mswsock.h>
#include <windows.h>
#include <string>
#include "uri_parse.h"

class iocp_base;

struct overlapped_base
{
	OVERLAPPED overlapped;
	iocp_base *remind_me;
};

class iocp_base
{
public:
	virtual void complete_callback(int key, int nRecv, overlapped_base *over_base) = 0;
};

void bind_to_iocp(HANDLE fd, int key);
void win32_init();
void win32_loop();
void async_writefile(HANDLE fd, overlapped_base &sess, char *buffer, int len);

bool connect_to_server(SOCKET fd,uri_info &info);

void async_read(SOCKET fd, overlapped_base & sess, char * buffer, int len);
