#include <algorithm>
#include <assert.h>
#include <atomic>
#include <boost/thread.hpp>
#include "cache_control.h"
#include "file_block.h"
#include "global_def.h"
#include "file_ma.h"
#include "down_task.h"

cache_control cache_controller;
extern std::atomic_uint total_cache_size;


cache_control::cache_control()
{
	should_drain = terminal = false;
	drain_thread = boost::thread(&cache_control::drain, this);
}

cache_control::~cache_control()
{
	terminal = true;

	check_lock.lock();//lock
	should_drain = true;
	start_drain.notify_one();
	check_lock.unlock();//unlock

	drain_thread.join();
}

void cache_control::check_in(std::shared_ptr<file_ma> fl)
{
	task_lock.lock();//lock

	assert(cache_set.count(fl) == 0);
	cache_set.insert(fl);

	task_lock.unlock();//unlock
}

void cache_control::set_async_check_out(std::shared_ptr<file_ma> fl)
{
	fl->async_out = true;
}

void cache_control::schedule()
{
	check_lock.lock();

	start_drain.notify_one();

	check_lock.unlock();
}

void cache_control::in_advance(std::shared_ptr<file_ma> fl)
{
	task_lock.lock();
	fl->in_advance = true;
	task_lock.unlock();

	check_lock.lock();
	should_drain = true;
	start_drain.notify_one();
	check_lock.unlock();
}

void cache_control::drain()
{
	uint32_t had_drain = 0;

	while (true)
	{
		if (terminal)
			return;

		check_lock.lock();//lock

		if (should_drain == false && total_cache_size < max_cache_size)
			start_drain.wait(check_lock);
		should_drain = false;

		check_lock.unlock();//unlock

		if (terminal)
			return;

		task_lock.lock();//lock

		auto res = std::max_element(cache_set.begin(), cache_set.end(), [](std::shared_ptr<file_ma> a, std::shared_ptr<file_ma> b){
			if (a->in_advance)
				return false;
			else if (b->in_advance)
				return true;
			else
				return a->getcachesize() < b->getcachesize();
		});

		assert(res != cache_set.end());
		auto target = *res;
		if (target->async_out)
			cache_set.erase(res);

		task_lock.unlock();//unlock

		_DEBUG_OUT("task[%p] start drain\n", target->parent->get_guid());
		target->do_write_back(had_drain);
		_DEBUG_OUT("%u had drain\n", had_drain);
		total_cache_size -= had_drain;
		had_drain = 0;
	}
}