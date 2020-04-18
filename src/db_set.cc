#include "db.h"

using namespace Alice;

void DB::saddCommand(Context& con)
{
    auto& cmdlist = con.commandList();
    expireIfNeeded(cmdlist[1]);
    size_t members = cmdlist.size();
    auto it = find(cmdlist[1]);
    if (isFound(it)) {
        checkType(con, it, Set);
        Set& set = getSetValue(it);
        int retval = 0;
        for (size_t i = 2; i < members; i++) {
            if (set.find(cmdlist[i]) == set.end()) {
                set.emplace(cmdlist[i]);
                retval++;
            }
        }
        con.appendReplyNumber(retval);
    } else {
        Set set;
        for (size_t i = 2; i < members; i++)
            set.emplace(cmdlist[i]);
        insert(cmdlist[1], set);
        con.appendReplyNumber(members - 2);
    }
    touchWatchKey(cmdlist[1]);
}

void DB::sisMemberCommand(Context& con)
{
    auto& cmdlist = con.commandList();
    expireIfNeeded(cmdlist[1]);
    auto it = find(cmdlist[1]);
    if (!isFound(it)) db_return(con, reply.n0);
    checkType(con, it, Set);
    Set& set = getSetValue(it);
    if (set.find(cmdlist[2]) != set.end())
        con.append(reply.n1);
    else
        con.append(reply.n0);
}

void DB::spopCommand(Context& con)
{
    auto& cmdlist = con.commandList();
    expireIfNeeded(cmdlist[1]);
    auto it = find(cmdlist[1]);
    if (!isFound(it)) db_return(con, reply.nil);
    checkType(con, it, Set);
    Set& set = getSetValue(it);
    auto randkey = getRandHashKey(set);
    size_t bucketNumber = std::get<0>(randkey);
    size_t where = std::get<1>(randkey);
    for (auto it = set.cbegin(bucketNumber);
            it != set.cend(bucketNumber); it++)
        if (where-- == 0) {
            con.appendReplyString(*it);
            set.erase(set.find(*it));
            break;
        }
    if (set.empty()) delKeyWithExpire(cmdlist[1]);
    touchWatchKey(cmdlist[1]);
}

void DB::srandMemberCommand(Context& con)
{
    auto& cmdlist = con.commandList();
    expireIfNeeded(cmdlist[1]);
    int count = 0;
    if (cmdlist.size() > 2) {
        count = str2l(cmdlist[2].c_str());
        if (str2numberErr()) db_return(con, reply.integer_err);
        if (count == 0) db_return(con, reply.nil);
    }
    auto it = find(cmdlist[1]);
    if (!isFound(it)) db_return(con, reply.nil);
    checkType(con, it, Set);
    Set& set = getSetValue(it);
    // 类型转换，int -> size_t
    if (count >= static_cast<ssize_t>(set.size())) {
        con.appendReplyMulti(set.size());
        for (auto& it : set) {
            con.appendReplyString(it);
        }
        return;
    }
    if (count == 0 || count < 0) {
        if (count == 0)
            count = -1;
        con.appendReplyMulti(-count);
        while (count++ < 0) {
            auto randkey = getRandHashKey(set);
            size_t bucketNumber = std::get<0>(randkey);
            size_t where = std::get<1>(randkey);
            for (auto it = set.cbegin(bucketNumber);
                    it != set.cend(bucketNumber); it++) {
                if (where-- == 0) {
                    con.appendReplyString(*it);
                    break;
                }
            }
        }
        return;
    }
    con.appendReplyMulti(count);
    Set tset;
    while (count-- > 0) {
        auto randkey = getRandHashKey(set);
        size_t bucketNumber = std::get<0>(randkey);
        size_t where = std::get<1>(randkey);
        for (auto it = set.cbegin(bucketNumber);
                it != set.cend(bucketNumber); it++) {
            if (where-- == 0) {
                if (tset.find(*it) != tset.end()) {
                    count++;
                    break;
                }
                tset.insert(*it);
                break;
            }
        }
    }
    for (auto it : tset) {
        con.appendReplyString(it);
    }
}

void DB::sremCommand(Context& con)
{
    auto& cmdlist = con.commandList();
    expireIfNeeded(cmdlist[1]);
    size_t size = cmdlist.size();
    auto it = find(cmdlist[1]);
    if (!isFound(it)) db_return(con, reply.n0);
    checkType(con, it, Set);
    Set& set = getSetValue(it);
    int retval = 0;
    for (size_t i = 2; i < size; i++) {
        auto it = set.find(cmdlist[i]);
        if (it != set.end()) {
            set.erase(it);
            retval++;
        }
    }
    if (set.empty()) delKeyWithExpire(cmdlist[1]);
    touchWatchKey(cmdlist[1]);
    con.appendReplyNumber(retval);
}

void DB::smoveCommand(Context& con)
{
    auto& cmdlist = con.commandList();
    expireIfNeeded(cmdlist[1]);
    expireIfNeeded(cmdlist[2]);
    auto src = find(cmdlist[1]);
    if (!isFound(src)) db_return(con, reply.n0);
    checkType(con, src, Set);
    Set& srcSet = getSetValue(src);
    auto si = srcSet.find(cmdlist[3]);
    if (si == srcSet.end()) db_return(con, reply.n0);
    srcSet.erase(si);
    if (srcSet.empty()) delKeyWithExpire(cmdlist[1]);
    auto des = find(cmdlist[2]);
    if (isFound(des)) {
        checkType(con, des, Set);
        Set& desSet = getSetValue(des);
        desSet.emplace(cmdlist[3]);
    } else {
        Set set;
        set.emplace(cmdlist[3]);
        insert(cmdlist[2], set);
    }
    touchWatchKey(cmdlist[1]);
    touchWatchKey(cmdlist[2]);
    con.append(reply.n1);
}

void DB::scardCommand(Context& con)
{
    auto& cmdlist = con.commandList();
    expireIfNeeded(cmdlist[1]);
    auto it = find(cmdlist[1]);
    if (!isFound(it)) db_return(con, reply.n0);
    checkType(con, it, Set);
    Set& set = getSetValue(it);
    con.appendReplyNumber(set.size());
}

void DB::smembersCommand(Context& con)
{
    auto& cmdlist = con.commandList();
    expireIfNeeded(cmdlist[1]);
    auto it = find(cmdlist[1]);
    if (!isFound(it)) db_return(con, reply.nil);
    checkType(con, it, Set);
    Set& set = getSetValue(it);
    con.appendReplyMulti(set.size());
    for (auto& it : set) {
        con.appendReplyString(it);
    }
}

void DB::sinterCommand(Context& con)
{
    auto& cmdlist = con.commandList();
    size_t size = cmdlist.size();
    size_t minSize = 0, minIter = 0;
    // 挑选出元素最多的集合
    for (size_t i = 1; i < size; i++) {
        expireIfNeeded(cmdlist[i]);
        auto it = find(cmdlist[i]);
        if (!isFound(it)) db_return(con, reply.nil);
        checkType(con, it, Set);
        Set& set = getSetValue(it);
        if (minSize == 0) {
            minSize = set.size();
            minIter = i;
        } else if (minSize > set.size()) {
            minSize = set.size();
            minIter = i;
        }
    }
    Set rset;
    Set& set = getSetValue(find(cmdlist[minIter]));
    for (auto& it : set) {
        size_t i;
        for (i = 1; i < size; i++) {
            if (i == minIter)
                continue;
            Set& set = getSetValue(find(cmdlist[i]));
            if (set.find(it) == set.end())
                break;
        }
        if (i == size)
            rset.insert(it);
    }
    con.appendReplyMulti(rset.size());
    for (auto& it : rset)
        con.appendReplyString(it);
}

void DB::sinterStoreCommand(Context& con)
{
    auto& cmdlist = con.commandList();
    expireIfNeeded(cmdlist[1]);
    size_t size = cmdlist.size();
    size_t minSize = 0, minIter = 0;
    for (size_t i = 2; i < size; i++) {
        expireIfNeeded(cmdlist[i]);
        auto it = find(cmdlist[i]);
        if (!isFound(it)) db_return(con, reply.n0);
        checkType(con, it, Set);
        Set& set = getSetValue(it);
        if (minSize == 0) {
            minSize = set.size();
            minIter = i;
        } else if (minSize > set.size()) {
            minSize = set.size();
            minIter = i;
        }
    }
    Set rset;
    Set& set = getSetValue(find(cmdlist[minIter]));
    for (auto& it : set) {
        size_t i;
        for (i = 2; i < size; i++) {
            if (i == minIter)
                continue;
            Set& set = getSetValue(find(cmdlist[i]));
            if (set.find(it) == set.end())
                break;
        }
        if (i == size)
            rset.insert(it);
    }
    auto it = find(cmdlist[1]);
    if (isFound(it)) {
        checkType(con, it, Set);
        Set& set = getSetValue(it);
        set.swap(rset);
        con.appendReplyNumber(set.size());
    } else {
        con.appendReplyNumber(rset.size());
        insert(cmdlist[1], rset);
    }
}

void DB::sunionCommand(Context& con)
{
    auto& cmdlist = con.commandList();
    size_t size = cmdlist.size();
    Set rset;
    for (size_t i = 1; i < size; i++) {
        expireIfNeeded(cmdlist[i]);
        auto it = find(cmdlist[i]);
        if (isFound(it)) {
            checkType(con, it, Set);
            Set& set = getSetValue(it);
            for (auto& it : set) {
                rset.emplace(it);
            }
        }
    }
    if (rset.empty())
        con.append(reply.nil);
    else {
        con.appendReplyMulti(rset.size());
        for (auto& it : rset)
            con.appendReplyString(it);
    }
}

void DB::sunionStoreCommand(Context& con)
{
    auto& cmdlist = con.commandList();
    size_t size = cmdlist.size();
    Set rset;
    for (size_t i = 2; i < size; i++) {
        expireIfNeeded(cmdlist[i]);
        auto it = find(cmdlist[i]);
        if (isFound(it)) {
            checkType(con, it, Set);
            Set& set = getSetValue(it);
            for (auto& it : set) {
                rset.emplace(it);
            }
        }
    }
    auto it = find(cmdlist[1]);
    if (isFound(it)) {
        checkType(con, it, Set);
        Set& set = getSetValue(it);
        set.swap(rset);
        con.appendReplyNumber(set.size());
    } else {
        con.appendReplyNumber(rset.size());
        insert(cmdlist[1], rset);
    }
}

void DB::sdiffCommand(Context& con)
{
    // TODO:
}

void DB::sdiffStoreCommand(Context& con)
{
    // TODO:
}
