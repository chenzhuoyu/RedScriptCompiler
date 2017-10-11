#ifndef REDSCRIPT_UTILS_NONCOPYABLE_H
#define REDSCRIPT_UTILS_NONCOPYABLE_H

namespace RedScript::Utils
{
struct NonCopyable
{
    NonCopyable() = default;
    NonCopyable(const NonCopyable &) = delete;
    NonCopyable &operator=(const NonCopyable &) = delete;
};
}

#endif /* REDSCRIPT_UTILS_NONCOPYABLE_H */
