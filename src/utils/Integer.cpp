#include "utils/Integer.h"
#include "utils/FreeList.h"

#include "engine/Memory.h"
#include "exceptions/ValueError.h"

#define STRINGIZE_(val) #val
#define STRINGIZE(val)  STRINGIZE_(val)

namespace RedScript::Utils
{
Integer::Integer(const std::string &value, int radix)
{
    /* try converting to integer */
    if (mpz_init_set_str(_value, value.c_str(), radix) < 0)
    {
        throw Exceptions::ValueError(Utils::Strings::format(
            "\"%s\" cannot be converted to int with base %d",
            value,
            radix
        ));
    }
}

uint64_t Integer::toHash(void) const
{
    /* is a safe unsigned integer */
    if (isSafeUInt())
        return toUInt();

    /* is a safe signed integer */
    if (isSafeInt())
        return static_cast<uint64_t>(toInt());

    /* export the data */
    size_t size;
    char *data = static_cast<char *>(mpz_export(nullptr, &size, 1, sizeof(uint8_t), 1, 0, _value));

    /* calculate the hash */
    auto str = std::string(data, size);
    uint64_t hash = std::hash<std::string>()(str);

    /* release the space */
    Engine::Memory::free(data);
    return hash;
}

std::string Integer::toString(void) const
{
    /* convert to string */
    char *str = mpz_get_str(nullptr, 10, _value);
    std::string result = str;

    /* release the buffer */
    Engine::Memory::free(str);
    return std::move(result);
}

void Integer::initialize(void)
{
    /* use custom memory functions */
    mp_set_memory_functions(
        [](size_t size)                     { return Engine::Memory::alloc(size); },
        [](void *ptr, size_t, size_t size)  { return Engine::Memory::realloc(ptr, size); },
        [](void *ptr, size_t)               { Engine::Memory::free(ptr); }
    );
}
}