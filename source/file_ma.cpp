#include <boost/filesystem.hpp>
#include <boost/locale.hpp>
#include <string>
#include <atomic>
#include "file_ma.h"
#include "utility.h"
#include "down_task.h"
#include "cache_control.h"
using namespace std;
using namespace boost::filesystem;

// extern cache_control cache_controller;

bool front_first(block* a, block* b)
{
	return a == b ? false : a->end_ptr < b->end_ptr;
}

file_ma::file_ma(down_task *parent, task_info *parms) :block_set(front_first)
{
	this->parent = parent;
	this->parms = parms;

	had_complete = had_init = false;
}

file_ma::~file_ma()
{
	_DEBUG_OUT("----------------------file_ma destruct\n");

	if (had_init)
	{
		for (auto x : block_set)
			delete x;

		if (0 == UnmapViewOfFile(profile_buf))
			util_errno_exit("UnmapViewOfFile fail:");
		if (0 == CloseHandle(hprofilemap))
			util_errno_exit("CloseHandle fail:");
		if (0 == CloseHandle(hprofile))
			util_errno_exit("CloseHandle fail:");
		if (0 == CloseHandle(hfile))
			util_errno_exit("CloseHandle fail:");
		if (had_complete)
		{
			path temp = boost::locale::conv::utf_to_utf<wchar_t>(parms->dir);
			temp /= parms->file_name + ".abc";
			remove(temp);
		}
	}

	_DEBUG_OUT("%u had left", cache_size.load());
}

std::list<block*> file_ma::init()
{
	std::list<block*> temp_list;
	std::string name, ext;
	int pos, i = 0;
	path temppath, temppath2;

	pos = parms->file_name.rfind('.');
	name = parms->file_name.substr(0, pos);
	if (pos != std::string::npos)
		ext = parms->file_name.substr(pos);

	while (++i)
	{
		temppath2 = temppath = boost::locale::conv::utf_to_utf<wchar_t>(parms->dir);
		bool file_exit = exists(temppath /= parms->file_name);
		bool record_exit = exists(temppath2 /= parms->file_name + ".abc");
		if (!file_exit)//文件不存在
		{
			//直接新建
			file_init(temppath, temppath2, CREATE_NEW);
			temp_list.push_back(new block(this, { 0, parms->file_len }));
			break;
		}
		else//文件存在
		{
			if (file_size(temppath) == parms->file_len)//长度对上
			{
				if (!record_exit)//记录文件不存在					
					break;//表示下载完成
				else
				{
					file_init(temppath, temppath2, OPEN_EXISTING);
					//读取记录文件
					profile *prof = (profile*)profile_buf;

					if (prof->len != parms->file_len /*|| filelen < prof->nblock*sizeof(ptr_pair) + 12*/)//格式错误
					{
						temp_list.push_back(new block(this, { 0, parms->file_len }));
						break;
					}

					if (prof->nblock == 0)
						break;//表示下载完成

					for (uint32_t i = 0; i < prof->nblock; ++i)
						temp_list.push_back(new block(this, prof->blocks[i]));

					break;
				}
			}
			else//长度对不上
			{
				//加个编号继续尝试
				parms->file_name = name + to_string(i) + ext;
			}
		}
	}

	return temp_list;
}

void file_ma::write_progress()
{
	assert(had_init);

	if (!had_complete && had_init)
	{
		_DEBUG_OUT("write progress\n");

		profile *prof = (profile*)profile_buf;
		prof->len = parms->file_len;

		block_set_lock.lock();

		prof->nblock = block_set.size();
		int i = 0;
		for (auto x : block_set)
			prof->blocks[i++] = *x;

		block_set_lock.unlock();
	}
}

void file_ma::check_in(block* p)
{
	block_set_lock.lock();

	assert(block_set.count(p) == 0);
	block_set.insert(p);

	block_set_lock.unlock();
}

void file_ma::file_init(path &path1, path &path2, int flag)
{
	hfile = CreateFile(path1.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, flag, FILE_ATTRIBUTE_NORMAL, NULL);
	if (INVALID_HANDLE_VALUE == hfile)
		util_errno_exit("CreateFileA fail:");

	hprofile = CreateFile(path2.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, flag, FILE_ATTRIBUTE_NORMAL, NULL);
	if (INVALID_HANDLE_VALUE == hprofile)
		util_errno_exit("CreateFileA fail:");

	//文件预分配
	if (flag == CREATE_NEW)
	{
		if (0 == SetFilePointerEx(hfile, *((LARGE_INTEGER*)&(parms->file_len)), NULL, FILE_BEGIN))
			util_errno_exit("SetFilePointerEx fail:");
		if (0 == SetEndOfFile(hfile))
			util_errno_exit("SetEndOfFile fail:");

		if (0 == SetFilePointer(hprofile, 4096, NULL, FILE_BEGIN))
			util_errno_exit("SetFilePointerEx fail:");
		if (0 == SetEndOfFile(hprofile))
			util_errno_exit("SetEndOfFile fail:");
	}
	//文件映射
	hprofilemap = CreateFileMapping(hprofile, NULL, PAGE_READWRITE, 0, 4096, NULL);
	if (NULL == hprofilemap)
		util_errno_exit("CreateFileMapping fail:");

	profile_buf = (char*)MapViewOfFile(hprofilemap, FILE_MAP_ALL_ACCESS, 0, 0, 4096);
	if (NULL == profile_buf)
		util_errno_exit("MapViewOfFile fail:");

	had_init = true;
}

void file_ma::do_write_back()
{
	std::list<block*> temp_list;

	block_set_lock.lock();

	for (auto x : block_set)
		temp_list.push_back(x);

	block_set_lock.unlock();

	for (auto x : temp_list)
		x->drain_all();

	write_progress();
}

uint32_t file_ma::getcachesize()
{
	return cache_size;
}

void file_ma::notify(block *which, int evcode, void *parm, void *parm2)
{
	switch (evcode)
	{
		case (int)block_evcode::complete:
		{
			bool set_empty = false;

			block_set_lock.lock();

			assert(block_set.count(which) == 1);
			block_set.erase(which);
			set_empty = block_set.empty();

			block_set_lock.unlock();

			if (set_empty)
			{
				had_complete = true;
				parent->notify(this, (int)file_evcode::complete, NULL, NULL);
			}

			break;
		}
		default:
			util_err_exit("无效的事件");
			break;
	}
}

uint32_t file_ma::get_numPieces()
{
	return block_set.size();
}
std::string file_ma::get_bitfield(uint32_t &remain_len)
{
	if (had_init)
	{
		uint32_t ptr = 0;
		remain_len = 0;
		uint32_t size = ceil((long double)parms->file_len / parms->min_split_size);
		unique_ptr<char[]> res = make_unique<char[]>(size);
		memset(res.get(), 'F', size);
		lock_guard<mutex> _lock(block_set_lock);

		for (auto x : block_set)
		{
			remain_len += x->len();

			ptr = x->begin() / parms->min_split_size;

			uint32_t y = x->begin() % parms->min_split_size;
			if (y > 0)
			{
				res[ptr] = to_hex(y * 16 / parms->min_split_size);
				++ptr;
			}

			uint32_t x2 = ceill((long double)x->end() / parms->min_split_size);
			memset(res.get() + ptr, '0', x2 - ptr);
			ptr = x2;
		}
		return string(res.get(), size);
	}
	else
		return string();
}