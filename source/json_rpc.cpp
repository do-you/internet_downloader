#include "json_rpc.h"
#include "utility.h"
#include <assert.h>
#include "Internet_Downloader.h"

//返回头部是否结束
bool handle_header(char *buf, char **buf_end, string &key, string &value)
{
    assert(**buf_end == '\0');
    key.clear();
    value.clear();

    *buf_end = strstr(buf, "\r\n");

    if (*buf_end != NULL)//parm_start开始的不是完整的一行
    {
        if (buf == *buf_end)//读到最后了
        {
            *buf_end += 2;
            return true;
        }
        else
        {
            char *sep = strchr(buf, ':');
            if (sep != NULL && sep < *buf_end)
            {
                key.assign(buf, sep);
                value.assign(sep + 1, *buf_end);
            }
            *buf_end += 2;
            return false;
        }
    }
    else
        return false;
}

json_rpc::json_rpc(rpc_base *target)
{
    this->target = target;
    started = false;
    memset(&over, 0, sizeof(over));
    over.remind_me = this;
}

void json_rpc::start_listening()
{
    listern_sc = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
    accept_sc = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
    if (listern_sc == INVALID_SOCKET || accept_sc == INVALID_SOCKET)
        util_errno_exit("WSASocket fail:");

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(9998);
    if (::bind(listern_sc, (SOCKADDR *) &addr, sizeof(addr)) == SOCKET_ERROR)
        util_errno_exit("bind fail:");

    if (SOCKET_ERROR == listen(listern_sc, 50))
        util_errno_exit("listen fail:");

    bind_to_iocp((HANDLE) listern_sc, 1);

    async_accept(listern_sc, accept_sc, over);
}

json json_rpc::process(const string &method, json &parms)
{
    json res;
    if (method == "aria2.getGlobalStat")
    {
        auto val = target->getGlobalStat();
        res["downloadSpeed"] = val.downloadSpeed;
        res["numActive"] = val.numActive;
        res["numStopped"] = val.numStopped;
        res["numStoppedTotal"] = val.numStoppedTotal;
        res["numWaiting"] = val.numWaiting;
        res["uploadSpeed"] = val.uploadSpeed;
    }
// 	else if (method == "aria2.tellActive" || method == "aria2.tellWaiting" || method == "aria2.tellStopped")
// 	{
// 		return json::parse(u8R"([{"bitfield":"00","completedLength":"851968","connections":"1","dir":"F:\\迅雷下载","downloadSpeed":"116724","files":[{"completedLength":"0","index":"1","length":"4456448","path":"F:\/迅雷下载\/Clion_1.0.3_Crack_in_Win7.avi","selected":"true","uris":[{"status":"used","uri":"http:\/\/d.pcs.baidu.com\/file\/e2c2fa0499ab8eaa18b55fc8a6d96076?fid=4044019620-250528-569449714484174&time=1457258030&rt=pr&sign=FDTAER-DCb740ccc5511e5e8fedcff06b081203-dpjz765bH%2fdkCConHZh9aq7pcQg%3d&expires=8h&chkbd=0&chkv=0&dp-logid=1524828712104146165&dp-callid=0&r=979441829"}]}],"gid":"cca4036aca08afb5","numPieces":"5","pieceLength":"1048576","status":"active","totalLength":"4456448","uploadLength":"0","uploadSpeed":"0"}])");
// 	}
    else if (method == "aria2.tellActive" || method == "aria2.tellWaiting" || method == "aria2.tellStopped")
    {
        res = json::array();

        decltype(target->tellActive()) val;
        if (method == "aria2.tellActive")
            val = target->tellActive();
        else if (method == "aria2.tellWaiting")
            val = target->tellWaiting();
        else if (method == "aria2.tellStopped")
            val = target->tellStopped();

        for (auto &x : val)
            res.push_back(std::move(serialization(x)));
    }
    else if (method == "aria2.getVersion")
    {
        res = json::parse(R"({"enabledFeatures":["http","json rpc"],"version":"3.0.1"})");
    }
    else if (method == "aria2.getGlobalOption")
    {
        auto val = target->getGlobalOption();
        res = {
                {"dir",                      val.dir},
                {"user-agent",               val.user_agent},
                {"max-download-limit",       "0"},
                {"max-upload-limit",         "0"},
                {"min-split-size",           val.min_split_size},
                {"max-concurrent-downloads", val.max_concurrent_downloads},
                {"split",                    val.split}
        };
    }
    else if (method == "aria2.tellStatus")
    {
        res = serialization(target->tellStatus(parms[0].get<std::string>()));
    }
    else if (method == "aria2.addUri")
    {
        list <down_parm> parm_list;
        json &json_parms = parms[1];

        for (json::iterator it = json_parms.begin(); it != json_parms.end(); ++it)
        {
            parm_list.emplace_back();
            parm_list.back().parm = it.key();
            parm_list.back().value = it.value().get<string>();
        }

        res = decimal_to_hex(target->add_task(parms[0][0].get<string>(), parm_list));
    }
    else if (method == "aria2.pause")
    {
        target->pause_task(hex_to_decimal(parms[0].get<string>()));
        return parms[0];
    }
    else if (method == "aria2.unpause")
    {
        target->start_task(hex_to_decimal(parms[0].get<string>()));
        return parms[0];
    }
    else if (method == "aria2.remove" || method == "aria2.removeDownloadResult")
    {
        target->remove_task(hex_to_decimal(parms[0].get<string>()));
        return parms[0];
    }
    else
        throw runtime_error(method + u8"未实现");

    return res;
}

json json_rpc::serialization(task_state x)
{
    json task;

    task["bitfield"] = x.bitfield;
    task["completedLength"] = x.completedLength;
    task["connections"] = x.connections;
    task["dir"] = x.dir;
    task["downloadSpeed"] = x.downloadSpeed;
    auto &files = task["files"] = json::array();
    //for (auto &y : x.files)
    auto &y = x.files;
    {
        json file;
        file["completedLength"] = y.completedLength;
        file["index"] = y.index;
        file["length"] = y.length;
        file["path"] = y.path;
        file["selected"] = y.selected;
        auto &uris = file["uris"] = json::array();
        for (auto &z : y.uris)
        {
            json uri;
            uri["status"] = z.status;
            uri["uri"] = z.uri;
            uris.push_back(std::move(uri));
        }
        files.push_back(std::move(file));
    }
    task["gid"] = x.gid;
    task["numPieces"] = x.numPieces;
    task["pieceLength"] = x.pieceLength;
    task["status"] = x.status;
    task["totalLength"] = x.totalLength;
    task["uploadLength"] = x.uploadLength;
    task["uploadSpeed"] = x.uploadSpeed;

    return task;
}

void json_rpc::complete_callback(int key, int nRecv, overlapped_base *over_base)
{
    _DEBUG_OUT("%u 建立连接\n", accept_sc);

    auto x = new client(this, accept_sc);
    x->start();

    accept_sc = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
    async_accept(listern_sc, accept_sc, this->over);
}

client::client(json_rpc *parent, SOCKET fd)
{
    this->parent = parent;
    this->fd = fd;
    memset(&over, 0, sizeof(over));
    over.remind_me = this;

    end = content_len = 0;
    content_begin = false;
}

client::~client()
{
    closesocket(fd);
    _DEBUG_OUT("%u close\n", fd);
}

void client::start()
{
    bind_to_iocp((HANDLE) fd, 1);
    async_read(fd, this->over, buf, piece_size - 1);
}


std::string http_response_header = "HTTP/1.1 200 OK\r\n\
Cache-Control: no-cache\r\n\
Access-Control-Allow-Origin: *\r\n\
Content-Type: application/json-rpc\r\n";

void client::complete_callback(int key, int nRecv, overlapped_base *over_base)
{
    if (nRecv == 0)
    {
        delete this;
        return;
    }

    end += nRecv;
    buf[end] = '\0';

    //处理头部
    if (content_begin == false)
    {
        char *buf_start = buf;
        char *buf_end = buf + end;
        string key, value;

        while (true)
        {
            content_begin = handle_header(buf_start, &buf_end, key, value);

            if (buf_end != NULL)//完整地处理了一行
            {
                buf_start = buf_end;
                buf_end = buf + end;

                if (content_begin)//头部读完了
                    break;
                else
                {
                    if (key.compare(0, 14, "Content-Length") == 0)
                        content_len = stoul(value);
                }
            }
            else
                break;
        }

        if (buf_start != buf)//把已经处理了的数据去掉
        {
            end = buf + end - buf_start;
            strncpy(buf, buf_start, end);
            buf[end] = '\0';
        }
    }

    bool recv_all = false;
    //处理数据
    if (content_begin)
    {
        if (content_len == 0)
        {
            printf("[%u] receive a invalid request\n", fd);
            delete this;
            return;
        }
        else if (content_len > piece_size - 1)//too big to process
        {
            printf("[%u] receive a request with data length %u\n", fd, content_len);
            delete this;
            return;
        }
        else
        {
            recv_all = end >= content_len;
            if (recv_all)//处理
            {
                assert(end == content_len);

                //parse json
                json request_json = json::parse(string(buf, content_len));
                json response_json;
                auto handler = [&](json &request) {
                    json res;

                    res["id"] = request["id"];
                    res["jsonrpc"] = "2.0";
                    try
                    {
                        _DEBUG_OUT("%u call %s\n", fd, request["method"].get<string>().c_str());
                        res["result"] = parent->process(request["method"], request["params"]);
                    }
                    catch (exception &e)
                    {
                        res["error"] = json({{"code",    1},
                                             {"message", e.what()}});
                    }

                    return res;
                };


                if (request_json.is_array())
                    for (auto &x : request_json)
                        response_json.push_back(handler(x));
                else
                    response_json = handler(request_json);


                //返回结果的json
                string json = response_json.dump();
                string resp = http_response_header;
                resp += string("Content-Length: ") + to_string(json.length()) + "\r\n\r\n";
                resp += json;

                if (SOCKET_ERROR == send(fd, resp.data(), resp.length(), 0))
                    util_errno_exit("send failed:");
            }
        }
    }

    if (recv_all)
    {
        end = content_len = 0;
        content_begin = false;
    }

    try
    {
        async_read(fd, this->over, buf + end, piece_size - 1 - end);
//        _DEBUG_OUT("继续投递\n");
    }
    catch (exception e)
    {
        printf("error occur on client:%s\n", e.what());
        delete this;
    }
}

// void client::complete_callback(int key, int nRecv, overlapped_base *over_base)
// {
// 	string temp_str;
// 	uint32_t content_begin;
// 
// 	if (nRecv == 0)
// 		delete this;
// 	else
// 	{
// 		buf[nRecv] = '\0';
// 
// 		temp_str.append(buf);
// 
// 		if ((content_begin = temp_str.find("\r\n\r\n")) != string::npos)
// 		{
// 			content_begin += 4;
// 
// 			auto pos = temp_str.find("Content-Length:");
// 			if (pos == string::npos)
// 				delete this;
// 
// 			uint32_t content_len = stoul(temp_str.substr(pos + 15));
// 			if (temp_str.length() >= (content_begin + content_len))
// 			{
// 				//parse json
// 				json request_json = json::parse(temp_str.substr(content_begin, content_len));
// 				json response_json;
// 
// 				response_json["id"] = request_json["id"];
// 				response_json["jsonrpc"] = "2.0";
// 				_DEBUG_OUT("%u call %s\n", fd, request_json["method"].get<string>().c_str());
// 				response_json["result"] = parent->process(request_json["method"], request_json["params"]);
// 
// 				//返回结果的json
// 				string &json = response_json.dump();
// 				string resp = http_response_header;
// 				resp += string("Content-Length: ") + to_string(json.length()) + "\r\n\r\n";
// 				resp += json;
// 
// 				if (SOCKET_ERROR == send(fd, resp.data(), resp.length(), 0))
// 					util_errno_exit("send failed:");
// 
// 				assert(temp_str.length() == (content_begin + content_len));
// 
// 				temp_str.clear();
// 			}
// 		}
// 
// 		try
// 		{
// 			async_read(fd, this->over, buf, piece_size - 1);
// 			_DEBUG_OUT("继续投递\n");
// 		}
// 		catch (exception e)
// 		{
// 			printf("error occur on client:%s\n", e.what());
// 			delete this;
// 		}
// 	}
// }
