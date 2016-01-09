#ifndef SESSION_H
#define SESSION_H

#include <forward_list>
#include <functional>
#include <string>
#include <unordered_map>
#include <boost/filesystem.hpp>
#include "http_parse.h"
#include "connection.h"
#include "filemanager.h"

using namespace std;
using namespace boost::filesystem;

enum class down_statues
{
	downloading,
	pause,
	failed,
	writing_back,
	finish
};

class session
{
#define max_cache_size 32*1024*1024
public:
	session(string url);

	void start();
	void pause();

	void write_process();

	void set_header(string parm, string value);
	void set_split(int nsplit);
	void set_path(string p);

	down_statues get_statues();
	uint64_t get_file_len(){ return file_len; }
	uint64_t get_remain_len();
	string   get_file_name(){ return file_name; }
	int      get_split_count(){ return connection_list.size(); }
protected:
	void init(string filename, uint64_t len, down_info &info_2);
	void block_complete(connection *which, down_info &info_2);

private:
	bool has_init;
	int max_split;
	unordered_map<connection*, block_ptr> connection_list;
	filemanager filemana;

	string file_name;
	uint64_t file_len;

	down_statues status;
	down_info info;

	unordered_map<string,string> user_header;
};





#endif // SESSION_H
