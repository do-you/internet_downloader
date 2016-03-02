#ifndef TASK_SCHEDULE_H
#define TASK_SCHEDULE_H

#include <list>
#include <unordered_map>
#include <mutex>
#include "down_task.h"

class task_schedule
{
public:
	task_schedule(uint32_t n);

	bool in(task_info* task);
	task_info* out(task_info* task);
private:
	uint32_t max_active_count;
	uint32_t current_active_count;

	std::list<task_info*> waiting_queue;
	std::unordered_map<task_info*, std::list<task_info*>::iterator> task_map;

	std::mutex schedule_mutex;
};

#endif // TASK_SCHEDULE_H


