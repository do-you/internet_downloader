#include "http_parse.h"
#include "iocp_base.h"


bool parseUrl(string url, down_info &info)
{
	int protocol_end, host_end, port_start, filename_end, filename_start;
	// 	std::string protocol, host, port, path,filename;

	if ((protocol_end = url.find("://")) == std::string::npos)
		return false;
	info.protocol = url.substr(0, protocol_end);
	if (info.protocol != "http")
		return false;

	if ((host_end = url.find("/", protocol_end + 3)) == std::string::npos)
		return false;
	info.host = url.substr(protocol_end + 3, host_end - protocol_end - 3);

	if ((port_start = info.host.rfind(":")) != std::string::npos)
	{
		info.port = info.host.substr(port_start + 1);
		info.host = info.host.substr(0, port_start);
	}
	else
		info.port = "80";

	info.path = url.substr(host_end);

	filename_end = info.path.rfind("?");
	filename_start = info.path.rfind("/", filename_end);
	info.filename = info.path.substr(filename_start + 1, filename_end - filename_start - 1);

	return true;
}
