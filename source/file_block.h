#ifndef FILE_BLOCK
#define FILE_BLOCK

#include <stdint.h>
#include <list>
#include <functional>
#include <atomic>
#include <mutex>
#include "iocp_base.h"

#define write_complete_cb std::function<void(block_cache*)>

struct file_overlapped
{
	overlapped_base over_base;
	char *buf;
};
struct td_buf
{
	char* buf;
	uint32_t len;
};
struct block
{
	uint64_t ptr;
	uint64_t end;

	uint64_t len() const;
};
struct block_cache
{
	block base;
	uint64_t actual_ptr;
	block_cache *next, *pre;

	block_cache(uint64_t a, uint64_t b);
	block_cache(block &a);
	block_cache() = default;
};

class file_block :public iocp_base
{
#define piece_size 4096
public:
	friend class file_io;

	void write_to_file(char * buf, uint32_t len);
	void flush();

	void auto_destruct();
	~file_block();
protected:
	file_block(block_cache *block, HANDLE hfile, uint32_t cachesize, write_complete_cb cb);

	// Í¨¹ý iocp_base ¼Ì³Ð
	virtual void complete_callback(int key, int nRecv, overlapped_base * over_base) override;

private:
	HANDLE hfile;
	block_cache *block;
	overlapped_base over;

	char *buffer;
	uint32_t offset;
	std::list<td_buf> cache_list;
	std::list<td_buf> free_list;
	uint32_t max_cache;
	uint32_t had_cache;
	std::atomic_uint nqueue;

	std::mutex cache_mutex;

	write_complete_cb cb;

	bool terminal;
};

#endif // FILE_BLOCK
