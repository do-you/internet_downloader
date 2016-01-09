#include <stdint.h>
#include <assert.h>
#include <functional>
#include "file_block.h"

block_cache::block_cache(uint64_t a, uint64_t b)
{
	actual_ptr = base.ptr = a;
	base.end = b;
}

block_cache::block_cache(block &a)
{
	base = a;
	actual_ptr = a.ptr;
}

uint64_t block::len() const
{
	return ptr < end ? end - ptr : 0;
}



void file_block::write_to_file(char * buf, uint32_t len)
{
	while (len > 0)
	{
		if (piece_size - offset > len)//剩余空间足够
		{
			memcpy(buffer + offset, buf, len);
			offset += len;
			break;
		}
		else//不够
		{
			uint32_t cpylen;

			cpylen = piece_size - offset;
			memcpy(buffer + offset, buf, cpylen);
			buf += cpylen;
			len -= cpylen;

			cache_mutex.lock();

			cache_list.push_back({ buffer, piece_size });
			had_cache += piece_size;
			//旧的满了，换新的
			if (free_list.size() > 0)
			{
				buffer = free_list.front().buf;
				free_list.pop_front();
			}
			else
				buffer = new char[piece_size];
			offset = 0;

			cache_mutex.unlock();
		}
	}

	if (had_cache >= max_cache)
	{
		if (nqueue.fetch_add(had_cache) == 0)
		{
			// 			printf("cache 为 %u 时启动写文件\n", (free_list.size() + cache_list.size()) * 4);
			async_writefile(hfile, over, cache_list.front().buf, cache_list.front().len);
		}
		had_cache = 0;
	}
}



void file_block::flush()
{
	if (offset > 0)
	{
		cache_mutex.lock();

		cache_list.push_back({ buffer, offset });
		had_cache += offset;
		//旧的满了，换新的
		if (free_list.size() > 0)
		{
			buffer = free_list.front().buf;
			free_list.pop_front();
		}
		else
			buffer = new char[piece_size];
		offset = 0;

		cache_mutex.unlock();
	}

	if (had_cache > 0)
	{
		if (nqueue.fetch_add(had_cache) == 0)
		{
			// 			printf("cache 为 %d 时启动写文件\n", (free_list.size() + cache_list.size()) * 4);
			async_writefile(hfile, over, cache_list.front().buf, cache_list.front().len);
		}
		had_cache = 0;
	}
}

void file_block::auto_destruct()
{
	flush();
	terminal = true;
	if (nqueue == 0)
		delete this;
}

file_block::~file_block()
{
	assert(terminal);
	delete[] buffer;
	for (auto &x : cache_list)
		delete[] x.buf;
	for (auto &x : free_list)
		delete[] x.buf;

	cb(block);
}

file_block::file_block(block_cache *block, HANDLE hfile, uint32_t cachesize, write_complete_cb cb)
{

	max_cache = cachesize;
	this->block = block;
	this->hfile = hfile;
	this->cb = cb;

	memset(&over, 0, sizeof(over));
	over.remind_me = this;
	over.overlapped.Offset = block->actual_ptr;
	over.overlapped.OffsetHigh = block->actual_ptr >> 32;

	buffer = new char[piece_size];
	had_cache = offset = 0;
	nqueue = 0;
	terminal = false;
}

void file_block::complete_callback(int key, int nRecv, overlapped_base * over_base)
{
	cache_mutex.lock();

	auto x = cache_list.front();
	cache_list.pop_front();
	if ((free_list.size() + cache_list.size()) * 4096 >= max_cache)
		delete[] x.buf;
	else
		free_list.push_back(x);

	cache_mutex.unlock();

	//修改文件指针
	block->actual_ptr += nRecv;
	over.overlapped.Offset = block->actual_ptr;
	over.overlapped.OffsetHigh = block->actual_ptr >> 32;


	if ((nqueue -= nRecv) > 0)
		async_writefile(hfile, over, cache_list.front().buf, cache_list.front().len);
	else
	{
		// 		printf("cache 为 %d 时暂停写文件\n", (free_list.size() + cache_list.size()) * 4);
		if (terminal)
			delete this;
	}
}



