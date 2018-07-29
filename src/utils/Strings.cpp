#include <cstring>
#include <sys/types.h>

#include "utils/Strings.h"

namespace RedScript::Utils::Strings
{
void strip(std::string &string)
{
    string.erase(string.begin(), std::find_if_not(string.begin(), string.end(), [](int v){ return std::isspace(v); }));
    string.erase(std::find_if_not(string.rbegin(), string.rend(), [](int v){ return std::isspace(v); }).base(), string.end());
}

void lower(std::string &string)
{
    /* transform string in-place */
    std::transform(
        string.begin(),
        string.end(),
        string.begin(),
        [](int c){ return std::tolower(c); }
    );
}

void split(std::vector<std::string> &result, const std::string &str, const std::string &delim)
{
    size_t end = 0;
    size_t start = 0;

    /* find every delimiter */
    while ((end = str.find(delim, start)) != std::string::npos)
    {
        result.push_back(str.substr(start, end - start));
        start = end + delim.size();
    }

    /* the last part */
    result.push_back(str.substr(start));
}

std::string repr(const void *data, size_t size)
{
    if (!data)
        return "(nullptr)";

    size_t n = size;
    const char *p = (const char *)data;
    static const char HexTable[] = "0123456789abcdef";

    char quote = '\'';
    std::string result = "\'";

    if (memchr(p, '\'', n) && !memchr(p, '"', n))
    {
        quote = '\"';
        result = "\"";
    }

    while (n--)
    {
        char ch = *p++;

        if (ch == quote)
        {
            result += '\\';
            result += ch;
            continue;
        }

        switch (ch)
        {
            case '\t'  : result += R"(\t)"; break;
            case '\n'  : result += R"(\n)"; break;
            case '\r'  : result += R"(\r)"; break;
            case '\a'  : result += R"(\a)"; break;
            case '\b'  : result += R"(\b)"; break;
            case '\f'  : result += R"(\f)"; break;
            case '\v'  : result += R"(\v)"; break;
            case '\033': result += R"(\e)"; break;
            case '\\'  : result += R"(\\)"; break;

            default:
            {
                if (ch >= ' ' && ch < 0x7f)
                {
                    result += ch;
                    break;
                }

                result += R"(\x)";
                result += HexTable[(ch & 0xf0) >> 4];
                result += HexTable[(ch & 0x0f) >> 0];
                break;
            }
        }
    }

    result += quote;
    return result;
}

std::string hexdump(const void *data, size_t size)
{
    char buffer[26] = {0};
    std::string result;

    if (!data)
        return "(nullptr)";

    for (int i = 0, r = 0; r < (size / 16 + (size % 16 != 0)); r++, i += 16)
    {
        snprintf(buffer, sizeof(buffer), "%08x | ", i);
        result += buffer;

        for (int c = i; c < i + 8; c++)
        {
            if (c >= size)
            {
                result += "   ";
                continue;
            }

            snprintf(buffer, sizeof(buffer), "%02x ", ((const uint8_t *)data)[c]);
            result += buffer;
        }

        result += " ";

        for (int c = i + 8; c < i + 16; c++)
        {
            if (c >= size)
            {
                result += "   ";
                continue;
            }

            snprintf(buffer, sizeof(buffer), "%02x ", ((const uint8_t *)data)[c]);
            result += buffer;
        }

        result += " | ";

        for (int c = i; c < i + 16; c++)
        {
            if (c >= size)
                result += " ";
            else if (!isprint(((const uint8_t *)data)[c]))
                result += ".";
            else
                result += ((const char *)data)[c];
        }

        result += "\n";
    }

    return result;
}
}
