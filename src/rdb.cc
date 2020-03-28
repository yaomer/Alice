#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <tuple>

#include "rdb.h"
#include "server.h"

using namespace Alice;

namespace Alice {

    static unsigned char magic[5] = { 0x41, 0x4c, 0x49, 0x43, 0x45 };
    static unsigned char select_db = 0xfe;
    static unsigned char eof = 0xff;
    static unsigned char string_type = 0;
    static unsigned char list_type = 1;
    static unsigned char set_type = 2;
    static unsigned char hash_type = 3;
    static unsigned char zset_type = 4;
    static unsigned char expire_key = 5;
    static unsigned char rdb_6bit_len = 0;
    static unsigned char rdb_14bit_len = 1;
    static unsigned char rdb_32bit_len = 0x80;
    static unsigned char rdb_64bit_len = 0x81;
}

void Rdb::save()
{
    char tmpfile[16];
    strcpy(tmpfile, "tmp.XXXXX");
    mktemp(tmpfile);
    _fd = open(tmpfile, O_RDWR | O_CREAT | O_APPEND, 0660);
    append(magic, 5);
    int64_t now = Angel::nowMs();
    int index = 0;
    for (auto& db : _dbServer->dbs()) {
        if (db->hashMap().empty()) {
            index++;
            continue;
        }
        saveLen(select_db);
        saveLen(index);
        for (auto it = db->hashMap().begin(); it != db->hashMap().end(); ++it) {
            auto expire = db->expireMap().find(it->first);
            if (expire != db->expireMap().end()) {
                if (expire->second <= now) {
                    db->delKeyWithExpire(it->first);
                    continue;
                } else {
                    saveLen(expire_key);
                    int64_t timeval = expire->second;
                    append(&timeval, 8);
                }
            }
            if (isXXType(it, DB::String)) {
                saveString(it);
            } else if (isXXType(it, DB::List)) {
                saveList(it);
            } else if (isXXType(it, DB::Set)) {
                saveSet(it);
            } else if (isXXType(it, DB::Hash)) {
                saveHash(it);
            } else if (isXXType(it, DB::Zset)) {
                saveZset(it);
            }
        }
        index++;
    }
    saveLen(eof);
    if (_buffer.size() > 0) flush();
    g_server->fsyncBackground(_fd);
    rename(tmpfile, g_server_conf.rdb_file.c_str());
}

void Rdb::saveBackground()
{
    _childPid = fork();
    logInfo("Background saving started by pid %ld", _childPid);
    if (_childPid == 0) {
        childPidReset();
        save();
        abort();
    }
}

// 借用redis中的两个编解码长度的函数
int Rdb::saveLen(uint64_t len)
{
    unsigned char buf[2];
    size_t writeBytes = 0;

    if (len < (1 << 6)) {
        buf[0] = (len & 0xff) | (rdb_6bit_len << 6);
        append(buf, 1);
        writeBytes = 1;
    } else if (len < (1 << 14)) {
        buf[0] = ((len >> 8) & 0xff) | (rdb_14bit_len << 6);
        buf[1] = len & 0xff;
        append(buf, 2);
        writeBytes = 2;
    } else if (len <= UINT32_MAX) {
        buf[0] = rdb_32bit_len;
        append(buf, 1);
        uint32_t len32 = htonl(len);
        append(&len32, 4);
        writeBytes = 1 + 4;
    } else {
        buf[0] = rdb_64bit_len;
        append(buf, 1);
        append(&len, 8);
        writeBytes = 1 + 8;
    }
    return writeBytes;
}

int Rdb::loadLen(char *ptr, uint64_t *lenptr)
{
    unsigned char buf[2] = {
        static_cast<unsigned char>(ptr[0]), 0 };
    int type = (buf[0] & 0xc0) >> 6;
    size_t readBytes = 0;

    if (type == rdb_6bit_len) {
        *lenptr = buf[0] & 0x3f;
        readBytes = 1;
    } else if (type == rdb_14bit_len) {
        buf[1] = static_cast<unsigned char>(ptr[1]);
        *lenptr = ((buf[0] & 0x3f) << 8) | buf[1];
        readBytes = 2;
    } else if (type == rdb_32bit_len) {
        uint32_t len32 = *reinterpret_cast<uint32_t*>(&ptr[1]);
        *lenptr = ntohl(len32);
        readBytes = 5;
    } else {
        *lenptr = *reinterpret_cast<uint64_t*>(&ptr[1]);
        readBytes = 8;
    }
    return readBytes;
}

void Rdb::saveString(const Iterator& it)
{
    saveLen(string_type);
    saveLen(it->first.size());
    append(it->first);
    DB::String& string = getXXType(it, DB::String&);
    saveLen(string.size());
    append(string);
}

void Rdb::saveList(const Iterator& it)
{
    saveLen(list_type);
    saveLen(it->first.size());
    append(it->first);
    DB::List& list = getXXType(it, DB::List&);
    saveLen(list.size());
    for (auto& it : list) {
        saveLen(it.size());
        append(it);
    }
}

void Rdb::saveSet(const Iterator& it)
{
    saveLen(set_type);
    saveLen(it->first.size());
    append(it->first);
    DB::Set& set = getXXType(it, DB::Set&);
    saveLen(set.size());
    for (auto& it : set) {
        saveLen(it.size());
        append(it);
    }
}

void Rdb::saveHash(const Iterator& it)
{
    saveLen(hash_type);
    saveLen(it->first.size());
    append(it->first);
    DB::Hash& hash = getXXType(it, DB::Hash&);
    saveLen(hash.size());
    for (auto& it : hash) {
        saveLen(it.first.size());
        append(it.first);
        saveLen(it.second.size());
        append(it.second);
    }
}

void Rdb::saveZset(const Iterator& it)
{
    saveLen(zset_type);
    saveLen(it->first.size());
    append(it->first);
    auto& tuple = getXXType(it, DB::Zset&);
    DB::_Zset& zset = std::get<0>(tuple);
    saveLen(zset.size());
    for (auto& it : zset) {
        saveLen(strlen(convert2f(std::get<0>(it))));
        append(convert2f(std::get<0>(it)));
        saveLen(std::get<1>(it).size());
        append(std::get<1>(it));
    }
}

void Rdb::load()
{
    int fd = open(g_server_conf.rdb_file.c_str(), O_RDONLY);
    struct stat st;
    fstat(fd, &st);
    size_t size = st.st_size;
    void *start = mmap(nullptr, size, PROT_READ, MAP_SHARED, fd, 0);
    if (start == MAP_FAILED) return;
    char *buf = reinterpret_cast<char*>(start);
    if (size < 5 || strncmp(buf, "ALICE", 5))
        return;
    buf += 5;
    uint64_t typelen;
    int64_t timeval = 0;
    while (true) {
        size_t len = loadLen(buf, &typelen);
        if (typelen == eof)
            break;
        if (typelen == select_db) {
            buf += len;
            buf += loadLen(buf, &typelen);
            _curDb = _dbServer->selectDb(typelen);
            continue;
        }
        if (buf[0] == expire_key) {
            timeval = *reinterpret_cast<int64_t*>(buf+1);
            buf += 9;
        } else if (buf[0] == string_type) {
            buf = loadString(buf + 1, &timeval);
        } else if (buf[0] == list_type) {
            buf = loadList(buf + 1, &timeval);
        } else if (buf[0] == set_type) {
            buf = loadSet(buf + 1, &timeval);
        } else if (buf[0] == hash_type) {
            buf = loadHash(buf + 1, &timeval);
        } else
            buf = loadZset(buf + 1, &timeval);
    }
    munmap(start, size);
    close(fd);
}

// loadXXX()不能使用stack-array，不然如果key/value太长的话
// 会导致stack-overflow

char *Rdb::loadString(char *ptr, int64_t *tvptr)
{
    uint64_t keylen, vallen;
    ptr += loadLen(ptr, &keylen);
    std::string key(ptr, keylen);
    ptr += keylen;
    ptr += loadLen(ptr, &vallen);
    char *vptr = ptr;
    ptr += vallen;
    _curDb->hashMap().emplace(key, std::string(vptr, vallen));
    if (*tvptr > 0) {
        _curDb->expireMap()[key] = *tvptr;
        *tvptr = 0;
    }
    return ptr;
}

char *Rdb::loadList(char *ptr, int64_t *tvptr)
{
    uint64_t keylen, vallen;
    ptr += loadLen(ptr, &keylen);
    std::string key(ptr, keylen);
    ptr += keylen;
    uint64_t listlen;
    ptr += loadLen(ptr, &listlen);
    DB::List list;
    while (listlen-- > 0) {
        ptr += loadLen(ptr, &vallen);
        list.emplace_back(std::string(ptr, vallen));
        ptr += vallen;
    }
    _curDb->hashMap().emplace(key, std::move(list));
    if (*tvptr > 0) {
        _curDb->expireMap()[key] = *tvptr;
        *tvptr = 0;
    }
    return ptr;
}

char *Rdb::loadSet(char *ptr, int64_t *tvptr)
{
    uint64_t keylen, vallen;
    ptr += loadLen(ptr, &keylen);
    std::string key(ptr, keylen);
    ptr += keylen;
    uint64_t setlen;
    ptr += loadLen(ptr, &setlen);
    DB::Set set;
    while (setlen-- > 0) {
        ptr += loadLen(ptr, &vallen);
        set.emplace(std::string(ptr, vallen));
        ptr += vallen;
    }
    _curDb->hashMap().emplace(key, std::move(set));
    if (*tvptr > 0) {
        _curDb->expireMap()[key] = *tvptr;
        *tvptr = 0;
    }
    return ptr;
}

char *Rdb::loadHash(char *ptr, int64_t *tvptr)
{
    uint64_t keylen, vallen;
    ptr += loadLen(ptr, &keylen);
    std::string key(ptr, keylen);
    ptr += keylen;
    uint64_t hashlen;
    ptr += loadLen(ptr, &hashlen);
    DB::Hash hash;
    uint64_t fieldlen;
    while (hashlen-- > 0) {
        ptr += loadLen(ptr, &fieldlen);
        char *fieldptr = ptr;
        ptr += fieldlen;
        ptr += loadLen(ptr, &vallen);
        hash.emplace(std::string(fieldptr, fieldlen), std::string(ptr, vallen));
        ptr += vallen;
    }
    _curDb->hashMap().emplace(key, std::move(hash));
    if (*tvptr > 0) {
        _curDb->expireMap()[key] = *tvptr;
        *tvptr = 0;
    }
    return ptr;
}

char *Rdb::loadZset(char *ptr, int64_t *tvptr)
{
    uint64_t keylen, vallen;
    ptr += loadLen(ptr, &keylen);
    std::string key(ptr, keylen);
    ptr += keylen;
    uint64_t zsetlen;
    ptr += loadLen(ptr, &zsetlen);
    DB::_Zset zset;
    DB::_Zmap zmap;
    uint64_t slen;
    while (zsetlen-- > 0) {
        ptr += loadLen(ptr, &slen);
        std::string scorestr(ptr, slen);
        double score = atof(scorestr.c_str());
        ptr += slen;
        ptr += loadLen(ptr, &vallen);
        std::string value(ptr, vallen);
        zmap.emplace(value, score);
        zset.emplace(score, std::move(value));
        ptr += vallen;
    }
    _curDb->hashMap().emplace(key, std::make_tuple(zset, zmap));
    if (*tvptr > 0) {
        _curDb->expireMap()[key] = *tvptr;
        *tvptr = 0;
    }
    return ptr;
}

void Rdb::append(const std::string& data)
{
    append(data.data(), data.size());
}

void Rdb::append(const void *data, size_t len)
{
    _buffer.append(reinterpret_cast<const char*>(data), len);
    if (_buffer.size() >= buffer_flush_size)
        flush();
}

void Rdb::flush()
{
    writeToFile(_fd, _buffer.data(), _buffer.size());
    _buffer.clear();
}

void Rdb::appendSyncBuffer(const Context::CommandList& cmdlist,
                           const char *query, size_t len)
{
    if (query) _syncBuffer.append(query, len);
    else DBServer::appendCommand(_syncBuffer, cmdlist);
}
