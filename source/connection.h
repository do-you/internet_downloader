#ifndef CONNECTION_H
#define CONNECTION_H

#include <functional>
#include <unordered_map>
#include <chrono>
#include <string>
#include "file_io.h"
#include "http_parse.h"
#include "iocp_base.h"
using namespace std;

enum class connection_statues
{
	pause,
	downloading,
	failed,
	finish
};

class connection :public iocp_base
{
#define segment_size 4096
	using finish_cb = function<void(connection*, down_info &info)>;
	using sniff_cb = function<void(string, uint64_t,down_info &)>;

public:
	//初始化函数
	connection(down_info &info);
	void init(finish_cb cb, block_ptr ptr, file_block *file);
	//控制函数
	void start();
	void pause();
	void start(down_info &info);
	void sniff(sniff_cb call_back);
	//显示状态函数 
	double average_speed();//Bytes per second

	//附加选项函数
	bool set_header(unordered_map<string,string> &header);

	uint64_t get_total_recv_len();

protected:
	void send_request();
	string make_header();

	void http_success_callback();
	void http_redirect_callbak();
	void http_error_callback(int response_code);
	//return true if need more data
	bool data_callback(char *buf, uint64_t len);
	//return the offset of data
	int  parse_header();
	void change_to_pause();
	void change_to_fail();
	// 通过 iocp_base 继承
	virtual void complete_callback(int key, int nRecv, overlapped_base * over_base) override;

	// private:
public:
	SOCKET fd;
	block_ptr block;
	bool need_data;
	bool had_init;
	bool has_read_header;
	unordered_map<string, string> header_parameter;
	bool sniff_mode;
	bool would_pause;
	uint64_t content_len;
	uint64_t had_received_len;
	std::chrono::steady_clock::time_point start_time, end_time;
	uint64_t len_mark;

	char buffer[segment_size];
	connection_statues statues;
	down_info info;
	file_block *file;
	finish_cb call_on_finish;
	sniff_cb cb;
	overlapped_base over;

	unordered_map<string, string> addition_header;
};

#endif // CONNECTION_H
