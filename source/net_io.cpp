#include <functional>
#include <algorithm>
#include "net_io.h"
#include "down_task.h"
#include "file_block.h"
using namespace std;


void qvalue(int n, uint64_t ary[], uint16_t res[], int len)
{
	uint64_t total = 0;
	long double max;
	int which;

	for (int i = 0; i < len; ++i)
		total += ary[i];

	for (int i = 0; i < len; ++i)
	{
		auto x = ary[i] * n / total;
		res[i] = x;
		n -= x;
	}

	while (n > 0)
	{
		max = -1;
		which = -1;
		for (int i = 0; i < len; i++)
		{
			long double temp = (long double)ary[i] / res[i] / (res[i] + 1) * ary[i];

			assert(temp > 0);

			if (temp > max)
			{
				max = temp;
				which = i;
			}
		}
		++res[which];
		--n;
	}
}


net_io::net_io(down_task *parent, task_info *parms)
{
	this->parent = parent;
	this->parms = parms;
	in_pause = had_init = false;
	pause_discout = 0;
	new_recv = 0;
	can_split = true;
}

void net_io::start()
{
	assert(had_init);

	connection_map_lock.lock();

	for (auto x : connection_map)
		x.first->start();

	connection_map_lock.unlock();

}

void net_io::pause()
{
	connection_map_lock.lock();

	in_pause = true;
	pause_discout = connection_map.size();
	for (auto x : connection_map)
		x.first->pause();

	connection_map_lock.unlock();

}

int net_io::get_split_count()
{
	return connection_map.size();
}

void net_io::notify(connection *which, int evcode, void *parm, const char *msg)
{
	switch (evcode)
	{
		case (int)connection_evcode::sniff:
			_DEBUG_OUT("get file info success:file_len %llu\n", parms->file_len);
			parent->notify(this, (int)net_evcode::get_file_info, NULL, NULL);
			break;
		case (int)connection_evcode::complete:
		{
			connection_map_lock.lock();

			connection_map.erase(which);
			connection* new_con = split();
			if (new_con != NULL && in_pause != true)
				new_con->start();

			bool is_over = connection_map.empty();
			_DEBUG_OUT("connection[%p] 接收完成,线程数 %d \n", which, connection_map.size());

			connection_map_lock.unlock();

			if (is_over)
				parent->notify(this, (int)net_evcode::complete, NULL, NULL);
			break;
		}
		case (int)connection_evcode::failed://要很多处理
			parent->notify(this, (int)net_evcode::fail, NULL, msg);
			break;
		case (int)connection_evcode::pause:
			if (--pause_discout == 0)
			{
				in_pause = false;
				parent->notify(this, (int)net_evcode::had_puase, NULL, NULL);
			}
			break;
		default:
			break;
	}
}


void net_io::async_get_file_info()
{
	connection::sniff(this, parms);
}

void net_io::init(std::list<block*> block_list)
{
	alloc_list.swap(block_list);

	if (!had_init && alloc_list.size() > 0)
	{
		uint64_t *len_ary = new uint64_t[alloc_list.size()];
		uint16_t *res = new uint16_t[alloc_list.size()];
		memset(res, 0, sizeof(uint16_t)*alloc_list.size());
		int i = 0;

		for (auto x : alloc_list)
			len_ary[i++] = x->len();
		qvalue(parms->split, len_ary, res, alloc_list.size());

		i = 0;
		for (auto x = alloc_list.begin(); x != alloc_list.end(); ++i)
		{
			if (res[i] != 0)
			{
				auto y = x;
				++x;

				auto temp_list = (*y)->divide(res[i], parms->min_split_size);

				connection_map.emplace(new connection(this, parms, *y), *y);
				for (auto z : temp_list)
					connection_map.emplace(new connection(this, parms, z), z);

				alloc_list.erase(y);
			}
			else
				++x;
		}

		delete[] len_ary;
		delete[] res;
	}
	else
		util_err_exit("net_io::init failed");

	had_init = true;
}

void net_io::submit_recv(uint32_t n)
{
	new_recv.fetch_add(n);
}

uint32_t net_io::get_new_recv()
{
	return new_recv.exchange(0);
}

connection* net_io::split()
{
	block *which = NULL;
	block *which2 = NULL;
	block *which3 = NULL;
	connection *res = NULL;

	if (can_split)
	{
		auto x = std::max_element(connection_map.begin(), connection_map.end(), [](const pair<connection*, block*> &a, const pair<connection*, block*> &b){
			return a.second->len() < b.second->len();
		});

		auto y = std::max_element(alloc_list.begin(), alloc_list.end(), [](const block *a, const block *b){
			return a->len() < b->len();
		});

		which = (x != connection_map.end() && x->second->len() > 2 * parms->min_split_size ? x->second : NULL);
		which2 = (y != alloc_list.end() && (*y)->len() > 2 * parms->min_split_size ? *y : NULL);

		if (!which && !which2)
			can_split = false;
		else//有一个非空
		{
			if (which2 == NULL || which->len() > which2->len())//分割which
				which3 = which->divide(2, parms->min_split_size).front();
			else
			{
				which3 = which2;
				alloc_list.erase(y);
			}

			res = new connection(this, parms, which3);
			connection_map.emplace(res, which3);
		}
	}

	return res;
}

