#include"utility.h"
#include <WinSock2.h>
#include <Ws2tcpip.h>
#include <windows.h>
#include <stdio.h>
#include <string>

void util_err_exit(char const* ErrMsg)
{
	fprintf(stderr, "%s", ErrMsg);
	exit(-1);
}
int util_geterror()
{
	return GetLastError();
}
void util_errno_exit(char const* ErrMsg)
{
	char buf[256];
	int n = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, NULL, util_geterror(), 0, buf, 256, NULL);
	fprintf(stderr, "%s%s", ErrMsg, buf);
	exit(-1);
}

bool util_utf8_to_acp(std::string utf8_str, std::string &outstr)
{
	wchar_t * lpUnicodeStr = NULL;
	int nRetLen = 0;

	if (utf8_str.empty())  //如果UTF8字符串为NULL则出错退出
		return false;

	nRetLen = MultiByteToWideChar(CP_UTF8, 0, utf8_str.c_str(), -1, NULL, NULL);  //获取转换到Unicode编码后所需要的字符空间长度
	lpUnicodeStr = new WCHAR[nRetLen];  //为Unicode字符串空间
	if (lpUnicodeStr == nullptr)
		return false;
	nRetLen = MultiByteToWideChar(CP_UTF8, 0, utf8_str.c_str(), -1, lpUnicodeStr, nRetLen);  //转换到Unicode编码
	if (!nRetLen)  //转换失败则出错退出
		return false;

	nRetLen = WideCharToMultiByte(CP_ACP, 0, lpUnicodeStr, -1, NULL, NULL, NULL, NULL);  //获取转换到GBK编码后所需要的字符空间长度
	char *lpGBKStr = new char[nRetLen];
	if (lpGBKStr == nullptr)
		return false;
	nRetLen = ::WideCharToMultiByte(CP_ACP, 0, lpUnicodeStr, -1, lpGBKStr, nRetLen, NULL, NULL);  //转换到GBK编码
	if (!nRetLen)  //转换失败则出错退出
		return false;

	outstr = lpGBKStr;

	delete[] lpGBKStr;
	delete[] lpUnicodeStr;

	return true;
}

struct addrinfo * util_getaddrinfo(char const* host, char const* port, int family, int socktype, int protocol)
{
	char hostname[256];
	int returnval;
	struct addrinfo *result = NULL;
	struct addrinfo *ptr = NULL;
	struct addrinfo hints;

	if (!host)
	{
		int ret = gethostname(hostname, sizeof(hostname));
		if (ret == SOCKET_ERROR)
			util_errno_exit("gethostname failed:");
		host = hostname;
	}

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = family;
	hints.ai_socktype = socktype;
	hints.ai_protocol = protocol;

	returnval = getaddrinfo(host, port, &hints, &result);

	return returnval == 0 ? result : NULL;
}
