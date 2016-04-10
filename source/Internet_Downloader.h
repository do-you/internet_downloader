#ifndef INTERNET_DOWNLOADER_H
#define INTERNET_DOWNLOADER_H

#include <stdint.h>
#include <unordered_set>
#include <mutex>
#include <vector>
#include "down_task.h"
#include "task_schedule.h"
#include "json_rpc.h"

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
	~Internet_Downloader();
	explicit Internet_Downloader(conf &con);

	size_t add_task(string uri, list<down_parm> &parms);

	void start_task(size_t guid);
	void pause_task(size_t guid);
	void remove_task(size_t guid);

	vector<pair<size_t, uint32_t>> get_new_recv();

	void notify(task_info*ptr, int evcode, void *parm, const char *msg);
	void notify(task_schedule *ptr, int evcode, task_info *parm, const char *msg);

	std::map<std::string, task_state> getGlobalStat();
	global_option getGlobalOption();

private:
	task_info* guid_convert(size_t guid);

private:
	unordered_set<task_info*> task_set;
	recursive_mutex task_set_lock;

	conf configure;

	task_schedule scheduler;
};
#endif // INTERNET_DOWNLOADER_H
