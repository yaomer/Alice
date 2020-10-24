#include "internal.h"

#include "../server.h"

using namespace alice;
using namespace alice::ssdb;

#define BIND(f) std::bind(&DB::f, db.get(), std::placeholders::_1)

namespace alice {
    namespace ssdb {
        builtin_keys_t builtin_keys;
    }
}

void engine::server_cron()
{
    check_expire_keys();
}

void engine::creat_snapshot()
{

}

bool engine::is_creating_snapshot()
{
    return false;
}

bool engine::is_created_snapshot()
{
    return true;
}

std::string engine::get_snapshot_name()
{
    return server_conf.ssdb_snapshot_name;
}

void engine::load_snapshot()
{
    // TODO:
    db->reload();
}

// 随机删除一定数量的过期键
void engine::check_expire_keys()
{
    auto now = lru_clock;
    int keys = server_conf.ssdb_expire_check_keys;
    if (keys > db->expire_keys.size())
        keys = db->expire_keys.size();
    for (int j = 0; j < keys; j++) {
        if (db->expire_keys.empty()) break;
        auto randkey = get_rand_hash_key(db->expire_keys);
        size_t bucket = std::get<0>(randkey);
        size_t where = std::get<1>(randkey);
        for (auto it = db->expire_keys.cbegin(bucket);
                it != db->expire_keys.cend(bucket); ++it) {
            if (where-- == 0) {
                if (it->second <= now) {
                    db->del_key_with_expire(it->first);
                }
                break;
            }
        }
    }
}

engine::engine()
    : db(new DB())
{
    cmdtable = {
        { "EXISTS",     { -2, IS_READ,  BIND(exists) } },
        { "TYPE",       { -2, IS_READ,  BIND(type) } },
        { "TTL",        { -2, IS_READ,  BIND(ttl) } },
        { "PTTL",       { -2, IS_READ,  BIND(pttl) } },
        { "EXPIRE",     { -3, IS_WRITE, BIND(expire) } },
        { "PEXPIRE",    { -3, IS_WRITE, BIND(pexpire) } },
        { "DEL",        {  2, IS_WRITE, BIND(del) } },
        { "KEYS",       { -2, IS_READ,  BIND(keys) } },
        { "FLUSHDB",    { -1, IS_WRITE, BIND(flushdb) } },
        { "FLUSHALL",   { -1, IS_WRITE, BIND(flushall) } },
        { "RENAME",     { -3, IS_WRITE, BIND(rename) } },
        { "RENAMENX",   { -3, IS_WRITE, BIND(renamenx) } },
        { "SET",        {  3, IS_WRITE, BIND(set) } },
        { "SETNX",      { -3, IS_WRITE, BIND(setnx) } },
        { "GET",        { -2, IS_READ,  BIND(get) } },
        { "GETSET",     { -3, IS_WRITE, BIND(getset) } },
        { "APPEND",     { -3, IS_WRITE, BIND(append) } },
        { "STRLEN",     { -2, IS_READ,  BIND(strlen) } },
        { "MSET",       {  3, IS_WRITE, BIND(mset) } },
        { "MGET",       {  2, IS_READ,  BIND(mget) } },
        { "INCR",       { -2, IS_WRITE, BIND(incr) } },
        { "INCRBY",     { -3, IS_WRITE, BIND(incrby) } },
        { "DECR",       { -2, IS_WRITE, BIND(decr) } },
        { "DECRBY",     { -3, IS_WRITE, BIND(decrby) } },
        { "SETRANGE",   { -4, IS_WRITE, BIND(setrange) } },
        { "GETRANGE",   { -4, IS_READ,  BIND(getrange) } },
        { "LPUSH",      {  3, IS_WRITE, BIND(lpush) } },
        { "LPUSHX",     { -3, IS_WRITE, BIND(lpushx) } },
        { "RPUSH",      {  3, IS_WRITE, BIND(rpush) } },
        { "RPUSHX",     { -3, IS_WRITE, BIND(rpushx) } },
        { "LPOP",       { -2, IS_WRITE, BIND(lpop) } },
        { "RPOP",       { -2, IS_WRITE, BIND(rpop) } },
        { "RPOPLPUSH",  { -3, IS_WRITE, BIND(rpoplpush) } },
        { "LREM",       { -4, IS_WRITE, BIND(lrem) } },
        { "LLEN",       { -2, IS_READ,  BIND(llen) } },
        { "LINDEX",     { -3, IS_READ,  BIND(lindex) } },
        { "LSET",       { -4, IS_WRITE, BIND(lset) } },
        { "LRANGE",     { -4, IS_READ,  BIND(lrange) } },
        { "LTRIM",      { -4, IS_WRITE, BIND(ltrim) } },
        // { "BLPOP",      {  3, IS_READ,  BIND(blpop) } },
        // { "BRPOP",      {  3, IS_READ,  BIND(brpop) } },
        // { "BRPOPLPUSH", { -4, IS_READ,  BIND(brpoplpush) } },
        { "HSET",       { -4, IS_WRITE, BIND(hset) } },
        { "HSETNX",     { -4, IS_WRITE, BIND(hsetnx) } },
        { "HGET",       { -3, IS_READ,  BIND(hget) } },
        { "HEXISTS",    { -3, IS_READ,  BIND(hexists) } },
        { "HDEL",       {  3, IS_WRITE, BIND(hdel) } },
        { "HLEN",       { -2, IS_READ,  BIND(hlen) } },
        { "HSTRLEN",    { -3, IS_READ,  BIND(hstrlen) } },
        { "HINCRBY",    { -4, IS_WRITE, BIND(hincrby) } },
        { "HMSET",      {  4, IS_WRITE, BIND(hmset) } },
        { "HMGET",      {  3, IS_READ,  BIND(hmget) } },
        { "HKEYS",      { -2, IS_READ,  BIND(hkeys) } },
        { "HVALS",      { -2, IS_READ,  BIND(hvals) } },
        { "HGETALL",    { -2, IS_READ,  BIND(hgetall) } },
        { "SADD",       {  3, IS_WRITE, BIND(sadd) } },
        { "SISMEMBER",  { -3, IS_READ,  BIND(sismember) } },
        { "SPOP",       { -2, IS_WRITE, BIND(spop) } },
        { "SRANDMEMBER",{  2, IS_READ,  BIND(srandmember) } },
        { "SREM",       {  3, IS_WRITE, BIND(srem)  } },
        { "SMOVE",      { -4, IS_WRITE, BIND(smove) } },
        { "SCARD",      { -2, IS_READ,  BIND(scard) } },
        { "SMEMBERS",   { -2, IS_READ,  BIND(smembers) } },
        { "SINTER",     {  2, IS_READ,  BIND(sinter) } },
        { "SINTERSTORE",{  3, IS_WRITE, BIND(sinterstore) } },
        { "SUNION",     {  2, IS_READ,  BIND(sunion) } },
        { "SUNIONSTORE",{  3, IS_WRITE, BIND(sunionstore) } },
    };
}

void DB::set_builtin_keys()
{
    std::string value;
    auto s = db->Get(leveldb::ReadOptions(), builtin_keys.location, &value);
    if (s.IsNotFound()) {
        s = db->Put(leveldb::WriteOptions(), builtin_keys.location, builtin_keys.location);
        assert(s.ok());
    }
    s = db->Get(leveldb::ReadOptions(), builtin_keys.size, &value);
    if (s.IsNotFound()) {
        s = db->Put(leveldb::WriteOptions(), builtin_keys.size, "0");
        assert(s.ok());
    }
    s = db->Get(leveldb::ReadOptions(), builtin_keys.seq, &value);
    if (s.IsNotFound()) {
        s = db->Put(leveldb::WriteOptions(), builtin_keys.seq, "0");
        assert(s.ok());
    }
}

void DB::clear()
{
    leveldb::WriteBatch batch;
    auto it = db->NewIterator(leveldb::ReadOptions());
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        batch.Delete(it->key());
    }
    auto s = db->Write(leveldb::WriteOptions(), &batch);
    assert(s.ok());
    set_builtin_keys();
}

// EXISTS key
void DB::exists(context_t& con)
{
    std::string value;
    auto& key = con.argv[1];
    check_expire(key);
    auto s = db->Get(leveldb::ReadOptions(), encode_meta_key(key), &value);
    if (s.ok()) con.append(shared.n1);
    else if (s.IsNotFound()) con.append(shared.n0);
    else reterr(con, s);
}

// TYPE key
void DB::type(context_t& con)
{
    std::string value;
    auto& key = con.argv[1];
    check_expire(key);
    auto s = db->Get(leveldb::ReadOptions(), encode_meta_key(key), &value);
    if (s.IsNotFound()) ret(con, shared.none_type);
    check_status(con, s);
    switch (get_type(value)) {
    case ktype::tstring:
        con.append(shared.string_type);
        break;
    case ktype::tlist:
        con.append(shared.list_type);
        break;
    case ktype::thash:
        con.append(shared.hash_type);
        break;
    case ktype::tset:
        con.append(shared.set_type);
        break;
    case ktype::tzset:
        con.append(shared.zset_type);
        break;
    default:
        assert(0);
    }
}

void DB::_ttl(context_t& con, bool is_ttl)
{
    std::string value;
    auto& key = con.argv[1];
    auto s = db->Get(leveldb::ReadOptions(), encode_meta_key(key), &value);
    if (s.IsNotFound()) ret(con, shared.n_2);
    check_status(con, s);
    auto it = expire_keys.find(key);
    if (it == expire_keys.end()) ret(con, shared.n_1);
    auto ttl_ms = it->second - angel::util::get_cur_time_ms();
    if (is_ttl) ttl_ms /= 1000;
    con.append_reply_number(ttl_ms);
}

// TTL key
void DB::ttl(context_t& con)
{
    _ttl(con, true);
}

// PTTL key
void DB::pttl(context_t& con)
{
    _ttl(con, false);
}

void DB::_expire(context_t& con, bool is_expire)
{
    std::string value;
    auto& key = con.argv[1];
    auto expire = str2ll(con.argv[2]);
    if (str2numerr()) ret(con, shared.integer_err);
    if (expire <= 0) ret(con, shared.timeout_err);
    auto s = db->Get(leveldb::ReadOptions(), encode_meta_key(key), &value);
    if (s.IsNotFound()) ret(con, shared.n0);
    check_status(con, s);
    if (is_expire) expire *= 1000;
    expire += angel::util::get_cur_time_ms();
    add_expire_key(key, expire);
    con.append(shared.n1);
}

// EXPIRE key seconds
void DB::expire(context_t& con)
{
    _expire(con, true);
}

// PEXPIRE key milliseconds
void DB::pexpire(context_t& con)
{
    _expire(con, false);
}

void DB::flushdb(context_t& con)
{
    clear();
    con.append(shared.ok);
}

void DB::flushall(context_t& con)
{
    clear();
    con.append(shared.ok);
}

void DB::_rename(context_t& con, bool is_nx)
{
    std::string value, newvalue;
    auto& key = con.argv[1];
    auto& newkey = con.argv[2];
    check_expire(key);
    auto s = db->Get(leveldb::ReadOptions(), encode_meta_key(key), &value);
    if (s.IsNotFound()) ret(con, shared.no_such_key);
    if (key == newkey) ret(con, is_nx ? shared.n0 : shared.ok);
    s = db->Get(leveldb::ReadOptions(), encode_meta_key(newkey), &newvalue);
    leveldb::WriteBatch batch;
    if (s.ok()) {
        if (is_nx) ret(con, shared.n0);
        auto err = del_key_with_expire_batch(&batch, newkey);
        if (err) reterr(con, err.value());
    } else if (!s.IsNotFound()) {
        reterr(con, s);
    }
    rename_key(&batch, key, value, newkey);
    s = db->Write(leveldb::WriteOptions(), &batch);
    check_status(con, s);
    con.append(is_nx ? shared.n1 : shared.ok);
}

// RENAME key newkey
void DB::rename(context_t& con)
{
    _rename(con, false);
}

// RENAMENX key newkey
void DB::renamenx(context_t& con)
{
    _rename(con, true);
}

void DB::keys(context_t& con)
{
    if (con.argv[1].compare("*"))
        ret(con, shared.unknown_option);
    int nums = 0;
    con.reserve_multi_head();
    auto *it = db->NewIterator(leveldb::ReadOptions());
    it->Seek(builtin_keys.location);
    assert(it->Valid());
    for (it->Next(); it->Valid(); it->Next()) {
        auto key = it->key();
        if (key[0] != ktype::meta) break;
        key.remove_prefix(1);
        con.append_reply_string(key.ToString());
        nums++;
    }
    con.set_multi_head(nums);
}

// DEL key [key ...]
void DB::del(context_t& con)
{
    int dels = 0;
    std::string value;
    leveldb::WriteBatch batch;
    for (size_t i = 1; i < con.argv.size(); i++) {
        auto s = db->Get(leveldb::ReadOptions(), encode_meta_key(con.argv[i]), &value);
        if (s.ok()) {
            auto err = del_key_with_expire_batch(&batch, con.argv[i]);
            if (err) reterr(con, err.value());
            dels++;
        } else if (!s.IsNotFound())
            reterr(con, s);
    }
    auto s = db->Write(leveldb::WriteOptions(), &batch);
    check_status(con, s);
    con.append_reply_number(dels);
}

void DB::check_expire(const key_t& key)
{
    auto it = expire_keys.find(key);
    if (it == expire_keys.end()) return;
    if (it->second > lru_clock) return;
    auto err = del_key_with_expire(key);
    if (err) {
        log_error("leveldb: %s", err->ToString().c_str());
        return;
    }
    argv_t argv = { "DEL", key };
    __server->append_write_command(argv, nullptr, 0);
}

errstr_t DB::del_key(const key_t& key)
{
    std::string value;
    auto meta_key = encode_meta_key(key);
    auto s = db->Get(leveldb::ReadOptions(), meta_key, &value);
    if (s.IsNotFound()) return std::nullopt;
    if (!s.ok()) return s;
    auto type = get_type(value);
    switch (type) {
    case ktype::tstring: return del_string_key(key);
    case ktype::tlist: return del_list_key(key);
    case ktype::thash: return del_hash_key(key);
    case ktype::tset: return del_set_key(key);
    // case ktype::tzset: return del_zset_key(key);
    }
    assert(0);
}

errstr_t DB::del_key_batch(leveldb::WriteBatch *batch, const key_t& key)
{
    std::string value;
    auto meta_key = encode_meta_key(key);
    auto s = db->Get(leveldb::ReadOptions(), meta_key, &value);
    if (s.IsNotFound()) return std::nullopt;
    if (!s.ok()) return s;
    auto type = get_type(value);
    switch (type) {
    case ktype::tstring: return del_string_key_batch(batch, key);
    case ktype::tlist: return del_list_key_batch(batch, key);
    case ktype::thash: return del_hash_key_batch(batch, key);
    case ktype::tset: return del_set_key_batch(batch, key);
    // case ktype::tzset: return del_zset_key_batch(batch, key);
    default: assert(0);
    }
}

void DB::rename_key(leveldb::WriteBatch *batch, const key_t& key,
                    const std::string& value, const key_t& newkey)
{
    switch (get_type(value)) {
    case ktype::tstring:
        rename_string_key(batch, key, value, newkey);
        break;
    case ktype::tlist:
        rename_list_key(batch, key, value, newkey);
        break;
    case ktype::thash:
        rename_hash_key(batch, key, value, newkey);
        break;
    case ktype::tset:
        rename_set_key(batch, key, value, newkey);
        break;
    case ktype::tzset:
        // rename_zset_key(batch, key, value, newkey);
        break;
    default: assert(0);
    }
}

size_t DB::get_next_seq()
{
    size_t seq;
    std::string value;
    auto s = db->Get(leveldb::ReadOptions(), builtin_keys.seq, &value);
    assert(s.ok());
    seq = atoll(value.c_str());
    s = db->Put(leveldb::WriteOptions(), builtin_keys.seq, i2s(seq + 1));
    assert(s.ok());
    return seq;
}
