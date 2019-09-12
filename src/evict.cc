#include "server.h"
#include "config.h"

using namespace Alice;

void DBServer::freeMemoryIfNeeded()
{
    if (g_server_conf.maxmemory == 0) return;
    ssize_t memorySize = getProcMemory();
    if (memorySize < 0) {
        logError("getProcMemory: %s", Angel::strerrno());
        return;
    }
    if (memorySize < g_server_conf.maxmemory) return;
    switch (g_server_conf.maxmemory_policy) {
    case EVICT_ALLKEYS_LRU: evictAllkeysWithLru(); break;
    case EVICT_VOLATILE_LRU: evictVolatileWithLru(); break;
    case EVICT_ALLKEYS_RANDOM: evictAllkeysWithRandom(); break;
    case EVICT_VOLATILE_RANDOM: evictVolatileWithRandom(); break;
    case EVICT_VOLATILE_TTL: evictVolatileWithTtl(); break;
    }
}

// 真正的lru算法需要一个双端链表来保存维护所有键的lru关系，这需要额外的内存，
// 所以我们这里只是近似模拟一下lru算法
// 我们从当前操作的数据库中随机选出g_server_conf.maxmemory_samples个键，
// 剔除掉其中lru值最大的，即相对来说最近没有被使用的
// 显然maxmemory_samples越大，就越接近真正的lru算法，但相对的会有一定的性能
// 损失，所以需要在这两者之间达到一定的平衡
void DBServer::evictAllkeysWithLru()
{
    int64_t lastlru = 0;
    int evict[2] = { -1 };
    auto& hash = db()->hashMap();
    for (int i = 0; i < g_server_conf.maxmemory_samples; i++) {
        auto randkey = getRandHashKey(hash);
        auto bucketNumber = std::get<0>(randkey);
        auto where = std::get<1>(randkey);
        auto j = where;
        for (auto it = hash.cbegin(bucketNumber); it != hash.end(bucketNumber); ++it) {
            if (j-- == 0) {
                if (it->second.lru() > lastlru) {
                    lastlru = it->second.lru();
                    evict[0] = bucketNumber;
                    evict[1] = where;
                    break;
                }
            }
        }
    }
    if (evict[0] == -1) return;
    evictKey(hash, evict);
}

void DBServer::evictVolatileWithLru()
{
    int64_t lastlru = 0;
    int evict[2] = { -1 };
    auto& hash = db()->expireMap();
    for (int i = 0; i < g_server_conf.maxmemory_samples; i++) {
        auto randkey = getRandHashKey(hash);
        auto bucketNumber = std::get<0>(randkey);
        auto where = std::get<1>(randkey);
        auto j = where;
        for (auto it = hash.cbegin(bucketNumber); it != hash.end(bucketNumber); ++it) {
            if (j-- == 0) {
                auto e = db()->hashMap().find(it->first);
                if (e->second.lru() > lastlru) {
                    lastlru = e->second.lru();
                    evict[0] = bucketNumber;
                    evict[1] = where;
                    break;
                }
            }
        }
    }
    if (evict[0] == -1) return;
    evictKey(hash, evict);
}

void DBServer::evictAllkeysWithRandom()
{
    int evict[2];
    auto& hash = db()->hashMap();
    auto randkey = getRandHashKey(hash);
    evict[0] = std::get<0>(randkey);
    evict[1] = std::get<1>(randkey);
    evictKey(hash, evict);
}

void DBServer::evictVolatileWithRandom()
{
    auto& hash = db()->expireMap();
    auto randkey = getRandHashKey(hash);
    auto bucketNumber = std::get<0>(randkey);
    auto where = std::get<1>(randkey);
    for (auto it = hash.cbegin(bucketNumber); it != hash.end(bucketNumber); ++it) {
        if (where-- == 0) {
            auto e = db()->hashMap().find(it->first);
            evictKey(e->first);
            break;
        }
    }
}

// 从当前操作的数据库的expireMap中随机选出g_server_conf.maxmemory_samples个键,
// 剔除掉其中ttl值最小的，即存活时间最短的
void DBServer::evictVolatileWithTtl()
{
    int64_t ttl = 0;
    int evict[2] = { -1 };
    auto& hash = db()->expireMap();
    for (int i = 0; i < g_server_conf.maxmemory_samples; i++) {
        auto randkey = getRandHashKey(hash);
        auto bucketNumber = std::get<0>(randkey);
        auto where = std::get<1>(randkey);
        auto j = where;
        for (auto it = hash.cbegin(bucketNumber); it != hash.end(bucketNumber); ++it) {
            if (j-- == 0) {
                if (it->second < ttl) {
                    ttl = it->second;
                    evict[0] = bucketNumber;
                    evict[1] = where;
                    break;
                }
            }
        }
    }
    if (evict[0] == -1) return;
    evictKey(hash, evict);
}

void DBServer::evictKey(const std::string& key)
{
    Context::CommandList clist = { "DEL", key };
    db()->delKeyWithExpire(key);
    appendWriteCommand(clist);
}

#if defined (__APPLE__)
#include <libproc.h>
#endif

// 获取当前进程已使用的内存大小(bytes)
ssize_t DBServer::getProcMemory()
{
    pid_t pid = g_server->dbServer().getpid();
#if defined (__APPLE__)
    struct proc_taskinfo pti;
    int ret = proc_pidinfo(pid, PROC_PIDTASKINFO, 0, &pti, PROC_PIDTASKINFO_SIZE);
    if (ret != PROC_PIDTASKINFO_SIZE) return -1;
    return pti.pti_resident_size;
#elif defined (__linux__)
    char name[32] = { 0 };
    char buf[1024] = { 0 };
    int vmrss = -1;
    snprintf(name, sizeof(name), "/proc/%d/status", pid);
    FILE *fp = fopen(name, "r");
    if (fp == nullptr) return -1;
    while (fgets(buf, sizeof(buf), fp)) {
        if (strncasecmp(buf, "vmrss:", 6) == 0) {
            sscanf(buf, "%s %d", name, &vmrss);
            vmrss *= 1024;
            break;
        }
    }
    fclose(fp);
    return vmrss;
#else
    logError("unknown platform");
    return -1;
#endif
}
