#ifndef FILE_BLOCK
#define FILE_BLOCK

#include <stdint.h>
#include <list>
#include <mutex>
#include "utility.h"

using ptr_pair = std::pair<uint64_t, uint64_t>;

enum class block_evcode
{
	complete
};

class file_ma;
class block
{
	friend bool front_first(block* a, block* b);
public:
	block(file_ma *parent, ptr_pair p);
	~block();
	operator ptr_pair();

	std::list<block*> divide(uint32_t n, uint32_t min_split_size);
	uint64_t begin() const;
	uint64_t end() const;
	uint64_t len() const;

	void fill(const char* buf, uint32_t len);
	void drain_all(uint32_t &had_drain);

	block *next;
	block *pre;

private:
	file_ma *parent;

	util_buf buffer;

	uint64_t recv_ptr;
	uint64_t file_ptr;
	uint64_t end_ptr;

	std::list<util_buf> cache_list;
	std::mutex cache_lock;
};

using block_ptr = block*;
#endif // FILE_BLOCK
