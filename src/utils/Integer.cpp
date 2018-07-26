
#include <utils/Integer.h>

#include "utils/Integer.h"
#include "utils/FreeList.h"

#include "engine/Memory.h"
#include "exceptions/ValueError.h"

#define STRINGIZE_(val) #val
#define STRINGIZE(val)  STRINGIZE_(val)

namespace RedScript::Utils
{
mpz_t Integer::_maxNegativeUInt;
mpz_t Integer::_minNegativeUInt;
Utils::FreeList<mpz_t, Integer::Alloc> Integer::_freeList;

Integer::Integer(const std::string &value, int radix) : _node(_freeList.alloc())
{
    /* try converting to integer */
    if (mpz_set_str(*(_value = _node->data), value.c_str(), radix) < 0)
    {
        throw Exceptions::ValueError(Utils::Strings::format(
            "\"%s\" cannot be converted to int with base %d",
            value,
            radix
        ));
    }
}

void Integer::Alloc::free(mpz_t *p)
{
    mpz_clear(*p);
    Engine::Memory::free(p);
}

mpz_t *Integer::Alloc::alloc(void)
{
    /* allocate a new `mpz_t` instance */
    void *p = Engine::Memory::alloc(sizeof(mpz_t));
    mpz_t *q = static_cast<mpz_t *>(p);

    /* initialize with at least 1024 bits */
    mpz_init2(*q, 1024);
    return q;
}

void Integer::swap(Integer &other)
{
    std::swap(_node, other._node);
    std::swap(_value, other._value);
}

bool Integer::isSafeInt(void) const
{
    /* INT64_MIN <= _value <= INT64_MAX */
    return (mpz_cmp_si(*_value, INT64_MIN) >= 0) &&
           (mpz_cmp_si(*_value, INT64_MAX) <= 0);
}

bool Integer::isSafeUInt(void) const
{
    /* 0 <= _value <= UINT64_MAX */
    return (mpz_cmp_ui(*_value, 0u) >= 0) &&
           (mpz_cmp_ui(*_value, UINT64_MAX) <= 0);
}

bool Integer::isSafeNegativeUInt(void) const
{
    /* -UINT64_MAX <= _value <= 0 */
    return (mpz_cmp(*_value, _maxNegativeUInt) <= 0) &&
           (mpz_cmp(*_value, _minNegativeUInt) >= 0);
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
    char *data = static_cast<char *>(mpz_export(nullptr, &size, 1, sizeof(uint8_t), 1, 0, *_value));

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
    char *str = mpz_get_str(nullptr, 10, *_value);
    std::string result = str;

    /* release the buffer */
    Engine::Memory::free(str);
    return std::move(result);
}

void Integer::shutdown(void)
{
    mpz_clear(_maxNegativeUInt);
    mpz_clear(_minNegativeUInt);
}

void Integer::initialize(void)
{
    /* use custom memory functions */
    mp_set_memory_functions(
        [](size_t size)                     { return Engine::Memory::alloc(size); },
        [](void *ptr, size_t, size_t size)  { return Engine::Memory::realloc(ptr, size); },
        [](void *ptr, size_t)               { Engine::Memory::free(ptr); }
    );

    /* integer constants */
    mpz_init_set_si(_maxNegativeUInt, 0);
    mpz_init_set_str(_minNegativeUInt, "-18446744073709551615", 10);
}
}