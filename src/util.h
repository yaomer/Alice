#ifndef _ALICE_SRC_UTIL_H
#define _ALICE_SRC_UTIL_H

#include <unordered_map>
#include <unordered_set>
#include <random>
#include <tuple>

namespace Alice {

void setSelfRunId(char *buf);

extern thread_local char convert_buf[64];

template <typename T>
const char *convert(T value)
{
    T v = value;
    bool isunsigned = ((v |= ((T)1 << (sizeof(T)*8-1))) > 0 ? true : false);
    const char *format = isunsigned ? "%llu" : "%lld";
    snprintf(convert_buf, sizeof(convert_buf), format, value);
    return convert_buf;
}
const char *convert2f(double value);
long str2l(const char *nptr);
long long str2ll(const char *nptr);
double str2f(const char *nptr);
bool str2numberErr();
int parseLine(std::vector<std::string>& argv, const char *line, const char *linep);
// Hash可以是unordered_map或unorderd_set
// getRandHashKey()从Hash中挑选出一个随机元素，并返回这个元素
// 所在的bucket以及bucket中的位置
// 切记Hash不可为空，否则函数会陷入死循环!!!
template <typename Hash>
std::tuple<size_t, size_t> getRandHashKey(const Hash& h)
{
    size_t bucketNumber;
    std::uniform_int_distribution<size_t> u(0, h.bucket_count() - 1);
    std::default_random_engine e(clock());
    do {
        bucketNumber = u(e);
    } while (h.bucket_size(bucketNumber) == 0);
    std::uniform_int_distribution<size_t> _u(0, h.bucket_size(bucketNumber) - 1);
    return std::make_tuple(bucketNumber, _u(e));
}
void parseLineWithSeparator(std::vector<std::string>& argv,
                            const char *s,
                            const char *es,
                            char c);
#define UNUSED(x) ((void)(x))

}

#endif // _ALICE_SRC_UTIL_H
