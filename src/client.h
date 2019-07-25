#ifndef _ALICE_SRC_CLIENT_H
#define _ALICE_SRC_CLIENT_H

#include <Angel/EventLoop.h>
#include <Angel/TcpClient.h>

#include <vector>
#include <string>

using std::placeholders::_1;
using std::placeholders::_2;

namespace Alice {

class Client {
public:
    Client(Angel::EventLoop *loop, Angel::InetAddr& inetAddr)
        : _loop(loop),
        _client(loop, inetAddr, "Alice")
    {
        _client.setMessageCb(
                std::bind(&Client::onMessage, this, _1, _2));
    }
    void onMessage(const Angel::TcpConnectionPtr& conn, Angel::Buffer& buf)
    {
        parseResponse(buf);
    }
    void parseLine(const char *line, const char *linep);
    void parseResponse(Angel::Buffer& buf);
    void send();
    void start() { _client.start(); }
private:
    Angel::EventLoop *_loop;
    Angel::TcpClient _client;
    std::vector<std::string> _argv;
    std::string _message;
};
}

#endif
