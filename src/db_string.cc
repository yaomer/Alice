#include "db.h"

using namespace Alice;

#define SET_NX 0x01
#define SET_XX 0x02
#define SET_EX 0x04
#define SET_PX 0x08

namespace Alice {
    thread_local std::unordered_map<std::string, int> setops = {
        { "NX", SET_NX },
        { "XX", SET_XX },
        { "EX", SET_EX },
        { "PX", SET_PX },
    };
}

void DB::setCommand(Context& con)
{
    unsigned cmdops = 0;
    int64_t expire = 0;
    auto& cmdlist = con.commandList();
    size_t len = cmdlist.size();
    for (size_t i = 3; i < len; i++) {
        std::transform(cmdlist[i].begin(), cmdlist[i].end(), cmdlist[i].begin(), ::toupper);
        auto op = setops.find(cmdlist[i]);
        if (op != setops.end()) cmdops |= op->second;
        else goto syntax_err;
        switch (op->second) {
        case SET_NX: case SET_XX:
            break;
        case SET_EX: case SET_PX: {
            if (i + 1 >= len) goto syntax_err;
            expire = str2ll(cmdlist[++i].c_str());
            if (str2numberErr()) {
                con.append(db_return_integer_err);
                return;
            }
            if (op->second == SET_EX)
                expire *= 1000;
            break;
        }
        default:
            goto syntax_err;
        }
    }
    if ((cmdops & SET_NX) && (cmdops & SET_XX))
        goto syntax_err;
    if (cmdops & SET_NX) {
        if (!isFound(find(cmdlist[1]))) {
            insert(cmdlist[1], cmdlist[2]);
            con.append(db_return_ok);
        } else {
            con.append(db_return_nil);
            return;
        }
    } else if (cmdops & SET_XX) {
        if (isFound(find(cmdlist[1]))) {
            insert(cmdlist[1], cmdlist[2]);
            con.append(db_return_ok);
        } else {
            con.append(db_return_nil);
            return;
        }
    } else {
        insert(cmdlist[1], cmdlist[2]);
        con.append(db_return_ok);
    }
    delExpireKey(cmdlist[1]);
    touchWatchKey(cmdlist[1]);
    if (cmdops & (SET_EX | SET_PX)) {
        addExpireKey(cmdlist[1], expire);
    }
    return;
syntax_err:
    con.append(db_return_syntax_err);
}

void DB::setnxCommand(Context& con)
{
    auto& cmdlist = con.commandList();
    if (!isFound(find(cmdlist[1]))) {
        insert(cmdlist[1], cmdlist[2]);
        touchWatchKey(cmdlist[1]);
        con.append(db_return_1);
    } else {
        con.append(db_return_0);
    }
}

void DB::getCommand(Context& con)
{
    auto& cmdlist = con.commandList();
    expireIfNeeded(cmdlist[1]);
    auto it = find(cmdlist[1]);
    if (!isFound(it)) {
        con.append(db_return_nil);
        return;
    }
    checkType(con, it, String);
    auto& value = getStringValue(it);
    appendReplySingleStr(con, value);
}

void DB::getSetCommand(Context& con)
{
    auto& cmdlist = con.commandList();
    expireIfNeeded(cmdlist[1]);
    touchWatchKey(cmdlist[1]);
    auto it = find(cmdlist[1]);
    if (!isFound(it)) {
        insert(cmdlist[1], cmdlist[2]);
        con.append(db_return_nil);
        return;
    }
    checkType(con, it, String);
    String oldvalue = getStringValue(it);
    insert(cmdlist[1], cmdlist[2]);
    appendReplySingleStr(con, oldvalue);
}

void DB::strlenCommand(Context& con)
{
    auto& cmdlist = con.commandList();
    expireIfNeeded(cmdlist[1]);
    auto it = find(cmdlist[1]);
    if (!isFound(it)) {
        con.append(db_return_0);
        return;
    }
    checkType(con, it, String);
    String& value = getStringValue(it);
    appendReplyNumber(con, value.size());
}

void DB::appendCommand(Context& con)
{
    auto& cmdlist = con.commandList();
    expireIfNeeded(cmdlist[1]);
    touchWatchKey(cmdlist[1]);
    auto it = find(cmdlist[1]);
    if (!isFound(it)) {
        insert(cmdlist[1], cmdlist[2]);
        appendReplyNumber(con, cmdlist[2].size());
        return;
    }
    checkType(con, it, String);
    String& string = getStringValue(it);
    string.append(cmdlist[2]);
    appendReplyNumber(con, string.size());
}

void DB::msetCommand(Context& con)
{
    auto& cmdlist = con.commandList();
    size_t size = cmdlist.size();
    if (size % 2 == 0) {
        con.append("-ERR wrong number of arguments for '" + cmdlist[0] + "'\r\n");
        return;
    }
    for (size_t i = 1; i < size; i += 2) {
        expireIfNeeded(cmdlist[i]);
        insert(cmdlist[i], cmdlist[i+1]);
        touchWatchKey(cmdlist[i]);
    }
    con.append(db_return_ok);
}

void DB::mgetCommand(Context& con)
{
    auto& cmdlist = con.commandList();
    size_t size = cmdlist.size();
    appendReplyMulti(con, size - 1);
    for (size_t i = 1; i < size; i++) {
        expireIfNeeded(cmdlist[i]);
        auto it = find(cmdlist[i]);
        if (isFound(it)) {
            if (!isXXType(it, String)) {
                con.append(db_return_nil);
                continue;
            }
            String& value = getStringValue(it);
            appendReplySingleStr(con, value);
        } else
            con.append(db_return_nil);
    }
}

void DB::incr(Context& con, int64_t incr)
{
    auto& cmdlist = con.commandList();
    expireIfNeeded(cmdlist[1]);
    auto it = find(cmdlist[1]);
    if (isFound(it)) {
        checkType(con, it, String);
        String& value = getStringValue(it);
        int64_t number = str2ll(value.c_str());
        if (!str2numberErr()) {
            number += incr;
            insert(cmdlist[1], String(convert(number)));
            appendReplyNumber(con, number);
        } else {
            con.append(db_return_integer_err);
            return;
        }
    } else {
        insert(cmdlist[1], String(convert(incr)));
        appendReplyNumber(con, incr);
    }
    touchWatchKey(cmdlist[1]);
}

void DB::incrCommand(Context& con)
{
    incr(con, 1);
}

void DB::incrbyCommand(Context& con)
{
    auto& cmdlist = con.commandList();
    int64_t increment = str2ll(cmdlist[2].c_str());
    if (str2numberErr()) {
        con.append(db_return_integer_err);
        return;
    }
    incr(con, increment);
}

void DB::decrCommand(Context& con)
{
    incr(con, -1);
}

void DB::decrbyCommand(Context& con)
{
    auto& cmdlist = con.commandList();
    int64_t decrement = str2ll(cmdlist[2].c_str());
    if (str2numberErr()) {
        con.append(db_return_integer_err);
        return;
    }
    incr(con, -decrement);
}
