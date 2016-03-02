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

std::atomic_uint total_cache_size = ATOMIC_VAR_INIT(0);

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

	while (len > 0) {
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
			cache_list.push_back(buffer);
			cache_lock.unlock();

			//换新的
			buffer.buf = new char[piece_size];
			buffer.len = 0;
		}
	}

	recv_ptr += len_bac;
	//累加
	parent->cache_size.fetch_add(len_bac);
	uint32_t x = total_cache_size.fetch_add(len_bac);
	if (x < max_cache_size && x + len_bac >= max_cache_size)
		cache_controller.schedule();
}

void block::drain_all(uint32_t &had_drain)
{
	LARGE_INTEGER file_pos;
	file_pos.QuadPart = this->file_ptr;
	DWORD temp_len;
	std::list<util_buf> temp_list;

	SetFilePointerEx(parent->hfile, file_pos, NULL, FILE_BEGIN);
	//清空cache_list
	while (true) {
		cache_lock.lock();

		if (cache_list.empty()) {
			cache_lock.unlock();
			break;
		}
		else
			temp_list.swap(this->cache_list);

		cache_lock.unlock();

		for (auto &x : temp_list) {
			if (0 == WriteFile(parent->hfile, x.buf, x.len, &temp_len, NULL))
				util_errno_exit("回写文件出错");
			this->file_ptr += x.len;
			had_drain += x.len;
			parent->cache_size -= x.len;
			delete[] x.buf;
		}
		temp_list.clear();
	}
	//检测是否完成
	assert(this->recv_ptr <= this->end_ptr);
	if (this->recv_ptr >= this->end_ptr) {
		if (0 == WriteFile(parent->hfile, buffer.buf, buffer.len, &temp_len, NULL))
			util_errno_exit("回写文件出错");
		this->file_ptr += buffer.len;
		had_drain += buffer.len;
		parent->cache_size -= buffer.len;
		buffer.len = 0;

		parent->notify(this, (int)block_evcode::complete, NULL, NULL);
		delete this;
	}
}