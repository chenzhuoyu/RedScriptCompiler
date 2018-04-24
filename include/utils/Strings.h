#ifndef REDSCRIPT_UTILS_STRINGS_H
#define REDSCRIPT_UTILS_STRINGS_H

#include <string>
#include <algorithm>
#include <fmt/printf.h>

namespace RedScript::Utils::Strings
{
void lower(std::string &string);

std::string repr(const void *data, size_t size);
std::string hexdump(const void *data, size_t size);

template <typename Iterable>
static inline std::string join(const Iterable &list, const std::string &delim = "")
{
    /* joined result */
    std::string result;

    /* join each part */
    for (const auto &item : list)
    {
        /* only append delimeter if present */
        if (!delim.empty() && !result.empty())
            result.append(delim);

        /* append single part */
        result.append(item);
    }

    return result;
}

template <typename ... Args>
static inline std::string format(const fmt::CStringRef &fmt, const Args & ... args)
{
    typedef fmt::internal::ArgArray<sizeof...(Args)> ArgArray;
    typename ArgArray::Type array { ArgArray::template make<fmt::BasicFormatter<char>>(args)... };

    fmt::MemoryWriter mw;
    fmt::printf(mw, fmt, fmt::ArgList(fmt::internal::make_type(args...), array));
    return mw.str();
}
}

#endif /* REDSCRIPT_UTILS_STRINGS_H */
