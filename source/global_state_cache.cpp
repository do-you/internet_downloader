#include "global_state_cache.h"

#include <mutex>
#include <thread>
#include "json_rpc.h"
#include "Internet_Downloader.h"


global_state_cache::global_state_cache(Internet_Downloader *target)
{
	stopped = false;
	this->target = target;
	gather_thread = thread(&global_state_cache::do_gather, this);
}

global_state global_state_cache::getGlobalStat()
{
	std::lock_guard<std::mutex> _lock(data_lock);
	return summary;
}

global_option global_state_cache::getGlobalOption()
{
	return target->getGlobalOption();
}

std::list<task_state> global_state_cache::tellActive()
{
	std::lock_guard<std::mutex> _lock(data_lock);

	std::list<task_state> res;
	for (auto &x : active_list)
		res.push_back(x->second);
	return res;
}

std::list<task_state> global_state_cache::tellWaiting()
{
	std::lock_guard<std::mutex> _lock(data_lock);

	std::list<task_state> res;
	for (auto &x : waitting_list)
		res.push_back(x->second);
	return res;
}

std::list<task_state> global_state_cache::tellStopped()
{
	std::lock_guard<std::mutex> _lock(data_lock);

	std::list<task_state> res;
	for (auto &x : stopped_list)
		res.push_back(x->second);
	return res;
}

task_state global_state_cache::tellStatus(string &gid)
{
	std::lock_guard<std::mutex> _lock(data_lock);

	decltype(task_states)::iterator ite;
	if ((ite = task_states.find(gid)) != task_states.end())
		return ite->second;
	else
		throw invalid_argument(string("非法的gid:") + gid);
}

size_t global_state_cache::add_task(string uri, list<down_parm> &parms)
{
	return target->add_task(uri, parms);
}

void global_state_cache::start_task(size_t guid)
{
	target->start_task(guid);
}

void global_state_cache::pause_task(size_t guid)
{
	target->pause_task(guid);
}

void global_state_cache::remove_task(size_t guid)
{
	target->remove_task(guid);
}

void global_state_cache::do_gather()
{
	while (stopped == false)
	{
		uint32_t total_speed = 0, numactive = 0, numwait = 0, numstop = 0;

		data_lock.lock();
		//初始化
		task_states = target->getGlobalStat();
		active_list.clear();
		waitting_list.clear();
		stopped_list.clear();

		//更新
		for (auto x = task_states.begin(); x != task_states.end(); ++x)
		{
			if (x->second.status == "active")
			{
				active_list.push_back(x);
				total_speed += stoul(x->second.downloadSpeed);
				++numactive;

				printf("task[%s]:speed %.2lf KB/s\n", x->second.gid.c_str(), stoul(x->second.downloadSpeed) / 1024.0);
			}
			else if (x->second.status == "waiting" || x->second.status == "paused")
			{
				waitting_list.push_back(x);
				++numwait;
			}
			else
			{
				stopped_list.push_back(x);
				++numstop;
			}
		}
		summary.downloadSpeed = to_string(total_speed);
		summary.numActive = to_string(numactive);
		summary.numStopped = summary.numStoppedTotal = to_string(numstop);
		summary.numWaiting = to_string(numwait);

		data_lock.unlock();


		std::this_thread::sleep_for(1s);
	}
}