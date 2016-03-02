#ifndef NET_IO_H
#define NET_IO_H

#include <forward_list>
#include <functional>
#include <string>
#include <unordered_map>
#include <boost/filesystem.hpp>
#include <mutex>
#include <atomic>
#include "uri_parse.h"
#include "util_container.h"
#include "connection.h"
using namespace std;
using namespace boost::filesystem;

class down_task;
struct task_info;

enum class net_evcode
{
	had_puase,
	get_file_info,
	complete,
	fail
};

class net_io
{
public:
	net_io(down_task *parent, task_info *parms);

	void start();
	void pause();

	int  get_split_count();

	void notify(connection *which, int evcode, void *parm, const char *msg);

	void async_get_file_info();

	void init(std::list<block*> block_list);

	void submit_recv(uint32_t n);
	uint32_t get_new_recv();

protected:
	connection* split();

private:
	bool had_init;
	bool can_split;
	bool in_pause;

	atomic_uint pause_discout;
	atomic_uint new_recv;

	list<block*> alloc_list;

	unordered_map<connection*, block*> connection_map;
	mutex connection_map_lock;

	task_info *parms;
	down_task *parent;
};


#endif // NET_IO_H
