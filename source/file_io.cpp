#include <WinSock2.h>
#include <windows.h>
#include <functional>
#include <assert.h>
#include <string>
#include "file_io.h"
#include "utility.h"

using namespace std;


void qvalue(int n, int ary[], int len)
{
	int *result = new int[len];
	double total = 0;
	double max = -1;
	int which = -1;

	for (int i = 0; i < len; i++)
		result[i] = 1;
	n -= len;

	while (n > 0)
	{
		for (int i = 0; i < len; i++)
		{
			double temp = (double)ary[i] / result[i] / (result[i] + 1)*ary[i];

			assert(temp > 0);

			if (temp > max)
			{
				max = temp;
				which = i;
			}
		}
		++result[which];
		--n;
	}
	memcpy(ary, result, sizeof(int)*len);
}


file_io::file_io(path &dir) :dir(dir), undistributed_queue(front_first)
{
	had_finish = had_drain_all = had_init = false;
}

file_io::~file_io()
{
	comp_mutex.lock();

	while (distributed_list.size() + writing_back_list.size() != 0)
		is_comp.wait(comp_mutex);

	comp_mutex.unlock();

	if (!had_finish)
	{
		UnmapViewOfFile(profile_buf);
		CloseHandle(hprofile);
		CloseHandle(hprofilemap);
		CloseHandle(hfile);
	}
	printf("is over\n");
}

void file_io::init(string &filename, uint64_t len)
{
	if (!had_init)
	{
		file_len = len;

		string name, ext;
		int pos, i = 0;
		path temppath, temppath2;

		pos = filename.rfind('.');
		name = filename.substr(0, pos);
		if (pos != string::npos)
			ext = filename.substr(pos);

		while (true)
		{
			temppath2 = temppath = dir;

			if (exists(temppath /= filename))
			{
				if (exists(temppath2 /= (filename + ".abc")) && is_regular_file(temppath2) && file_size(temppath2) != 0)
				{
					file_init(temppath.string(), OPEN_EXISTING);
					//读取记录
					DWORD profilelen = 0;
					int res = ReadFile(hprofile, profile_buf, 4 * 1024, &profilelen, NULL);
					if (res == 0)
						util_errno_exit("ReadFile faile:");
					//解析profile
					parse_profile(profilelen);

					break;
				}
				else
					filename = name + to_string(++i) + ext;
			}
			else
			{
				//直接创建
				file_init(temppath.string(), CREATE_NEW);
				undistributed_queue.emplace(0, file_len);
				break;
			}
		}
		had_init = true;
	}
}

bool file_io::dynamic_allocate(block_ptr &ptr, uint32_t split_size)
{
	if (undistributed_queue.size() > 0)
	{
		auto x = new block_cache(*undistributed_queue.begin());
		undistributed_queue.erase(undistributed_queue.begin());

		distributed_list.push_back(x);
		ptr = &x->base;
		return true;
	}
	else if (distributed_list.size() == 0)
		return false;
	else
	{
		uint64_t max_len = 0;
		block_ptr max_bl = nullptr;

		split_size = max(min_split_size, split_size);
		for (auto a = distributed_list.begin(); a != distributed_list.end(); a = a->next)
		{
			auto temp = &a->base;
			if (temp->len() > max_len)
			{
				max_len = temp->len();
				max_bl = temp;
			}
		}

		if ((max_len /= 2) >= split_size)
		{
			uint64_t end = max_bl->end;
			max_bl->end -= max_len;

			auto x = new block_cache(end - max_len, end);
			distributed_list.push_back(x);
			ptr = &x->base;
			return true;
		}

		return false;
	}
}

int file_io::static_allocate(block_ptr ary[], int nsplit, uint32_t split_size)
{
	if (had_init && undistributed_queue.size() > 0)
	{
		if (undistributed_queue.size() >= nsplit)//一人一个
		{
			for (int i = 0; i < nsplit; i++)
			{
				auto x = new block_cache(*undistributed_queue.begin());
				undistributed_queue.erase(undistributed_queue.begin());
				distributed_list.push_back(x);
				ary[i] = &x->base;
			}
		}
		else//一人一个或以上
		{
			if (split_size < min_split_size)
				split_size = min_split_size;

			block *block_array = new block[undistributed_queue.size()];
			int *len_array = new int[undistributed_queue.size()];

			int array_len, result_off;
			for (array_len = 0, result_off = 0; undistributed_queue.size() != 0;)
			{
				if (undistributed_queue.begin()->base.len() <= split_size)//小于等于split_size的直接用一个线程负责
				{
					auto x = new block_cache(*undistributed_queue.begin());
					undistributed_queue.erase(undistributed_queue.begin());
					distributed_list.push_back(x);

					ary[result_off++] = &x->base;

					--nsplit;
				}
				else//将大于split_size的快复制出来，一会用Q值法分配
				{
					block_array[array_len] = undistributed_queue.begin()->base;
					undistributed_queue.erase(undistributed_queue.begin());
					len_array[array_len] = block_array[array_len].len() / split_size;

					++array_len;
				}
			}
			//修改的Q值法，每一块至少分配一个
			qvalue(nsplit, len_array, array_len);

			for (int i = 0; i < array_len; ++i)
			{
				int real_split_count = min(block_array[i].len() / split_size, len_array[i]);
				//分割插入
				uint64_t piece_len = block_array[i].len() / real_split_count;
				uint64_t ptr = block_array[i].ptr;
				block_ptr x = nullptr;
				for (int j = 0; j < real_split_count; j++)
				{
					auto y = new block_cache(ptr, ptr + piece_len);
					ptr += piece_len;
					distributed_list.push_back(y);
					x = ary[result_off++] = &y->base;
				}
				x->end = block_array[i].end;
			}
			assert(distributed_list.size() == result_off);
			delete[] block_array;
			delete[] len_array;
		}
		return distributed_list.size();
	}
	return 0;
}

void file_io::block_complete(block_ptr ptr)
{
	block_cache *blk_ptr = CONTAINING_RECORD(ptr, block_cache, base);

	distributed_list.erase(blk_ptr);
	writing_back_list.push_back(blk_ptr);

	if (distributed_list.size() == 0 && undistributed_queue.size() == 0)
		printf("文件接收完毕\n");
}

void file_io::write_progress()
{
	if (had_init&&!had_finish)
	{
		profile* pro = (profile*)profile_buf;
		pro->len = file_len;

		int nblock = 0;
		for (auto &x : undistributed_queue)
		{
			if (x.base.len() > 0)
			{
				pro->blocks[nblock] = x.base;
				++nblock;
			}
		}
		for (auto x = distributed_list.begin(); x != distributed_list.end(); x = x->next)
		{
			pro->blocks[nblock].ptr = x->actual_ptr;
			pro->blocks[nblock].end = x->base.end;

			++nblock;
		}
		for (auto x = writing_back_list.begin(); x != writing_back_list.end(); x = x->next)
		{
			pro->blocks[nblock].ptr = x->actual_ptr;
			pro->blocks[nblock].end = x->base.end;

			++nblock;
		}

		pro->nblock = nblock;

		FlushViewOfFile(profile_buf, 12 + sizeof(block)*nblock);
	}
}

file_block* file_io::create_block_manager(block_ptr blo, uint32_t cachesize)
{
	return new file_block(CONTAINING_RECORD(blo, block_cache, base), hfile, cachesize, std::bind(&file_io::write_cmplete, this, std::placeholders::_1));
}

void file_io::set_path(string &p)
{
	if (!had_init)
		dir = p;
}

void file_io::testsync()
{
	comp_mutex.lock();

	while (distributed_list.size() + writing_back_list.size() != 0)
		is_comp.wait(comp_mutex);

	comp_mutex.unlock();

	printf("is over\n");
}

void file_io::file_init(const string &filename, int flag)
{
	file_name = filename;

	hfile = CreateFileA(filename.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, flag, FILE_FLAG_OVERLAPPED, NULL);
	if (INVALID_HANDLE_VALUE == hfile)
		util_errno_exit("CreateFileA fail:");

	hprofile = CreateFileA((filename + ".abc").c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, flag, FILE_ATTRIBUTE_NORMAL, NULL);
	if (INVALID_HANDLE_VALUE == hprofile)
		util_errno_exit("CreateFileA fail:");

	bind_to_iocp(hfile, 2);

	//文件预分配
	if (flag == CREATE_NEW)
	{
		if (0 == SetFilePointerEx(hfile, *((LARGE_INTEGER*)&file_len), NULL, FILE_BEGIN))
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
}

bool file_io::parse_profile(size_t len)
{
	profile *prof = (profile*)profile_buf;

	if (len < 12 || prof->len != file_len || len < prof->nblock*sizeof(block) + 12)
	{
		undistributed_queue.emplace(0, file_len);
		return false;
	}

	for (uint32_t i = 0; i < prof->nblock; ++i)
	{
		undistributed_queue.emplace(prof->blocks[i]);
	}

	return true;
}


void file_io::write_cmplete(block_cache *blk)//要加锁保护
{
	writing_back_list.erase(blk);
	delete blk;

	if (distributed_list.size() + writing_back_list.size() == 0)
	{
		had_drain_all = true;
		if (undistributed_queue.size() == 0)
		{
			UnmapViewOfFile(profile_buf);
			CloseHandle(hprofile);
			CloseHandle(hprofilemap);
			if (0 == DeleteFileA((file_name + ".abc").c_str()))
				util_errno_exit("DeleteFile fail:");
			CloseHandle(hfile);

			had_finish = true;
		}
	}

	is_comp.notify_one();
}

bool front_first(const block_cache &a, const block_cache &b)
{
	return a.base.ptr < b.base.ptr;
}


