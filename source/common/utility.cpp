#include"utility.h"
#include <WinSock2.h>
#include <Ws2tcpip.h>
#include <windows.h>
#include <stdio.h>
#include <string>
#include <assert.h>
#include <stdexcept>
#include <memory>

void util_err_exit(char const *ErrMsg)
{
    fprintf(stderr, "%s", ErrMsg);
    exit(-1);
}

int util_geterror()
{
    return GetLastError();
}

void util_errno_exit(char const *ErrMsg)
{
    char buf[256];
    int n = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, NULL, util_geterror(), 0, buf, 256, NULL);
    fprintf(stderr, "%s%s", ErrMsg, buf);
    exit(-1);
}

bool util_utf8_to_acp(std::string utf8_str, std::string &outstr)
{
    std::unique_ptr<wchar_t[]> lpUnicodeStr;
    std::unique_ptr<char[]> lpGBKStr;
    int nRetLen = 0;

    if (utf8_str.empty())  //如果UTF8字符串为NULL则出错退出
        return false;

    nRetLen = MultiByteToWideChar(CP_UTF8, 0, utf8_str.c_str(), -1, NULL, NULL);  //获取转换到Unicode编码后所需要的字符空间长度
    lpUnicodeStr.reset(new wchar_t[nRetLen]);  //为Unicode字符串空间
    nRetLen = MultiByteToWideChar(CP_UTF8, 0, utf8_str.c_str(), -1, lpUnicodeStr.get(), nRetLen);  //转换到Unicode编码
    if (!nRetLen)  //转换失败则出错退出
        return false;

    nRetLen = WideCharToMultiByte(CP_ACP, 0, lpUnicodeStr.get(), -1, NULL, NULL, NULL, NULL);  //获取转换到GBK编码后所需要的字符空间长度
    lpGBKStr.reset(new char[nRetLen]);
    nRetLen = ::WideCharToMultiByte(CP_ACP, 0, lpUnicodeStr.get(), -1, lpGBKStr.get(), nRetLen, NULL, NULL);  //转换到GBK编码
    if (!nRetLen)  //转换失败则出错退出
        return false;

    outstr.assign(lpGBKStr.get(), nRetLen);

    return true;
}

bool util_acp_to_utf8(std::string acp_str, std::string &outstr)
{
    std::unique_ptr<wchar_t[]> lpUnicodeStr;
    std::unique_ptr<char[]> lputfStr;
    int nRetLen = 0;

    if (acp_str.empty())  //如果GBK字符串为NULL则出错退出
        return false;

    nRetLen = ::MultiByteToWideChar(CP_ACP, 0, acp_str.c_str(), -1, NULL, NULL);  //获取转换到Unicode编码后所需要的字符空间长度
    lpUnicodeStr.reset(new wchar_t[nRetLen]);  //为Unicode字符串空间
    nRetLen = ::MultiByteToWideChar(CP_ACP, 0, acp_str.c_str(), -1, lpUnicodeStr.get(), nRetLen);  //转换到Unicode编码
    if (!nRetLen)  //转换失败则出错退出
        return false;

    nRetLen = ::WideCharToMultiByte(CP_UTF8, 0, lpUnicodeStr.get(), -1, NULL, NULL, NULL, NULL);//获取转换到UTF8编码后所需要的字符空间长度
    lputfStr.reset(new char[nRetLen]);
    nRetLen = ::WideCharToMultiByte(CP_UTF8, 0, lpUnicodeStr.get(), -1, lputfStr.get(), nRetLen, NULL, NULL);//转换到UTF8编码
    if (!nRetLen)  //转换失败则出错退出
        return false;

    outstr.assign(lputfStr.get(), nRetLen);

    return true;
}

struct addrinfo *util_getaddrinfo(char const *host, char const *port, int family, int socktype, int protocol)
{
    char hostname[256];
    int returnval;
    struct addrinfo *result = NULL;
    struct addrinfo *ptr = NULL;
    struct addrinfo hints;

    if (!host) {
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

char to_hex(uint8_t x)
{
    static const char res[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
    assert(x >= 0 && x < 16);
    return res[x];
}

std::string decimal_to_hex(uint64_t x)
{
    char res[16];
    for (int i = 15; i >= 0; --i) {
        res[i] = to_hex(x & 0xF);
        x >>= 4;
    }
    return std::string(res, 16);
}

uint64_t hex_to_decimal(std::string str)
{
    uint64_t res = 0;
    if (str.length() > 0 && str.length() <= 16) {
        for (auto x : str) {
            res <<= 4;
            if (x >= '0' && x <= '9')
                res |= (x - '0');
            else if (x >= 'a' && x <= 'f')
                res |= (x - 'a' + 10);
            else if (x >= 'A' && x <= 'F')
                res |= (x - 'A' + 10);
            else
                throw std::runtime_error("非法的字符串");
        }
        return res;
    }
    else
        throw std::runtime_error("非法的字符串");
}
