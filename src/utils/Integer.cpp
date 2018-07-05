#include "utils/Integer.h"
#include "exceptions/ValueError.h"

static void mpfree(void *ptr, size_t size)
{
    void (*freefunc)(void *, size_t);
    mp_get_memory_functions(nullptr, nullptr, &freefunc);
    freefunc(ptr, size);
}

namespace RedScript::Utils
{
Integer::Integer(const std::string &value, int radix)
{
    /* try converting to integer */
    if (mpz_init_set_str(_value, value.c_str(), radix) < 0)
        throw Exceptions::ValueError(Utils::Strings::format("\"%s\" cannot be converted to int with base %d", value, radix));
}

void Integer::swap(Integer &other)
{
    mpz_t temp = {};
    memcpy(temp         , _value       , sizeof(mpz_t));
    memcpy(_value       , other._value , sizeof(mpz_t));
    memcpy(other._value , temp         , sizeof(mpz_t));
}

void Integer::assign(const Integer &other)
{
    mpz_clear(_value);
    mpz_init_set(_value, other._value);
}

bool Integer::isSafeInt(void) const
{
    /* INT64_MIN <= _value <= INT64_MAX */
    return (mpz_cmp_si(_value, INT64_MIN) >= 0) &&
           (mpz_cmp_si(_value, INT64_MAX) <= 0);
}

bool Integer::isSafeUInt(void) const
{
    /* 0 <= _value <= UINT64_MAX */
    return (mpz_cmp_ui(_value, 0u) >= 0) &&
           (mpz_cmp_ui(_value, UINT64_MAX) <= 0);
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
    mpfree(data, size);
    return hash;
}

std::string Integer::toString(void) const
{
    /* convert to string */
    auto str = mpz_get_str(nullptr, 10, _value);
    std::string result = str;

    /* release the buffer */
    mpfree(str, result.size() + 1);
    return std::move(result);
}
}