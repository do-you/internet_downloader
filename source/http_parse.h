#ifndef HTTP_PARSE_H
#define HTTP_PARSE_H

#include <string>
#include "iocp_base.h"
using namespace std;

bool parseUrl(string url, down_info &info);

#endif // !HTTP_PARSE_H

