#include <stdio.h>
#include <functional>
#include <unordered_set>
#include <unordered_map>
#include <map>
#include <algorithm>
#include <iterator>
#include "net_io.h"
#include "utility.h"
using namespace std;


struct cmd_process
{
	const char *cmd;
	void(*process)(net_io &, char *);

	operator const char*() const
	{
		return cmd;
	}
};

void set_split(net_io &c, char *p)
{
	int a;
	if (0 == (a = atoi(p)))
		util_err_exit("解析错误");
	else
		c.set_split(a);
}
void set_header(net_io &c, char *p)
{
	char *pos = NULL;

	if (NULL == (pos = strchr(p, ':')))
		util_err_exit("解析错误");
	else
	{
		*pos = '\0';
		if (strlen(p) == 0 || strlen(pos + 1) == 0)
			util_err_exit("解析错误");
		else
			c.set_header(p, pos + 1);
	}
}
void set_path(net_io &c, char *p)
{
	boost::system::error_code erco;
	is_directory(p, erco);
	if (erco.value() != 0)
		util_err_exit(erco.message().c_str());
	else
		c.set_path(p);
}

cmd_process process_list[] = {
	{ "d", set_path },
	{ "header", set_header },
	{ "s", set_split }
};

// #define process_list_end (cmd_process*)(&process_list + 1)
#define process_list_end process_list+3

bool cmp(const char *a, const char *b)
{
	return strcmp(a, b) < 0;
}

int main(int argc, char *argv[])
{
	win32_init();

	char temp[150];
	int i, j;
	string url = argv[argc - 1];

	down_info temp2;
	if (!parseUrl(url, temp2))
		util_err_exit("格式错误\n");
	net_io seon(url);


	for (i = 1; i < argc - 1; i++)
	{
		if (argv[i][0] == '-')
		{
			char *cmd = argv[i];
			while (*cmd == '-') ++cmd;

			auto begin = process_list;
			auto end = process_list_end;

			for (j = 0; j < strlen(cmd); ++j)
			{
				memcpy(temp, cmd, j + 1);
				temp[j + 1] = '\0';

				auto low = lower_bound(begin, end, temp, cmp);
				if (low == process_list_end)
					util_err_exit("无匹配\n");
				++temp[j];
				auto up = upper_bound(begin, end, temp, cmp);

				begin = low;
				end = up;

				if (up - low == 1)
				{
					if (strlen(cmd) > strlen(low->cmd))
					{
						//剩下的就是参数
						low->process(seon, cmd + strlen(low->cmd));
						break;
					}
					else
					{
						if (++i < argc - 1)
						{
							low->process(seon, argv[i]);
							break;
						}
						else
							util_err_exit("参数不完整");
					}
				}
			}
			if (j == strlen(cmd))
				util_err_exit("匹配不到唯一的\n");
		}
	}//parse end


	seon.start();
	while (true)
	{
		double progress = (seon.get_file_len() - seon.get_remain_len()) / (double)seon.get_file_len();
		printf("%d  %s  s:%d  p:%2.2lf\%\n", seon.get_statues(), seon.get_file_name().c_str(), seon.get_split_count(), progress * 100);
		if (++i % 5 == 0)
		{
			seon.write_process();
			printf("以保存记录\n");
		}
		Sleep(1000);
	}


	win32_loop();
	return 0;
}