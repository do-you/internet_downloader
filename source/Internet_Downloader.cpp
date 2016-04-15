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

static conf global_conf = { u8"F:\\文件传输测试", 16 * 1024 * 1024, 4, 2 * 1024 * 1024, 4 };


Internet_Downloader::Internet_Downloader() :Internet_Downloader(global_conf) {}

Internet_Downloader::~Internet_Downloader()
{
	for (auto x : task_set)
		delete x;
}

Internet_Downloader::Internet_Downloader(conf &con) : configure(con), scheduler(this, con.max_concurrent_downloads) {}

size_t Internet_Downloader::add_task(string uri, list<down_parm> &parms)
{
	uri_info info;
	if (parseUrl(uri, info) == false)
		throw invalid_argument(string("非法的uri:") + uri);

	unique_ptr<task_info> new_task(new task_info);
	new_task->info = info;
	new_task->file_name = getfilename(info);
	new_task->dir = configure.dir;
	new_task->split = configure.split;
	new_task->min_split_size = configure.min_split_size;
	new_task->statues = task_info::pause;
	new_task->file_len = 0;
	new_task->bitfile = "0";
	new_task->complete_length = 0;

	for (auto &x : parms)
	{
		auto res = search_opt(x.parm);

		if (result_count(res) == 0)
			;//throw  invalid_argument("无效的参数:" + x.parm);
		else if (result_count(res) == 1)
			res.first->process(new_task.get(), x.value);
		else
			throw  invalid_argument("参数不唯一:" + x.parm);
	}

	new_task->statues = task_info::pause;
	scheduler.add(new_task.get());

	task_set_lock.lock();
	task_set.insert(new_task.get());
	task_set_lock.unlock();

	_DEBUG_OUT("task[%p] create\n", new_task.get());
	start_task((size_t)new_task.get());//start a task when add

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
		lock_guard<recursive_mutex> lock(ptr->_lock);

		if (ptr->statues == task_info::pause)
		{
			ptr->statues = task_info::waitting;
			scheduler.in_schedule(ptr);
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
		lock_guard<recursive_mutex> lock(ptr->_lock);

		switch (ptr->statues)
		{
			case task_info::pending:
				throw runtime_error("暂不支持暂停pending中的任务 GUID:" + to_string(guid));
			case task_info::active:
				ptr->statues = task_info::pending;
				ptr->task_ptr->pause();
				break;
			case task_info::waitting:
				_DEBUG_OUT("task[%p] pause\n", ptr);
				scheduler.out_of_schedule(ptr);
				ptr->statues = task_info::pause;
				break;
			case task_info::pause:
			case task_info::finish:
			case task_info::fail:
				throw runtime_error("非法操作 GUID:" + to_string(guid));
		}
	}
	else
		throw invalid_argument("无效的GUID:" + to_string(guid));
}

void Internet_Downloader::remove_task(size_t guid)
{
	task_info *ptr;
	if (ptr = guid_convert(guid))
	{
		lock(task_set_lock, ptr->_lock);

		switch (ptr->statues)
		{
			case task_info::active:
			case task_info::pending:
				task_set_lock.unlock();
				ptr->_lock.unlock();
				throw runtime_error("活动中的任务不能删除 GUID:" + to_string(guid));
			case task_info::waitting:
				scheduler.out_of_schedule(ptr);
			case task_info::pause:
			case task_info::finish:
			case task_info::fail:
				_DEBUG_OUT("task[%p] remove\n", ptr);
				task_set.erase(ptr);
				task_set_lock.unlock();
				scheduler.remove(ptr);
				ptr->_lock.unlock();
				delete ptr;
				break;
		}
	}
	else
		throw invalid_argument("无效的GUID:" + to_string(guid));
}

vector<pair<size_t, uint32_t>> Internet_Downloader::get_new_recv()
{
	vector<pair<size_t, uint32_t>> temp_vec;

	auto active_set = scheduler.get_activelist();
	for (auto x : active_set)
	{
		lock_guard<recursive_mutex> lock(x->_lock);
		temp_vec.push_back({ (size_t)x, x->task_ptr->get_new_recv() });
	}

	return temp_vec;
}

void Internet_Downloader::notify(task_info*ptr, int evcode, void *parm, const char *msg)
{
	lock_guard<recursive_mutex> lock(ptr->_lock);

	switch (evcode)
	{
		case (int)down_task_evcode::had_puase:
		{
			_DEBUG_OUT("task[%p] had pause\n", ptr);

			ptr->statues = task_info::pause;
			scheduler.out_of_schedule(ptr);

			break;
		}
		case (int)down_task_evcode::receive_complete:
			ptr->statues = task_info::pending;
			_DEBUG_OUT("task[%p] receive complete\n", ptr);
			break;
		case (int)down_task_evcode::all_complete:
		{
			_DEBUG_OUT("task[%p] finish\n", ptr);
			ptr->statues = task_info::finish;
			scheduler.out_of_schedule(ptr);

			uint32_t remain_len = 0;
			ptr->bitfile = ptr->task_ptr->get_bitfield(remain_len);
			ptr->complete_length = ptr->file_len - remain_len;
			delete ptr->task_ptr;
			ptr->task_ptr = NULL;

			break;
		}
		case (int)down_task_evcode::fail://delete task_ptr
		{
			_DEBUG_OUT("task[%p] fail:%s\n", ptr, msg);
			ptr->statues = task_info::fail;
			scheduler.out_of_schedule(ptr);

			uint32_t remain_len = 0;
			ptr->bitfile = ptr->task_ptr->get_bitfield(remain_len);
			ptr->complete_length = ptr->file_len - remain_len;
			delete ptr->task_ptr;
			ptr->task_ptr = NULL;

			break;
		}
		default:
			util_err_exit("无效的事件");
			break;
	}
}

void Internet_Downloader::notify(task_schedule *ptr, int evcode, task_info *parm, const char *msg)//可能要加锁
{
	switch (evcode)
	{
		case (int)schedule_evcode::change_to_active://要判断是否还在task_set里面
		{
			lock_guard<recursive_mutex> lock(parm->_lock);

			_DEBUG_OUT("task[%p] start\n", ptr);
			parm->statues = task_info::active;
			parm->start(this);
			break;
		}
		default:
			break;
	}
}

std::map<std::string, task_state> Internet_Downloader::getGlobalStat()
{
	task_set_lock.lock();
	unordered_set<task_info*> temp_set = task_set;
	task_set_lock.unlock();
// 	auto temp_set = scheduler.get_activelist();

	std::map<std::string, task_state> res;
	for (auto task : temp_set)
	{
		lock_guard<recursive_mutex> _lock(task->_lock);

		task_state per_res;
		uint32_t remainlen = 0;

		string gid_bac = per_res.gid = decimal_to_hex((uint64_t)task);
		//file
		per_res.dir = task->dir;
		per_res.files.index = "0";
		per_res.files.length = to_string(task->file_len);
		per_res.files.path = task->dir + string("\\") + task->file_name;
		per_res.files.selected = "true";
		per_res.pieceLength = to_string(task->min_split_size);
		per_res.totalLength = to_string(task->file_len);
		//net
		per_res.files.uris.push_back({ "used",task->info.host + string("/") + task->info.urlpath });

		switch (task->statues)
		{
			case task_info::active:per_res.status = "active"; break;
			case task_info::pending: per_res.status = "active"; break;
			case task_info::waitting: per_res.status = "waiting "; break;
			case task_info::pause: per_res.status = "paused"; break;
			case task_info::finish: per_res.status = "complete"; break;
			case task_info::fail: per_res.status = "error"; break;
		}

		if (task->task_ptr != NULL)
		{
			per_res.bitfield = task->task_ptr->get_bitfield(remainlen);
			per_res.completedLength = per_res.files.completedLength = to_string(task->file_len - remainlen);
			per_res.numPieces = to_string(task->task_ptr->get_numPieces());
			per_res.connections = to_string(task->task_ptr->get_split_count());
			per_res.downloadSpeed = to_string(task->task_ptr->get_new_recv());
		}
		else
		{
			per_res.bitfield = task->bitfile;
			per_res.completedLength = per_res.files.completedLength = to_string(task->complete_length);
			per_res.numPieces = "0";
			per_res.connections = "0";
			per_res.downloadSpeed = "0";
		}

		res.emplace(std::move(gid_bac), std::move(per_res));
	}

	return res;
}

global_option Internet_Downloader::getGlobalOption()
{
	global_option opt;
	opt.dir = configure.dir;
	opt.max_concurrent_downloads = to_string(configure.max_concurrent_downloads);
	opt.split = to_string(configure.split);
	opt.min_split_size = to_string(configure.min_split_size);
	opt.user_agent = "tinydown/3.0";

	return opt;
}