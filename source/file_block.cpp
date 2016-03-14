#include <WinSock2.h>
#include <windows.h>
#include <algorithm>
#include <list>
#include <assert.h>
#include "file_block.h"
#include "file_ma.h"
#include "global_def.h"
#include "cache_control.h"

#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif

extern cache_control cache_controller;

block::block(file_ma *parent, ptr_pair p)
{
	this->parent = parent;

	recv_ptr = file_ptr = p.first;
	end_ptr = p.second;

	buffer.buf = new char[piece_size];
	buffer.len = 0;

	next = pre = nullptr;

	parent->check_in(this);
}

block::~block()
{
	assert(buffer.len >= 0);
	if (buffer.len > 0)
	{
		LARGE_INTEGER file_pos;
		DWORD temp_len;
		file_pos.QuadPart = this->file_ptr;
		SetFilePointerEx(parent->hfile, file_pos, NULL, FILE_BEGIN);

		if (0 == WriteFile(parent->hfile, buffer.buf, buffer.len, &temp_len, NULL))
			util_errno_exit("回写文件出错");
		assert(buffer.len == temp_len);

		this->file_ptr += buffer.len;
		parent->cache_size -= buffer.len;
		cache_controller.decrease_cached_size(buffer.len);

		buffer.len = 0;
	}
	delete[] buffer.buf;
}

block::operator ptr_pair()
{
	return{ file_ptr, end_ptr };
}

std::list<block *> block::divide(uint32_t n, uint32_t min_split_size)
{
	std::list<block *> temp;
	uint64_t recv_ptr_bac = recv_ptr;

	assert(n > 0);

	uint32_t block_count = min((end_ptr - recv_ptr_bac) / min_split_size, n);

	assert(block_count >= 0);

	if (block_count > 1) {
		uint64_t piece_len = (end_ptr - recv_ptr_bac) / block_count;

		uint64_t end_ptr_bac = end_ptr;
		end_ptr = recv_ptr_bac + piece_len;
		--block_count;

		auto pre = this;
		for (uint32_t i = 0; i < block_count; ++i) {
			auto now = new block(parent, { pre->end_ptr, pre->end_ptr + piece_len });
			temp.push_back(now);
			pre = now;
		}
		pre->end_ptr = end_ptr_bac;
	}
	return temp;
}

uint64_t block::begin() const
{
	return recv_ptr;
}

uint64_t block::end() const
{
	return end_ptr;
}

uint64_t block::len() const
{
	return end_ptr > recv_ptr ? end_ptr - recv_ptr : 0;
}

void block::fill(const char *buf, uint32_t len)
{
	uint32_t len_bac = len;

	assert(len > 0);

	while (len > 0)
	{
		if (piece_size - buffer.len > len)//剩余空间足够
		{
			memcpy(buffer.buf + buffer.len, buf, len);
			buffer.len += len;
			break;
		}
		else//不够或刚好
		{
			uint32_t cpylen;

			cpylen = piece_size - buffer.len;
			memcpy(buffer.buf + buffer.len, buf, cpylen);
			buf += cpylen;
			len -= cpylen;
			buffer.len += cpylen;

			assert(buffer.len == piece_size);

			cache_lock.lock();
			cache_list.emplace_back(buffer.buf);
			cache_lock.unlock();

			//换新的
			buffer.buf = new char[piece_size];
			buffer.len = 0;
		}
	}

	recv_ptr += len_bac;
	//累加
	parent->cache_size.fetch_add(len_bac);
	cache_controller.increase_cached_size(len_bac);
}

void block::drain_all()
{
	LARGE_INTEGER file_pos;
	file_pos.QuadPart = this->file_ptr;
	DWORD temp_len;
	uint32_t drain_len = 0;
	bool comp = false;
	std::list<std::unique_ptr<char[]>> temp_list;

	SetFilePointerEx(parent->hfile, file_pos, NULL, FILE_BEGIN);

	//清空cache_list
	while (true)
	{
		cache_lock.lock();

		if (cache_list.empty())
		{
			cache_lock.unlock();
			break;
		}
		else
			temp_list.swap(this->cache_list);

		cache_lock.unlock();

		while (!temp_list.empty())
		{
			auto &x = temp_list.front();

			if (0 == WriteFile(parent->hfile, x.get(), piece_size, &temp_len, NULL))
				util_errno_exit("回写文件出错");
			assert(piece_size == temp_len);

			this->file_ptr += piece_size;
			drain_len += piece_size;

			temp_list.pop_front();
		}
	}

	//检测是否完成
	assert(this->recv_ptr <= this->end_ptr);

	if (this->recv_ptr >= this->end_ptr)
	{
		comp = true;
		if (buffer.len > 0)
		{
			if (0 == WriteFile(parent->hfile, buffer.buf, buffer.len, &temp_len, NULL))
				util_errno_exit("回写文件出错");
			assert(buffer.len == temp_len);

			this->file_ptr += buffer.len;
			drain_len += buffer.len;
			buffer.len = 0;
		}
	}

	parent->cache_size -= drain_len;
	cache_controller.decrease_cached_size(drain_len);

	if (comp)
	{
		parent->notify(this, (int)block_evcode::complete, NULL, NULL);
		delete this;
	}
}