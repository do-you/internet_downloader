#include <boost/regex.hpp>
#include <boost/format.hpp>
#include "uri_parse.h"
#include "iocp_base.h"

//base
string alpha = "[a-zA-Z]";
string digit = "[0-9]";
string digits = "[0-9]+";
string alphadigit = "[a-zA-Z0-9]";
string safe = "[-$_.+]";
string extra = "[!*'(),]";
string reserved = "[;/?:@&=]";
string hex = "[0-9A-Fa-f]";
string escape = "(%[0-9A-Fa-f]{2})";
string unreserved = str(boost::format("(%1%|%2%|%3%|%4%)") % alpha % digit % safe % extra);
string uchar = str(boost::format("(%1%|%2%)") % unreserved % escape);
string xchar = str(boost::format("(%1%|%2%|%3%)") % unreserved % reserved % escape);

//ip based
string scheme = "(?<scheme>([a-z]|[0-9]|[+-.])+)";
string user = "(\\w|[;?&=])*";
string password = user;
string domainlabel = str(boost::format("(%1%(%1%|-)*%1%|%1%)") % alphadigit);
string toplabel = str(boost::format("(%1%(%2%|-)*%2%|%1%)") % alpha % alphadigit);
string hostname = str(boost::format("((%1%\\.)*%2%)") % domainlabel % toplabel);
string hostnumber = str(boost::format("(%1%\\.%1%\\.%1%\\.%1%)") % digits);
string host = str(boost::format("(?<host>%1%|%2%)") % hostname % hostnumber);
string port = "(?<port>[0-9]+)";
string hostport = str(boost::format("(%1%(:%2%)?)") % host % port);
string login = str(boost::format("((%1%(:%2%)?@)?%3%)") % user % password % hostport);
string urlpath = str(boost::format("(?<urlpath>%1%*)") % xchar);
string ip_schemepart = str(boost::format("//%1%(/%2%)?") % login % urlpath);
string genericurl = str(boost::format("%1%:%2%") % scheme % ip_schemepart);

//http
// string hsegment = str(boost::format("((%1%|[;:@&=])*)") % uchar);
// string m_search = str(boost::format("((%1%|[;:@&=])*)") % uchar);
// string hpath = str(boost::format("(%1%(/%1%)*)") % hsegment);
// string httpurl = str(boost::format("http://%1%(?<hpath_and_query>/%2%(\\?%3%)?)?") % hostport % hpath % m_search);

std::string UrlDecode(const std::string &str);
std::string UrlEncode(const std::string &str);

bool parseUrl(string uri, uri_info &info)
{
    boost::smatch res;
    boost::regex re(genericurl);
    uri = UrlEncode(uri);

    if (boost::regex_match(uri, res, re))
    {
        auto scheme_match = res["scheme"];
        if (scheme_match.str() == "http")
            info.scheme = scheme_t::http;
        else
            return false;

        auto host_match = res["host"];
        info.host = host_match.str();

        auto port_match = res["port"];
        info.port = port_match.matched ? port_match.str() : "80";

        auto urlpath_match = res["urlpath"];
        info.urlpath = urlpath_match.str();

        return true;
    }
    else
        return false;
}

std::string getfilename(uri_info &info)
{
    auto filename_end = info.urlpath.find('?');
    auto filename_start = info.urlpath.rfind('/', filename_end) + 1;
    return UrlDecode(info.urlpath.substr(filename_start, filename_end - filename_start));
}

unsigned char ToHex(unsigned char x)
{
    return x > 9 ? x + 55 : x + 48;
}

unsigned char FromHex(unsigned char x)
{
    unsigned char y;
    if (x >= 'A' && x <= 'Z') y = x - 'A' + 10;
    else if (x >= 'a' && x <= 'z') y = x - 'a' + 10;
    else if (x >= '0' && x <= '9') y = x - '0';
    else
        assert(0);
    return y;
}

std::string UrlEncode(const std::string &str)
{
    std::string strTemp = "";
    size_t length = str.length();
    for (size_t i = 0; i < length; i++)
    {
        if (isalnum((unsigned char) str[i]) ||
            (str[i] == '-') ||
            (str[i] == '_') ||
            (str[i] == '.') ||
            (str[i] == '~') ||
            (str[i] == ':') ||
            (str[i] == '/'))
            strTemp += str[i];
        else if (str[i] == ' ')
            strTemp += "+";
        else
        {
            strTemp += '%';
            strTemp += ToHex((unsigned char) str[i] >> 4);
            strTemp += ToHex((unsigned char) str[i] % 16);
        }
    }
    return strTemp;
}

std::string UrlDecode(const std::string &str)
{
    std::string strTemp = "";
    size_t length = str.length();
    for (size_t i = 0; i < length; i++)
    {
        if (str[i] == '+') strTemp += ' ';
        else if (str[i] == '%')
        {
            assert(i + 2 < length);
            unsigned char high = FromHex((unsigned char) str[++i]);
            unsigned char low = FromHex((unsigned char) str[++i]);
            strTemp += high * 16 + low;
        }
        else strTemp += str[i];
    }
    return strTemp;
}

// bool checkurl(string url, string &file_name)
// {
// 	boost::smatch res;
// 	bool ismatch = false;
// 	boost::regex re("(?<protocol>https?|ftp)://(?<domain>[-A-Z0-9.]+)(?<file>/[-A-Z0-9+&@#/%=~_|!:,.;]*)?(?<parameters>\?[-A-Z0-9+&@#/%=~_|!:,.;]*)?");
// 	if (boost::regex_match(url, res, re))
// 	{
// 		string file = res["file"];
// 		int ops = file.rfind('/');
// 		file_name = file.substr(ops + 1);
// 		return true;
// 	}
// 	else
// 		return false;
// }
