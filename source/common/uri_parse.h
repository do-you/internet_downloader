#ifndef URI_PARSE_H
#define URI_PARSE_H

#include <string>
#include <stdint.h>
#include <WinSock2.h>
using namespace std;

enum class scheme_t
{
	http
};

struct uri_info
{
	scheme_t scheme;
	string user;
	string password;
	string host;
	string port;
	string urlpath;
};

// struct http_url_info
// {
// 	scheme_t scheme;
// 	string user;
// 	string password;
// 	string host;
// 	string port;
// 	string path_and_query;
// };

bool parseUrl(string uri, uri_info &info);
// bool checkurl(string url, string &file_name);
string getfilename(uri_info &info);

#endif // URI_PARSE_H

