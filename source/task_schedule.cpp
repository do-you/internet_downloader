#include "task_schedule.h"


task_schedule::task_schedule(uint32_t n)
{
	max_active_count = n;
	current_active_count = 0;
}

bool task_schedule::in(task_info* task)
{
	std::lock_guard<std::mutex> lock(schedule_mutex);

	if (current_active_count < max_active_count)
		return true;
	else
	{
		auto x = waiting_queue.insert(waiting_queue.end(), task);
		task_map.insert({ task, x });
		return false;
	}
}

task_info* task_schedule::out(task_info* task)
{
	std::lock_guard<std::mutex> lock(schedule_mutex);

	auto x = task_map.find(task);
	if (x != task_map.end())
	{
		waiting_queue.erase(x->second);
		task_map.erase(x);
		return NULL;
	}
	else
	{
		if (waiting_queue.empty())
			return NULL;
		else
		{
			auto x = waiting_queue.front();
			waiting_queue.pop_front();
			task_map.erase(x);
			return x;
		}
	}
}
