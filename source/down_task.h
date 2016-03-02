#ifndef DOWN_TASK_H
#define DOWN_TASK_H

#include <string>
#include <boost/filesystem.hpp>
#include <memory>
#include "net_io.h"
#include "file_ma.h"
#include "uri_parse.h"
using namespace std;
using namespace boost::filesystem;

class Internet_Downloader;


enum class down_task_evcode
{
	had_puase,
	receive_complete,
	all_complete,
	fail,
	init_complete
};

class down_task
{
public:
	down_task(Internet_Downloader *parent, task_info* info);

	void start();
	void pause();

	void init();

	void notify(file_ma *which, int evcode, void *parm, const char *msg);
	void notify(net_io *which, int evcode, void *parm, const char *msg);

	size_t get_guid();
	uint32_t get_new_recv();
protected:
private:
	Internet_Downloader *parent;
	task_info* info;
	shared_ptr<file_ma> progress;
	net_io  net_ma;

	// 	bool net_comp;
	bool had_init;
};

//构成一个完整的down_task所必要的信息
struct task_info
{
	void start(Internet_Downloader *parent);

	~task_info();//处理task_ptr

	down_task* task_ptr = NULL;

	enum
	{
		active,
		pending,
		waitting,
		pause,
		finish,
		fail
	}statues;
	std::mutex _lock;

	//net_parms的内容
	int split;
	uri_info info;

	unordered_map<string, string> user_header;

	//file_parms的内容
	int min_split_size;
	path dir;

	//公共
	string file_name;
	uint64_t file_len;

	task_info* next;
	task_info* pre;
};

#endif // DOWN_TASK_H
