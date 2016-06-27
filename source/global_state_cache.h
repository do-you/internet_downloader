#ifndef global_state_cache_H
#define global_state_cache_H

#include <map>
#include <thread>
#include <mutex>
#include "json_rpc.h"

class Internet_Downloader;

class global_state_cache :public rpc_base
{
public:
	global_state_cache(Internet_Downloader *target);

	virtual global_state getGlobalStat() override;
	virtual global_option getGlobalOption() override;
	virtual std::list<task_state> tellActive() override;
	virtual std::list<task_state> tellWaiting() override;
	virtual std::list<task_state> tellStopped() override;
	virtual task_state tellStatus(string gid) override;

	virtual size_t add_task(string uri, list<down_parm> &parms) override;
	virtual void start_task(size_t guid)override;
	virtual void pause_task(size_t guid)override;
	virtual void remove_task(size_t guid)override;

private:
	void do_gather();

private:
	std::map<std::string, task_state> task_states;
	std::list<decltype(task_states)::iterator> active_list, waitting_list, stopped_list;
	global_state summary;

	std::mutex data_lock;

	std::thread  gather_thread;

	bool stopped;
	Internet_Downloader *target;
};

#endif // global_state_cache_H

