#ifndef REDSCRIPT_UTILS_IMMOVABLE_H
#define REDSCRIPT_UTILS_IMMOVABLE_H

namespace RedScript::Utils
{
struct Immovable
{
    Immovable() = default;
    Immovable(Immovable &&) = delete;
    Immovable &operator=(Immovable &&) = delete;
};
}

#endif /* REDSCRIPT_UTILS_IMMOVABLE_H */
