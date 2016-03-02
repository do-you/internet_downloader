#include <algorithm>
#include <boost/format.hpp>
#include "connection.h"
#include "iocp_base.h"
#include "utility.h"
#include "net_io.h"
#include "file_block.h"
#include "down_task.h"

unordered_map<string, string> default_header{
	make_pair("Accept", "*/*"),
	make_pair("Accept-Encoding", "gzip, deflate"),
	make_pair("User-Agent", "tinydown/3.0"),
	make_pair("Connection", "keep-alive")
};

connection::connection(net_io *parent, task_info* parms, block* blk) :connection(parent, parms)
{
	this->blk = blk;
}

connection::connection(net_io *parent, task_info* parms)
{
	this->info = parms->info;
	this->parms = parms;
	this->parent = parent;

	would_pause = header_end /*= need_data*/ = sniff_mode = had_init = false;

	memset(&over, 0, sizeof(over));
	over.remind_me = this;
}

void connection::sniff(net_io * cb, task_info* parms)
{
	auto x = new connection(cb, parms);
	x->sniff_mode = true;
	x->start();
}

void connection::start()
{
	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == SOCKET_ERROR)
		util_errno_exit("新建socket错误");
	else
		bind_to_iocp((HANDLE)fd, 1);

	if (!connect_to_server(fd, info))
		util_errno_exit("连接服务器失败");

	async_read(fd, over, buffer, piece_size);

	send_request();

	would_pause = false;
}


void connection::pause()
{
	would_pause = true;
}

string connection::make_header()
{
	string base_header;
	string new_line = "\r\n";

	base_header = str(boost::format("GET /%1% HTTP/1.1\r\nHost: %2%\r\n") % info.urlpath % info.host);

	if (!sniff_mode)
		base_header += string("Range: bytes=") + to_string(blk->begin()) + string("-") + to_string(blk->end() - 1) + new_line;

	for (auto &x : parms->user_header)
		base_header += x.first + string(":") + x.second + new_line;

	for (auto &x : default_header)
		if (parms->user_header.count(x.first) == 0)
			base_header += x.first + string(":") + x.second + new_line;

	base_header += new_line;

	return base_header;
}

void connection::send_request()
{
	string header = make_header();

	if (SOCKET_ERROR == send(fd, header.c_str(), header.length(), 0))
		util_errno_exit("send fail:");
}

void connection::data_callback(const char *buf, uint64_t len)
{
	assert(len >= 0);

	if (len > 0)
		blk->fill(buf, len);

	if (blk->len() > 0)
	{
		if (would_pause)
		{
			closesocket(fd);
			header_end = false;
			parent->notify(this, (int)connection_evcode::pause, NULL, NULL);
		}
		else
			async_read(fd, over, this->buffer, blk->len() > piece_size ? piece_size : blk->len());
	}
	else//完成
	{
		closesocket(fd);
		parent->notify(this, (int)connection_evcode::complete, NULL, NULL);
		delete this;
	}
}
void connection::parse_header(char *buf, uint64_t len)
{
	tempbuf.append(buf, len);
	if (tempbuf.rfind("\r\n\r\n") != string::npos)
	{
		const char *buffer = tempbuf.c_str();
		// 	char *ptr;
		int response_code;
		const char *parms_start;
		char *parms_end;

		if (_strnicmp(buffer, "HTTP/1.1", 8))
			util_err_exit("错误的头部");

		//先获取响应码
		parms_start = buffer;
		while (!isspace(*parms_start)) parms_start++;
		while (isspace(*parms_start)) parms_start++;//响应码开始
		response_code = atoi(parms_start);
		//move to the beginning of the parmeters
		parms_start = strstr(parms_start, "\r\n") + 2;

		bool read_len = false;
		switch (response_code / 100)
		{
			case 0:
				util_err_exit("响应码无法解析");
				break;
			case 1:
			case 2://请求成功
				while (strncmp(parms_start, "\r\n", 2) != 0)
				{
					if (sniff_mode)
					{
						if (!_strnicmp(parms_start, "content-length", 14))
						{
							parms->file_len = strtoull(parms_start + 15, &parms_end,10);
							read_len = true;
						}
						else if (!_strnicmp(parms_start, "content-disposition", 19))
						{
							const char* filename_start = strstr(parms_start + 20, "filename=\"") + 10;
							if (filename_start != NULL)
							{
								const char* filename_end = strchr(filename_start, '"');
								string temp_str(filename_start, filename_end);
								util_utf8_to_acp(temp_str, parms->file_name);
							}
						}
					}
					else if (!_strnicmp(parms_start, "content-range", 13))
					{
						const char* len_start = parms_start + 14;
						while (!isdigit(*len_start)) ++len_start;
						if (blk->begin() != atoi(len_start))
							util_err_exit("范围不符合");
					}
					parms_start = strstr(parms_start, "\r\n") + 2;
				}
				parms_start += 2;

				if (sniff_mode)
				{
					if (read_len)
					{
						parent->notify(this, (int)connection_evcode::sniff, NULL, NULL);
						closesocket(fd);
						delete this;
						return;
					}
					else
						util_err_exit("头部解析错误");
				}
				else
				{
					header_end = true;

					data_callback(parms_start, tempbuf.c_str() + tempbuf.size() - parms_start);
				}
				break;
			case 3://重定向
				while (strncmp(parms_start, "\r\n", 2) != 0)
				{
					if (!_strnicmp(parms_start, "location", 8))
					{
						const char* uri_start = parms_start + 9;
						while (isspace(*uri_start)) ++uri_start;
						const char* uri_end = strstr(uri_start, "\r\n");
						string new_url = string(uri_start, uri_end);

						if (!parseUrl(new_url, info))
							util_err_exit("非法的url\n");

						closesocket(fd);
						start();
						break;
					}
					parms_start = strstr(parms_start, "\r\n") + 2;
				}
				break;
			case 4://错误
			case 5:
			default:
				return;
		}
		tempbuf.clear();
	}
}

void connection::complete_callback(int key, int nRecv, overlapped_base * over_base)
{
	parent->submit_recv(nRecv);

	if (nRecv != 0)
	{
		buffer[nRecv] = '\0';
		if (header_end)
		{
			try
			{
				data_callback(buffer, nRecv);
			}
			catch (boost::system::system_error e)
			{
				parent->notify(this, (int)connection_evcode::failed, NULL, e.what());
			}
		}
		else
			parse_header(buffer, nRecv);
	}
	else
		parent->notify(this, (int)connection_evcode::failed, NULL, "服务器断开链接");
}
