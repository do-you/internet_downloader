#ifndef FILE_MA_H
#define FILE_MA_H

#include <stdint.h>
#include <set>
#include <list>
#include <boost/filesystem.hpp>
#include <atomic>
#include "file_block.h"
using namespace boost::filesystem;

class down_task;
struct task_info;

bool front_first(block* a, block* b);

enum class file_evcode
{
	complete,
	all_drain
};

class file_ma
{
public:
	file_ma(down_task *parent, task_info *parms);
	~file_ma();

	std::list<block*> init();
	void notify(block *which, int evcode, void *parm, void *parm2);

	//状态函数
	uint32_t get_numPieces();
	std::string get_bitfield(uint32_t &remain_len);

protected:
	void file_init(path &path1, path &path2, int flag);
	void write_progress();

private:
	bool had_init;
	bool had_complete;

	down_task *parent;
	task_info *parms;

	HANDLE hfile;
	HANDLE hprofile;
	HANDLE hprofilemap;
	struct profile
	{
		uint64_t len;
		uint64_t nblock;
		ptr_pair blocks[1];
	};
	char *profile_buf;

	std::set<block*,decltype(front_first)*> block_set;
	std::mutex block_set_lock;
	std::atomic_uint cache_size;

private://由cache_control调用
	friend class cache_control;
	void do_write_back();
	uint32_t getcachesize();

private://由block调用
	friend class block;
	//由block构造时调用
	void check_in(block* p);
};

#endif // FILE_MA_H
