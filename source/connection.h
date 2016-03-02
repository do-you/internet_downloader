#ifndef CONNECTION_H
#define CONNECTION_H

#include <functional>
#include <unordered_map>
#include <chrono>
#include <string>
#include "uri_parse.h"
#include "iocp_base.h"
#include "global_def.h"
using namespace std;

enum class connection_evcode
{
	pause,
	failed,
	complete,
	sniff
};

class net_io;
struct task_info;
class block;
class connection :public iocp_base
{
public:
	//初始化函数
	connection(net_io *parent, task_info* parms, block* blk);
	// 	void init(finish_cb cb, block_ptr ptr, file_block *file);
	//控制函数
	void start();
	void pause();

	static void sniff(net_io * cb, task_info* parms);

	// 	double average_speed();//Bytes per second

protected:
	connection(net_io *parent, task_info* parms);

	string make_header();
	void send_request();

	void data_callback(const char *buf, uint64_t len);
	void  parse_header(char *buf, uint64_t len);

	// 通过 iocp_base 继承
	virtual void complete_callback(int key, int nRecv, overlapped_base * over_base) override;

	// private:
public:
	SOCKET fd;
	block* blk;
// 	bool need_data;
	bool had_init;
	bool header_end;
	bool sniff_mode;
	bool would_pause;

// 	string filename;
// 	uint64_t content_len;
// 	uint64_t had_received_len;
// 	std::chrono::steady_clock::time_point start_time, end_time;
// 	uint64_t len_mark;

	char buffer[piece_size + 1];
	uri_info info;

	net_io *parent;
	task_info* parms;
	overlapped_base over;

	string tempbuf;
};

#endif // CONNECTION_H
