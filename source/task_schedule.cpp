#include "task_schedule.h"

#include <stdexcept>
#include <assert.h>
#include "Internet_Downloader.h"

task_schedule::task_schedule(Internet_Downloader *parent, uint32_t n)
{
	this->parent = parent;
	max_active_count = n;
}

void task_schedule::in_schedule(task_info* task)
{
	std::set<task_info*>::iterator pos;
	lock_guard<recursive_mutex> _lock(schedule_mutex);

	assert(active_set.count(task) + waitting_set.count(task) == 0);

	if ((pos = stopped_set.find(task)) != stopped_set.end())
	{
		auto x = *pos;
		if (waitting_set.insert(x).second)
		{
			stopped_set.erase(pos);
			schedule();
		}
		else
			throw invalid_argument("已在队列");
	}
	else
		throw invalid_argument("非法操作");
}

void task_schedule::out_of_schedule(task_info* task)
{
	std::set<task_info*>::iterator pos;
	lock_guard<recursive_mutex> _lock(schedule_mutex);

	assert(stopped_set.count(task) == 0);
	assert(active_set.count(task) + waitting_set.count(task) == 1);

	if ((pos = active_set.find(task)) != active_set.end())
	{
		auto x = *pos;
		active_set.erase(pos);
		stopped_set.insert(x);
		schedule();
	}
	else if ((pos = waitting_set.find(task)) != waitting_set.end())
	{
		auto x = *pos;
		waitting_set.erase(pos);
		stopped_set.insert(x);
	}
	else
		throw invalid_argument("非法的操作");
}

void task_schedule::schedule()
{
	if (active_set.size() < max_active_count && waitting_set.size() > 0)
	{
		auto x = *waitting_set.begin();
		waitting_set.erase(x);
		active_set.insert(x);
		parent->notify(this, (int)schedule_evcode::change_to_active, x, NULL);
	}
}
void task_schedule::add(task_info* task)
{
	lock_guard<recursive_mutex> _lock(schedule_mutex);

	assert(active_set.count(task) + waitting_set.count(task) + stopped_set.count(task) == 0);

	if (stopped_set.insert(task).second == false)
		throw invalid_argument("非法操作");
}

void task_schedule::remove(task_info* task)
{
	std::set<task_info*>::iterator pos;
	lock_guard<recursive_mutex> _lock(schedule_mutex);

	assert(active_set.count(task) + waitting_set.count(task) + stopped_set.count(task) == 1);

	if ((pos = stopped_set.find(task)) != stopped_set.end())
		stopped_set.erase(pos);
	else
	{
		out_of_schedule(task);
		stopped_set.erase(pos);
	}
}

uint32_t task_schedule::get_activecount()
{
	return active_set.size();
}

uint32_t task_schedule::get_waittingcount()
{
	return waitting_set.size();
}

uint32_t task_schedule::get_stoppedcount()
{
	return stopped_set.size();
}

std::set<task_info*> task_schedule::get_activelist()
{
	std::lock_guard<std::recursive_mutex> _lock(schedule_mutex);
	return active_set;
}

std::set<task_info*> task_schedule::get_waittinglist()
{
	std::lock_guard<std::recursive_mutex> _lock(schedule_mutex);
	return waitting_set;
}

std::set<task_info*> task_schedule::get_stoppedlist()
{
	std::lock_guard<std::recursive_mutex> _lock(schedule_mutex);
	return stopped_set;
}
