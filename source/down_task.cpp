#include "down_task.h"
#include "Internet_Downloader.h"
#include "cache_control.h"

extern cache_control cache_controller;

down_task::down_task(Internet_Downloader *parent, task_info* info) :progress(new file_ma(this, info)), net_ma(this, info)
{
	this->parent = parent;
	this->info = info;
	had_init = false;
}

void down_task::start()
{
	assert(had_init);

	net_ma.start();
}

void down_task::pause()
{
	net_ma.pause();
}

void down_task::init()
{
	net_ma.async_get_file_info();
}

void down_task::notify(net_io *which, int evcode, void *parm, const char *msg)
{
	switch (evcode)
	{
		case (int)net_evcode::had_puase:
			cache_controller.in_advance(progress);
			parent->notify(info, (int)down_task_evcode::had_puase, NULL, NULL);
			break;
		case (int)net_evcode::get_file_info:
		{
			auto res = progress->init();
			if (!res.empty())
			{
				cache_controller.check_in(progress);
				net_ma.init(std::move(res));
				this->had_init = true;
				//不是必要的事件
				//parent->notify(info, (int)down_task_evcode::init_complete, NULL, NULL);

				this->start();
			}
			else//文件已完成
				parent->notify(info, (int)down_task_evcode::all_complete, NULL, NULL);
			break;
		}
		case (int)net_evcode::complete:
			cache_controller.set_async_check_out(progress);
			cache_controller.in_advance(progress);
			parent->notify(info, (int)down_task_evcode::receive_complete, NULL, NULL);
			break;
		case (int)net_evcode::fail:
			cache_controller.set_async_check_out(progress);
			cache_controller.in_advance(progress);
			parent->notify(info, (int)down_task_evcode::fail, parm, msg);
			break;
		default:
			util_err_exit("无效的事件");
			break;
	}
}

void down_task::notify(file_ma *which, int evcode, void *parm, const char *msg)
{
	switch (evcode)
	{
		case (int)file_evcode::complete:
			parent->notify(info, (int)down_task_evcode::all_complete, NULL, NULL);
			break;
		default:
			util_err_exit("无效的事件");
			break;
	}
}

size_t down_task::get_guid()
{
	return (size_t)info;
}

uint32_t down_task::get_new_recv()
{
	return net_ma.get_new_recv();
}

void task_info::start(Internet_Downloader *parent)
{
	if (task_ptr == NULL)
	{
		task_ptr = new down_task(parent, this);
		task_ptr->init();
	}
	else
		task_ptr->start();
}

task_info::~task_info() //处理task_ptr
{
	if (task_ptr != NULL)
		delete task_ptr;
}
