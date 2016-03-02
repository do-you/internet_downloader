#ifndef CACHE_CONTROL_H
#define CACHE_CONTROL_H

#include <stdint.h>
#include <unordered_set>
#include <atomic>
#include <boost/thread.hpp>
#include <list>
#include <memory>
#include <condition_variable>

class block;
class file_ma;

class cache_control
{
public:
	cache_control();
	~cache_control();
	void check_in(std::shared_ptr<file_ma> fl);
	void set_async_check_out(std::shared_ptr<file_ma> fl);
	void schedule();
	void in_advance(std::shared_ptr<file_ma> fl);
protected:
	void drain();

private:
	bool terminal;
	bool should_drain;
	boost::thread drain_thread;

// 	using cache_ite = std::unordered_set<file_ma*>::const_iterator;
	std::unordered_set<std::shared_ptr<file_ma>> cache_set;

	std::mutex task_lock;
	std::mutex check_lock;
	std::condition_variable_any start_drain;
};


#endif // CACHE_CONTROL_H
