#ifndef JSON_RPC_H
#define JSON_RPC_H

#include <WinSock2.h>
#include <string>
#include <vector>
#include <list>
#include "iocp_base.h"
#include "json.hpp"
#include "global_def.h"
using json = nlohmann::json;

struct global_state
{
	std::string downloadSpeed;
	std::string numActive;
	std::string numStopped;
	std::string numStoppedTotal;
	std::string numWaiting;
	std::string uploadSpeed;
};
struct task_state
{
	//common
	std::string gid;
	std::string status;

	//about file
	std::string dir;
	std::string bitfield;
	std::string completedLength;
	struct file_t
	{
		std::string completedLength;
		std::string index;
		std::string length;
		std::string path;
		std::string selected;
		struct uri_t
		{
			std::string status;
			std::string uri;
		};
		std::vector<uri_t> uris;
	}files;
	std::string numPieces;
	std::string pieceLength;
	std::string totalLength;

	//about network
	std::string connections;
	std::string downloadSpeed;//getnewrecv
	std::string uploadLength;//hadn't implement
	std::string uploadSpeed;//hadn't implement
};
struct global_option
{
	std::string dir;
	std::string user_agent;
//std::string max_download_limit;
//std::string max_upload_limit;
	std::string min_split_size;
	std::string split;
	std::string max_concurrent_downloads;
};

struct down_parm;
class rpc_base
{
public:
	virtual global_state getGlobalStat() = 0;
	virtual global_option getGlobalOption() = 0;
	virtual std::list<task_state> tellActive() = 0;
	virtual std::list<task_state> tellWaiting() = 0;
	virtual std::list<task_state> tellStopped() = 0;
	virtual task_state tellStatus(std::string &gid) = 0;

	virtual size_t add_task(std::string uri, std::list<down_parm> &parms) = 0;
	virtual void start_task(size_t guid) = 0;
	virtual void pause_task(size_t guid) = 0;
	virtual void remove_task(size_t guid) = 0;
};


class client;
class json_rpc :public iocp_base
{
public:
	json_rpc(rpc_base *target);
	void start_listening();
	json process(const string & method, json &parms);

protected:
	json serialization(task_state &x);

private:
	void complete_callback(int key, int nRecv, overlapped_base *over_base) override;

private:
	rpc_base *target;
	bool started;
	SOCKET listern_sc;
	SOCKET accept_sc;
	overlapped_base over;
};

class client :public iocp_base
{
public:
	client(json_rpc *parent, SOCKET fd);
	~client();
	void start();

private:
	void complete_callback(int key, int nRecv, overlapped_base *over_base) override;

private:
	json_rpc* parent;
	SOCKET fd;

	char buf[piece_size];
	uint16_t end, content_len;
	bool content_begin;

	overlapped_base over;
};


#endif // JSON_RPC_H


