#ifndef REDSCRIPT_UTILS_POINTERS_H
#define REDSCRIPT_UTILS_POINTERS_H

namespace RedScript::Pointers
{
template <typename T>
static inline void deleteAndSetNull(T *&object)
{
    T *ptr = object;
    object = nullptr;
    delete ptr;
}
}

#endif /* REDSCRIPT_UTILS_POINTERS_H */
