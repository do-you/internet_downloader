#ifndef INTERNET_DOWNLOADER_H
#define INTERNET_DOWNLOADER_H

#include <string>
#include <stdint.h>
#include <unordered_set>
#include <mutex>
#include <vector>
#include "down_task.h"
#include "util_container.h"
#include "task_schedule.h"

using namespace std;

struct down_parm
{
	string parm;
	string value;
};

struct conf
{
	string dir;
	uint32_t disk_cache;
	int max_concurrent_downloads;
	int min_split_size;
	int split;
};

class Internet_Downloader
{
public:
	Internet_Downloader();
	Internet_Downloader(conf &con);

	size_t add_task(uri_info info, list<down_parm> &parms);

	void start_task(size_t guid);
	void pause_task(size_t guid);
	void remove_task(size_t guid);

	vector<pair<size_t, uint32_t>> get_new_recv();

	void notify(task_info*ptr, int evcode, void *parm, const char *msg);


public:
	void getGlobalStat()
	{

	}

private:
	task_info* guid_convert(size_t guid);
	void try_start(task_info *task);

private:
	unordered_set<task_info*> task_set;
	std::mutex task_set_lock;

	conf configure;

	task_schedule scheduler;
};
#endif // INTERNET_DOWNLOADER_H
