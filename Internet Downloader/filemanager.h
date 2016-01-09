#ifndef FILEMANAGER_H
#define FILEMANAGER_H

#include <WinSock2.h>
#include <windows.h>
#include <string>
#include <boost/filesystem.hpp>
#include <set>
#include <mutex>
#include <condition_variable>
#include "iocp_base.h"
#include "util_container.h"
#include "file_block.h"

using namespace boost::filesystem;

#define min_split_size 2*1024*1024 //2MB

using block_ptr = block*;

bool front_first(const block_cache &a, const block_cache &b);

class filemanager
{
public:
	filemanager(path &dir);
	~filemanager();
	void init(std::string &filename, uint64_t len);
	bool dynamic_allocate(block_ptr &ptr, uint32_t split_size);
	int  static_allocate(block_ptr ary[], int nsplit, uint32_t split_size);
	void block_complete(block_ptr ptr);
	void write_progress();
	file_block* create_block_manager(block_ptr blo, uint32_t cachesize);
	bool drain_all(){ return had_drain_all; }
	void set_path(std::string &p);

	void testsync();
protected:
	void file_init(const std::string &filename, int flag);
	bool parse_profile(size_t len);
	void write_cmplete(block_cache *blk);

public:
	bool had_init;
	bool had_drain_all;
	bool had_finish;

	path dir;

	std::string file_name;
	uint64_t file_len;

	HANDLE hfile;
	HANDLE hprofile;
	HANDLE hprofilemap;
	struct profile
	{
		uint64_t len;
		uint32_t nblock;
		block blocks[1];
	};
	char *profile_buf;

	std::set<block_cache, decltype(front_first)*> undistributed_queue;
	my_list<block_cache> distributed_list;
	my_list<block_cache> writing_back_list;

	std::mutex comp_mutex;
	std::condition_variable_any is_comp;
};

#endif // FILEMANAGER_H

// struct my_semaphore
// {
// 	int block_count = 0;
// 	mutex count_mutex;
// 	condition_variable_any release;
// 	//-1
// 	void decrease();
// 	//+1
// 	void increase();
// 	//
// 	void wait_utill_zero();
// };
// void my_semaphore::decrease()
// {
// 	count_mutex.lock();
// 	--block_count;
// 	release.notify_one();
// 	count_mutex.unlock();
// }
// 
// void my_semaphore::increase()
// {
// 	count_mutex.lock();
// 	++block_count;
// 	count_mutex.unlock();
// }
// 
// void my_semaphore::wait_utill_zero()
// {
// 	count_mutex.lock();
// 
// 	while (block_count != 0)
// 		release.wait(count_mutex);
// 
// 	count_mutex.unlock();
// }


