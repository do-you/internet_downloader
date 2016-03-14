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

	void check_in(std::shared_ptr<file_ma> &fl);
	void check_out(std::shared_ptr<file_ma> &fl);
	//一次性的
	void enqueue(std::shared_ptr<file_ma> &fl);

	void increase_cached_size(uint32_t n);
	void decrease_cached_size(uint32_t n);

protected:
	void drain();
	void schedule();

private:
	bool terminal;
	bool should_drain;
	boost::thread drain_thread;

	std::unordered_set<std::shared_ptr<file_ma>> cache_set;
	std::unordered_set<std::shared_ptr<file_ma>> priority_queue;

	std::mutex task_lock;
	std::condition_variable_any start_drain;

	std::atomic_uint total_cache_size;
};


#endif // CACHE_CONTROL_H
