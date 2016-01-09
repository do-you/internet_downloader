#include <algorithm>
#include "connection.h"
#include "iocp_base.h"
#include "utility.h"
#include "file_io.h"

unordered_map<string, string> default_header{
	make_pair("Accept", "*/*"),
	make_pair("Accept-Encoding", "gzip, deflate"),
	make_pair("User-Agent", "tinydown/2.0")
};

connection::connection(down_info &info) :addition_header(default_header)
{
	this->info = info;
	would_pause = has_read_header = need_data = sniff_mode = had_init = false;
	content_len = had_received_len = 0;
	memset(&over, 0, sizeof(over));
	over.remind_me = this;
	header_parameter.clear();
}

void connection::init(finish_cb cb, block_ptr ptr, file_block *file)
{
	if (!had_init)
	{
		this->block = ptr;
		this->file = file;
		call_on_finish = cb;
		statues = connection_statues::pause;
		had_init = true;
	}
}

void connection::start()
{
	if (statues == connection_statues::pause && (had_init || sniff_mode))
	{
		if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == SOCKET_ERROR)
			statues = connection_statues::failed;
		else
			bind_to_iocp((HANDLE)fd, 1);

		if (!connect_to_server(fd, info))
		{
			printf("连接服务器失败\n");
			statues = connection_statues::failed;
			return;
		}

		async_read(fd, over, buffer, segment_size - 1);

		send_request();

		statues = connection_statues::downloading;
		start_time = std::chrono::steady_clock::now();
		len_mark = had_received_len;

		would_pause = false;
	}
}

void connection::start(down_info & info)
{
	this->info = info;
	start();
}

void connection::sniff(sniff_cb call_back)
{
	sniff_mode = true;
	cb = call_back;
	start();
}

void connection::pause()
{
	if (statues == connection_statues::downloading)
		would_pause = true;
}

double connection::average_speed()
{
	if (statues == connection_statues::downloading)
	{
		std::chrono::duration<double> m_time = std::chrono::steady_clock::now() - start_time;
		return (had_received_len - len_mark) / m_time.count();
	}
	else if (statues == connection_statues::pause || statues == connection_statues::finish)
	{
		std::chrono::duration<double> m_time = end_time - start_time;
		return (had_received_len - len_mark) / m_time.count();
	}
	else
		return 0;
}

bool connection::set_header(unordered_map<string,string> &header)
{
	if (header.empty())
		return false;

	for (auto &x : header)
	{
		if (addition_header.count(x.first) > 0)
			addition_header[x.first] = x.second;
		else
			addition_header.insert(x);
	}
	return true;
}

uint64_t connection::get_total_recv_len()
{
	return had_received_len;
}

string connection::make_header()
{
	char temp[1441];
	string base_header;
	string new_line = "\r\n";

	sprintf(temp, "GET %s HTTP/1.1\r\nHost: %s\r\n", info.path.c_str(), info.host.c_str());
	base_header = temp;
	if (!sniff_mode)
		base_header += string("Range: bytes=") + to_string(block->ptr) + string("-") + to_string(block->end - 1) + new_line;

	for (auto &x : addition_header)
	{
		base_header += x.first + string(":") + x.second + new_line;
	}

	base_header += new_line;

	return base_header;
}

void connection::send_request()
{
	string header = make_header();

	if (SOCKET_ERROR == send(fd, header.c_str(), header.length(), 0))
		util_errno_exit("send fail:");
}

inline void connection::change_to_fail()
{
	statues = connection_statues::failed;
	printf("%p fail:服务器主动断开 %d\n", this, fd);
	closesocket(fd);
	file->auto_destruct();
	//可能需要调用上层的fail_callback
	delete this;
}

void connection::http_success_callback()
{
	string value;

	if (header_parameter.find("content-length") != header_parameter.end())
		content_len = stoull(header_parameter["content-length"]);
	else
		util_err_exit("头部中找不到内容长度\n");

	if (header_parameter.find("content-range") != header_parameter.end())
	{
		value = header_parameter["content-range"];
		uint64_t range_start = 0;
		sscanf(value.c_str(), "%*s %llu", &range_start);

		if (range_start != block->ptr)
			util_err_exit("头部中的数据范围不符合\n");
	}

	if (header_parameter.find("content-disposition") != header_parameter.end())
	{
		value = header_parameter["content-disposition"];
		size_t filename_start = value.find("filename=\"")+10;

		if (filename_start != std::string::npos)
		{
			size_t filename_end = value.find('"', filename_start) ;
			string utf8_filename = value.substr(filename_start, filename_end - filename_start);
			util_utf8_to_acp(utf8_filename, info.filename);
		}
	}

	need_data = true;
}

void connection::http_redirect_callbak()
{
	string value;

	if (header_parameter.find("location") != header_parameter.end())
	{
		value = header_parameter["location"];
		int offset = -1;
		while (value[++offset] == ' ');
		value = value.substr(offset);

		change_to_pause();

		if (parseUrl(value, info))
			start();
		else
			util_err_exit("跳转地址解析错误\n");
	}
	else
		util_err_exit("找不到要跳转的地址\n");
}

void connection::http_error_callback(int response_code)
{

}
bool connection::data_callback(char *buf, uint64_t len)
{
	// 	return true;

	if (len > 0)
	{
		len = min(len, block->len());

		had_received_len += len;
		block->ptr += len;

		file->write_to_file(buf, len);

		if (block->ptr >= block->end)//完成
		{
			statues = connection_statues::finish;
			end_time = std::chrono::steady_clock::now();

			call_on_finish(this, info);//先通知上层处理
			file->auto_destruct();

			delete this;
			return false;
		}
	}

	if (would_pause)
	{
		change_to_pause();
		return false;
	}
	else
		return true;
}
int connection::parse_header()
{
	// 	char *ptr;
	// 	int response_code;
	char *parms_start, *end, *parms_end;

	assert(!_strnicmp(buffer, "HTTP/1.1", 8));

	//先获取响应码
	parms_start = buffer;
	while (!isspace(*parms_start)) parms_start++;
	while (isspace(*parms_start)) parms_start++;
	parms_end = parms_start;
	while (!isspace(*parms_end)) parms_end++;
	header_parameter.insert({ "code",string(parms_start ,parms_end) });
	//move to the begin of the next line
	parms_start = strstr(parms_start, "\r\n") + 2;
	//read other parameter
	while (strncmp(parms_start, "\r\n", 2) != 0)
	{
		end = strstr(parms_start, "\r\n");
		parms_end = parms_start;
		while (*parms_end != ':' && parms_end < end) ++parms_end;

		if (parms_end == end)
			util_err_exit("非法的HTTP参数");
		else
		{
			std::for_each(parms_start, parms_end, [](char &a) {a = tolower(a); });
			header_parameter.insert({ string(parms_start ,parms_end),string(parms_end + 1,end) });
		}
		//move to the begin of the next line
		parms_start = end + 2;
	}
	parms_start += 2;

	return parms_start - buffer;
}
void connection::change_to_pause()
{
	closesocket(fd);
	need_data = has_read_header = false;
	header_parameter.clear();
	len_mark = had_received_len;
	end_time = std::chrono::steady_clock::now();
	if (file != NULL)
		file->flush();
	statues = connection_statues::pause;
}
void connection::complete_callback(int key, int nRecv, overlapped_base * over_base)
{
	int data_offset;

	if (nRecv != 0)
	{
		buffer[nRecv] = '\0';
		data_offset = 0;

		if (!has_read_header) //read header
		{
			data_offset = parse_header();
			has_read_header = true;

			int response_code = std::stoi(header_parameter["code"]);
			switch (response_code / 100)
			{
				case 1:
				case 2://请求成功
					http_success_callback();
					if (sniff_mode)
					{
						closesocket(fd);
						printf("%d 断开 \n", fd);
						cb(info.filename, content_len, info);

						delete this;

						return;
					}
					break;
				case 3://重定向
					http_redirect_callbak();
					break;
				case 4://错误
				case 5:
					http_error_callback(response_code);
				default:
					return;
			}
		}

		if (need_data)
			if (data_callback(buffer + data_offset, nRecv - data_offset))
				async_read(fd, over, buffer, segment_size - 1);
	}
	else
		change_to_fail();
}

