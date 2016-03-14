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

cache_control::cache_control()
{
	should_drain = terminal = false;
	drain_thread = boost::thread(&cache_control::drain, this);
}

cache_control::~cache_control()
{
	task_lock.lock();//lock

	terminal = true;
	start_drain.notify_one();

	task_lock.unlock();//unlock

	drain_thread.join();
}

void cache_control::check_in(std::shared_ptr<file_ma> &fl)
{
	task_lock.lock();//lock

	assert(cache_set.count(fl) == 0);
	cache_set.insert(fl);

	task_lock.unlock();//unlock
}

void cache_control::check_out(std::shared_ptr<file_ma> &fl)
{
	task_lock.lock();//lock

	assert(cache_set.count(fl) == 1);
	cache_set.erase(fl);

	task_lock.unlock();//unlock
}

void cache_control::schedule()
{
	task_lock.lock();

	start_drain.notify_one();

	task_lock.unlock();
}

void cache_control::enqueue(std::shared_ptr<file_ma> &fl)
{
	task_lock.lock();

	assert(priority_queue.count(fl) == 0);
	priority_queue.insert(fl);
	start_drain.notify_one();

	task_lock.unlock();
}

void cache_control::increase_cached_size(uint32_t n)
{
	auto x = total_cache_size.fetch_add(n);
	if (x < max_cache_size && x + n >= max_cache_size)
		cache_controller.schedule();
}

void cache_control::decrease_cached_size(uint32_t n)
{
	total_cache_size -= n;
}

void cache_control::drain()
{
	while (true)
	{
		std::shared_ptr<file_ma> target;

		while (true)
		{
			lock_guard<std::mutex> _lock(task_lock);

			if (terminal)
				return;

			if (priority_queue.empty())
			{
				if (total_cache_size < max_cache_size)
					start_drain.wait(task_lock);
				else
				{
					auto res = std::max_element(cache_set.begin(), cache_set.end(), [](const std::shared_ptr<file_ma> &a, const std::shared_ptr<file_ma> &b){
						return a->getcachesize() < b->getcachesize();
					});
					if (res != cache_set.end())
					{
						target = *res;
						break;
					}
				}
			}
			else
			{
				auto x = priority_queue.begin();
				target = std::move(*x);
				priority_queue.erase(x);
				break;
			}
		}

		assert(target);

		_DEBUG_OUT("task[%p] start drain,%u had cached\n", target->parent->get_guid(), total_cache_size);
		target->do_write_back();
		_DEBUG_OUT("%u left\n", total_cache_size);
	}
}