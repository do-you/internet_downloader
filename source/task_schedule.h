#ifndef TASK_SCHEDULE_H
#define TASK_SCHEDULE_H

#include <set>
#include <mutex>

class Internet_Downloader;
struct task_info;

enum class schedule_evcode
{
	change_to_active
};

//class scheduleable
//{
//	friend class task_schedule;
//private:
//	enum
//	{
//		active,
//		waitting,
//		stopped
//	} state;
//};

class task_schedule
{
public:
	task_schedule(Internet_Downloader *parent, uint32_t n);

	void in_schedule(task_info* task);
	void out_of_schedule(task_info* task);
	void add(task_info* task);
	void remove(task_info* task);

	//get functions
	uint32_t get_activecount();
	uint32_t get_waittingcount();
	uint32_t get_stoppedcount();
	std::set<task_info*> get_activelist();
	std::set<task_info*> get_waittinglist();
	std::set<task_info*> get_stoppedlist();
protected:
	void schedule();

private:
	Internet_Downloader *parent;

	uint32_t max_active_count;

	std::set<task_info*> active_set, waitting_set, stopped_set;
	std::recursive_mutex schedule_mutex;
};

#endif // TASK_SCHEDULE_H