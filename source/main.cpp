#include <stdio.h>
#include <list>
#include "iocp_base.h"
#include "Internet_Downloader.h"
using namespace std;

list<down_parm> parse_arg(int argc, char *argv[], uri_info &info)
{
	list<down_parm> temp_parm;
	bool have_url = false;

	for (int i = 1; i < argc; ++i)
	{
		if (argv[i][0] == '-')
		{
			if (i + 1 < argc && argv[i + 1][0] != '-')
				temp_parm.push_back({ argv[i] + 1, argv[++i] });
			else
				temp_parm.push_back({ argv[i] + 1, "true" });
		}
		else//url
		{
			if (!parseUrl(argv[i], info))
				util_err_exit("非法的url");
			else
				have_url = true;
		}
	}

	if (!have_url)
		util_err_exit("请传入url");

	return temp_parm;
}

int main(int argc, char *argv[])
{
	win32_init();

	uri_info info;
	auto x = parse_arg(argc, argv, info);

	Internet_Downloader idm;
	auto guid = idm.add_task(info, x);

	// 	Sleep(3000);
	// 	_DEBUG_OUT("call pause\n");
	// 	idm.pause_task(guid);
	// 	Sleep(3000);
	// 	idm.start_task(guid);
	while (true)
	{
		Sleep(1000);
		auto x = idm.get_new_recv();
		for (auto &y : x)
		{
			printf("task[%p]:speed %.2lf KB/s\n", y.first, (double)y.second / 1024);
		}
	}

	win32_loop();
	return 0;
}