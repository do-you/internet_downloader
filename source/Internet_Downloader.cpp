#include <boost/filesystem.hpp>
#include <stdexcept>
#include <mutex>
#include "Internet_Downloader.h"
#include "down_task.h"
#include "utility.h"
#include "uri_parse.h"
#include "preferences.h"
#include "task_schedule.h"
using namespace boost::filesystem;

static conf global_conf = { "F:\\文件传输测试", 16 * 1024 * 1024, 1, 2 * 1024 * 1024, 1 };


Internet_Downloader::Internet_Downloader() :Internet_Downloader(global_conf){}

Internet_Downloader::Internet_Downloader(conf &con) : configure(con), scheduler(con.max_concurrent_downloads){}

size_t Internet_Downloader::add_task(uri_info info, list<down_parm> &parms)
{
	unique_ptr<task_info> new_task(new task_info);
	new_task->info = info;
	new_task->file_name = getfilename(info);
	new_task->dir = configure.dir;
	new_task->split = configure.split;
	new_task->min_split_size = configure.min_split_size;
	new_task->statues = task_info::pause;

	for (auto &x : parms)
	{
		auto res = search_opt(x.parm);

		if (result_count(res) == 0)
			throw  invalid_argument("无效的参数:" + x.parm);
		else if (result_count(res) == 1)
			res.first->process(new_task.get(), x.value);
		else
			throw  invalid_argument("参数不唯一:" + x.parm);
	}

	_DEBUG_OUT("task[%p] create\n", new_task.get());

	if (scheduler.in(new_task.get()))
	{
		_DEBUG_OUT("task[%p] start\n", new_task.get());

		new_task->statues = task_info::active;
		new_task->start(this);
	}
	else
	{
		_DEBUG_OUT("task[%p] get into waitting queue\n", new_task.get());

		new_task->statues = task_info::waitting;
	}

	task_set_lock.lock();

	task_set.insert(new_task.get());

	task_set_lock.unlock();

	return (size_t)new_task.release();
}
task_info* Internet_Downloader::guid_convert(size_t guid)
{
	task_info *ptr = (task_info *)guid;

	task_set_lock.lock();

	auto res = task_set.count(ptr);

	task_set_lock.unlock();

	return res == 1 ? ptr : NULL;
}


void Internet_Downloader::start_task(size_t guid)
{
	task_info *ptr;
	if (ptr = guid_convert(guid))
	{
		lock_guard<mutex> lock(ptr->_lock);

		if (ptr->statues == task_info::pause)
		{
			if (scheduler.in(ptr))
			{
				_DEBUG_OUT("task[%p] start\n", ptr);

				ptr->statues = task_info::active;
				ptr->start(this);
			}
			else
			{
				_DEBUG_OUT("task[%p] get into waitting queue\n", ptr);

				ptr->statues = task_info::waitting;
			}
		}
		else
			throw runtime_error("此任务不能开始 GUID:" + to_string(guid));
	}
	else
		throw invalid_argument("无效的GUID:" + to_string(guid));
}

void Internet_Downloader::pause_task(size_t guid)
{
	task_info *ptr;
	if (ptr = guid_convert(guid))
	{
		lock_guard<mutex> lock(ptr->_lock);

		if (ptr->statues == task_info::pending)
			throw runtime_error("暂不支持暂停pending中的任务 GUID:" + to_string(guid));
		else if (ptr->statues == task_info::active)
		{
			ptr->statues = task_info::pending;
			ptr->task_ptr->pause();
		}
		else if (ptr->statues == task_info::waitting)
		{
			_DEBUG_OUT("task[%p] pause\n", ptr);

			scheduler.out(ptr);
			ptr->statues = task_info::pause;
		}
		else
			throw runtime_error("非法操作 GUID:" + to_string(guid));
	}
	else
		throw invalid_argument("无效的GUID:" + to_string(guid));
}

void Internet_Downloader::remove_task(size_t guid)
{
	task_info *ptr;
	if (ptr = guid_convert(guid))
	{
		lock_guard<mutex> lock(ptr->_lock);

		if (ptr->statues == task_info::active || ptr->statues == task_info::pending)
			throw runtime_error("此任务不能暂停 GUID:" + to_string(guid));
		else if (ptr->statues == task_info::waitting)
			scheduler.out(ptr);

		task_set_lock.lock();
		_DEBUG_OUT("task[%p] remove\n", ptr);
		task_set.erase(ptr);
		task_set_lock.unlock();

		delete ptr;
	}
	else
		throw invalid_argument("无效的GUID:" + to_string(guid));
}

vector<pair<size_t, uint32_t>> Internet_Downloader::get_new_recv()
{
	vector<pair<size_t, uint32_t>> temp_vec;

	task_set_lock.lock();
	for (auto x : task_set)
	{
		lock_guard<mutex> lock(x->_lock);
		if (x->statues == task_info::active)
			temp_vec.push_back({ (size_t)x, x->task_ptr->get_new_recv() });
	}
	task_set_lock.unlock();

	return temp_vec;
}

void Internet_Downloader::try_start(task_info *task)
{
	task->_lock.lock();
	if (task->statues == task_info::waitting || task->statues == task_info::pause)
	{
		_DEBUG_OUT("task[%p]:start\n", task);

		task->statues = task_info::active;
		task->start(this);
	}
	task->_lock.unlock();
}


void Internet_Downloader::notify(task_info*ptr, int evcode, void *parm, const char *msg)
{
	switch (evcode)
	{
		case (int)down_task_evcode::had_puase:
		{
			ptr->_lock.lock();

			_DEBUG_OUT("task[%p] had pause\n", ptr);

			ptr->statues = task_info::pause;
			auto x = scheduler.out(ptr);
			ptr->_lock.unlock();

			if (x != NULL)
				try_start(x);
			break;
		}
		case (int)down_task_evcode::receive_complete:
			ptr->statues = task_info::pending;
			_DEBUG_OUT("task[%p] receive complete\n", ptr);
			break;
		case (int)down_task_evcode::all_complete:
		{
			ptr->_lock.lock();

			_DEBUG_OUT("task[%p] finish\n", ptr);

			ptr->statues = task_info::finish;
			auto x = scheduler.out(ptr);
			delete ptr->task_ptr;
			ptr->task_ptr = NULL;

			ptr->_lock.unlock();

			if (x != NULL)
				try_start(x);
			break;
		}
		case (int)down_task_evcode::fail://delete task_ptr
		{
			ptr->_lock.lock();

			_DEBUG_OUT("task[%p] fail:%s\n", ptr, msg);

			ptr->statues = task_info::fail;
			auto x = scheduler.out(ptr);
			delete ptr->task_ptr;
			ptr->task_ptr = NULL;

			ptr->_lock.unlock();

			if (x != NULL)
				try_start(x);
			break;
		}
		default:
			util_err_exit("无效的事件");
			break;
	}
}
