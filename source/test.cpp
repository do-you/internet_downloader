#include <stdio.h>
#include <algorithm>
#include <unordered_map>
#include <mutex>
#include "filemanager.h"
#include "connection.h"
#include "utility.h"
#include <thread>
using namespace std;
using namespace boost::filesystem;

#define MB(x) x*1024*1024
#define splite_size 10

void complete_cb(connection *which, down_info &info);

down_info info;
string m_dir = "E:\\测试";
filemanager fl(m_dir);
unordered_map<connection*, block_ptr> connection_list;

bool test(block_ptr &a, block_ptr &b)
{
	return a->ptr < b->ptr;
}
mutex mute;
void init(string &a, uint64_t b,down_info &info1)
{
	string filename;
	uint64_t filelen = 0;
	block_ptr ptr[16];
	file_block *block[16];


	filename = a;
	filelen = b;

	printf("获取文件信息成功\n");

	fl.init(filename, filelen);
	fl.static_allocate(ptr, splite_size, 2 * 1024 * 1024);
	for (int i = 0; i < splite_size; i++)
	{
		block[i] = fl.create_block_manager(ptr[i], MB(32) / splite_size);
	}

	for (int i = 0; i < splite_size; i++)
	{
		auto x = new connection(info);

		x->set_header("Cookie: gdriveid=EBFC09CAFDDA469233027B7B12819465");
// 		x->set_header("Referer: http://pan.baidu.com/disk/home");
// 		x->set_header("Cookie: BDUSS=0ZOUnVFcjd3QkRIYTdEclRvZkZGUFVrb0MtdTBRVVpoZmhxS3B5YUx0VWxDMjFXQVFBQUFBJCQAAAAAAAAAAAEAAACZ8XsONDk2MDM3NjQ1AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAACV-RVYlfkVWT;BAIDUID=C6B9945853349ECE3EF4FBEBEF1B1D80:FG=1");
// 		x->set_header("User-Agent: netdisk;5.2.7;PC;PC-Windows;6.2.9200;WindowsBaiduYunGuanJia");

		printf("%p create\n", x);
		x->init(complete_cb, ptr[i], block[i]);
		x->start();
		connection_list.insert({ x, ptr[i] });
	}
	uint64_t temp = 0;
	int i = 0;
	while (++i)
	{
		temp = 0;

		mute.lock();
		printf("\n");
		for (auto x : connection_list)
		{
// 			if (i == 5)
// 			{
// 				x.first->pause();
// 			}
// 			else if (i == 9)
// 			{
// 				x.first->start();
// 			}
// 			else
				printf("               %p  %llu %llu  %.2lf KB/s\n", x.first, x.first->block->ptr / 1024, x.first->block->end / 1024, x.first->average_speed() / 1024);
		}
		printf("\n");

		fl.write_progress();
// 		if (i == 5)
// 			exit(0);

		mute.unlock();


		Sleep(1000);
	}

}
void test2()
{
	printf("waitting\n");
	fl.testsync();
	printf("任务完成\n");
}
void complete_cb(connection *which, down_info &info)
{
	block_ptr ptr;

	mute.lock();

	printf(" %s  %2.2lf KB/S\n", info.host.c_str(), which->average_speed() / 1024);

	fl.block_complete(connection_list[which]);
	connection_list.erase(which);

	if (fl.dynamic_allocate(ptr, min_split_size))
	{
		auto x = new connection(info);

		x->set_header("Cookie: gdriveid=EBFC09CAFDDA469233027B7B12819465");
// 		x->set_header("Referer: http://pan.baidu.com/disk/home");
// 		x->set_header("Cookie: BDUSS=0ZOUnVFcjd3QkRIYTdEclRvZkZGUFVrb0MtdTBRVVpoZmhxS3B5YUx0VWxDMjFXQVFBQUFBJCQAAAAAAAAAAAEAAACZ8XsONDk2MDM3NjQ1AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAACV-RVYlfkVWT;BAIDUID=C6B9945853349ECE3EF4FBEBEF1B1D80:FG=1");
// 		x->set_header("User-Agent: netdisk;5.2.7;PC;PC-Windows;6.2.9200;WindowsBaiduYunGuanJia");

		connection_list.insert({ x,ptr });

		printf("重新分配 %llu-%llu\n", ptr->ptr, ptr->end);

		x->init(complete_cb, ptr, fl.create_block_manager(ptr, 1024 * 1024 * 8));
		x->start();
	}
	else
	{
		if (connection_list.size() == 0)
		{
			new thread(test2);
		}
	}

	mute.unlock();
}
int main()
{
	win32_init();


// 	string url = "http://dldir1.qq.com/qqfile/qq/QQ7.9/16638/QQ7.9.exe";
// 	string url = "http://down.360.cn/360sd/360sd_x64_std_5.0.0.6121.exe";
	string url = "http://gdl.lixian.vip.xunlei.com/download?fid=esIB1Uln/7WfnYiMmCY2jhCLBDsaLQQNAAAAAKrdkVZP1ZVBeWl01LuZLTpsEUl0&mid=666&threshold=150&tid=2C92C19508220A0FA7333C01859678F4&srcid=4&verno=1&g=AADD91564FD59541796974D4BB992D3A6C114974&scn=t38&i=AADD91564FD59541796974D4BB992D3A6C114974&t=2&ui=63823051&ti=1174794715470081&s=218377498&m=0&n=013AD44AFFE789A9E5D1BE03EDBEE781B53968C2055D5B30313C6A03E5A2E69785889772BA8F917777161F8C33746D2E63026CBF18425D5B375301B4025B4D50343C6A03FA9EE5A587849C73BAB995E7BBE56CCA3270340000&ff=0&co=4E895AC9731E4DF028B57C7AA2BFBF77&cm=1&pk=lixian&ak=1:1:7:3&e=1459417458&ms=10485760&ck=EBFC09CAFDDA469233027B7B12819465&at=D520722FF4B046BBEFE383FFEF3572A7";
// 	string url = "http://d.pcs.baidu.com/file/4a908c78b911b6a247c2d1c4df4fa9c8?fid=4044019620-250528-797554367086629&time=1451632885&rt=pr&sign=FDTAER-DCb740ccc5511e5e8fedcff06b081203-RVTwIFX3vR13u%2b0Q6L5bYvz8NNE%3d&expires=8h&chkbd=0&chkv=0&dp-logid=14840391429453931&dp-callid=0&r=574523794";

	if (!parseUrl(url, info))
		util_err_exit("url格式错误\n");

	auto x = new connection(info);
	x->set_header("Cookie: gdriveid=EBFC09CAFDDA469233027B7B12819465");
// 	x->set_header("Referer: http://pan.baidu.com/disk/home");
// 	x->set_header("Cookie: BDUSS=0ZOUnVFcjd3QkRIYTdEclRvZkZGUFVrb0MtdTBRVVpoZmhxS3B5YUx0VWxDMjFXQVFBQUFBJCQAAAAAAAAAAAEAAACZ8XsONDk2MDM3NjQ1AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAACV-RVYlfkVWT;BAIDUID=C6B9945853349ECE3EF4FBEBEF1B1D80:FG=1");
// 	x->set_header("User-Agent: netdisk;5.2.7;PC;PC-Windows;6.2.9200;WindowsBaiduYunGuanJia");
	x->sniff(init);

	win32_loop();
	return 0;
}