#ifndef _ALICE_SRC_SENTINEL_H
#define _ALICE_SRC_SENTINEL_H

#include <Angel/TcpClient.h>
#include <Angel/EventLoop.h>

#include <unordered_map>

#include "server.h"
#include "config.h"
#include "sentinel_instance.h"

namespace Alice {

class Sentinel {
public:
    explicit Sentinel(Angel::EventLoop *loop, Angel::InetAddr inetAddr);
    void init();
    uint64_t currentEpoch() const { return _currentEpoch; }
    SentinelInstance::SentinelInstanceMap& masters() { return *_masters; }
    void sendPingToServers();
    void sendInfoToServers();
    void sendPubMessageToServers();
    void recvPubMessageFromServer(const Angel::TcpConnectionPtr& conn, Angel::Buffer& buf);
    void updateSentinels(const char *s, const char *es);
    void subServer(const Angel::TcpConnectionPtr& conn);
    void askToBeElectedLeaderByOtherSentinels();
    bool getMasterDownState(const std::string& ip, int port);
    Angel::EventLoop *loop() { return _loop; }
    Alice::Server& server() { return _server; }
    void sentinelCron();
    void infoCommand(Context& con);
    void sentinelCommand(Context& con);
    void start()
    {
        _server.start();
        init();
    }
private:
    Angel::EventLoop *_loop;
    Alice::Server _server;
    SentinelInstance::SentinelInstanceMap *_masters;
    uint64_t _currentEpoch;
};

extern Alice::Sentinel *g_sentinel;

}

#endif
