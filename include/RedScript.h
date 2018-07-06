#ifndef REDSCRIPT_H
#define REDSCRIPT_H

#include <stddef.h>

namespace RedScript
{
void shutdown(void);
void initialize(size_t stack, size_t young, size_t old, size_t perm);
}

#endif /* REDSCRIPT_H */
