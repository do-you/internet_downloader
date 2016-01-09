#include <functional>
#include <numeric>
#include "http_parse.h"
#include "net_io.h"


using namespace std;

net_io::net_io(string url) :filemana(current_path())
{
	this->max_split = 1;
	has_init = false;
	file_len = 0;

	if (!parseUrl(url, info))
	{
		status = down_statues::failed;
		printf("%s 非法\n", url.c_str());
	}
	status = down_statues::pause;
}

uint64_t net_io::get_remain_len()
{
	return std::accumulate(connection_list.begin(), connection_list.end(), 0LLU, [](uint64_t a, unordered_map<connection*, block_ptr>::reference b) {return a + b.second->len(); });
}

void net_io::init(string filename, uint64_t len, down_info &info_2)
{
	assert(!has_init);

	int split_count;
	block_ptr *ary = new block_ptr[max_split];

	has_init = true;

	file_name = filename;
	file_len = len;
	filemana.init(filename, len);

	split_count = filemana.static_allocate(ary, max_split, min_split_size);
	for (int i = 0; i < split_count; i++)
	{
		auto x = new connection(info);
		connection_list.insert({ x, ary[i] });
		//初始化下载线程的参数
		x->init(bind(&net_io::block_complete, this, placeholders::_1, placeholders::_2), ary[i], filemana.create_block_manager(ary[i], max_cache_size / split_count));
		x->set_header(user_header);
		x->start();
	}

	delete[] ary;
}

void net_io::block_complete(connection *which, down_info &info_2)
{
	block_ptr ptr;

	filemana.block_complete(connection_list[which]);
	connection_list.erase(which);

	if (filemana.dynamic_allocate(ptr, min_split_size))
	{
		printf("重新分配 %llu-%llu\n", ptr->ptr, ptr->end);

		auto x = new connection(info);
		connection_list.insert({ x, ptr });
		//初始化下载线程的参数
		x->init(bind(&net_io::block_complete, this, placeholders::_1, placeholders::_2), ptr, filemana.create_block_manager(ptr, max_cache_size / max_split));
		x->set_header(user_header);
		x->start();
	}
	else if (connection_list.size() == 0)
		status = down_statues::writing_back;
}

void net_io::start()
{
	if (status == down_statues::pause)
	{
		//防止重入
		status = down_statues::downloading;

		if (has_init)
		{
			for (auto &x : connection_list)
				x.first->start();
		}
		else
		{
			auto x = new connection(info);
			x->set_header(user_header);
			x->sniff(bind(&net_io::init, this, placeholders::_1, placeholders::_2, placeholders::_3));
		}
	}
}

void net_io::pause()
{
	if (status == down_statues::downloading)
	{
		status = down_statues::pause;

		for (auto x : connection_list)
			x.first->pause();
	}
}

void net_io::write_process()
{
	if (has_init)
		filemana.write_progress();
}

void net_io::set_header(string parm, string value)
{
	if (user_header.count(parm) == 0)
		user_header.insert({ parm, value });
	else
		user_header[parm] = value;
}

void net_io::set_split(int nsplit)//如果允许任务暂停后更改线程
{
	if (!has_init)
		max_split = nsplit;
}

void net_io::set_path(string p)
{
	if (!has_init)
		filemana.set_path(p);
}

down_statues net_io::get_statues()
{
	if (status == down_statues::writing_back&&filemana.drain_all())
		status = down_statues::finish;

	return status;
}
