#include <algorithm>
#include <functional>
#include "preferences.h"
#include "down_task.h"
using namespace std;


void set_split(task_info *c, const string &p);
void set_header(task_info *c, const string &p);
void set_path(task_info *c, const string &p);
void set_filename(task_info *c, const string &p);

const parm_implement parm_list[] = {
	{ "dir", set_path },
	{ "header", set_header },
	{ "out", set_filename},
	{ "split", set_split }
};

#define process_list_start parm_list
#define process_list_end (const parm_implement*)(&parm_list + 1)

void set_split(task_info *c, const string &p)
{
	int a = stoi(p);
	c->split = a;
}

void set_header(task_info *c, const string &p)
{
	int pos = 0;

	if (string::npos == (pos = p.find(':')))
		throw std::invalid_argument("自定义头部格式错误");
	else
	{
		auto x = p.substr(0, pos);
		auto y = p.substr(pos + 1);
		if (c->user_header.find(x) == c->user_header.end())
			c->user_header.insert({ x, y });
		else
			c->user_header[x] = y;
	}
}

void set_path(task_info *c, const string &p)
{
	create_directories(p);
	c->dir = p;//有编码问题
}

void set_filename(task_info * c, const string & p)
{
	c->file_name = p;
}

auto cmp = [](const string &a, const string &b) {return a.compare(b) < 0; };

std::pair<const parm_implement*, const parm_implement*> search_opt(string value)
{
	auto low = lower_bound(parm_list, process_list_end, value, cmp);

	auto up = low;

	while (up < process_list_end && up->cmd.length()>= value.length() && up->cmd.compare(0, value.size(), value) == 0) ++up;

	return make_pair(low, up);
}

int result_count(std::pair<const parm_implement*, const parm_implement*> &a)
{
	return a.second - a.first;
}
