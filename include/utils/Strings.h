#ifndef REDSCRIPT_UTILS_STRINGS_H
#define REDSCRIPT_UTILS_STRINGS_H

#include <string>
#include <vector>
#include <algorithm>
#include <fmt/printf.h>

namespace RedScript::Utils::Strings
{
void strip(std::string &string);
void lower(std::string &string);
void split(std::vector<std::string> &result, const std::string &str, const std::string &delim);

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

template <typename Str, typename ... Args>
static inline std::string format(Str &&fmt, Args && ... args)
{
    /* use libfmt to format strings */
    return fmt::sprintf(std::forward<Str>(fmt), std::forward<Args>(args) ...);
}
}

#endif /* REDSCRIPT_UTILS_STRINGS_H */
