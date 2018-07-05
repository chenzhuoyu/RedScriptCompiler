#ifndef REDSCRIPT_UTILS_INTEGER_H
#define REDSCRIPT_UTILS_INTEGER_H

#include <string>
#include <cstdint>
#include <mpir.h>

namespace RedScript::Utils
{
class Integer
{
    mpz_t _value;

public:
    Integer() { mpz_init(_value); }
   ~Integer() { mpz_clear(_value); }

public:
    Integer(int64_t value) { mpz_init_set_sx(_value, value); }
    Integer(uint64_t value) { mpz_init_set_ux(_value, value); }
    Integer(const std::string &value, int radix = 10);

public:
    Integer(Integer &&other)      { mpz_init(_value); swap(other); }
    Integer(const Integer &other) { mpz_init(_value); assign(other); }

public:
    Integer &operator=(Integer &&other)      { swap(other);   return *this; }
    Integer &operator=(const Integer &other) { assign(other); return *this; }

public:
    void swap(Integer &other);
    void assign(const Integer &other);

public:
    bool isZero(void) const { return mpz_cmp_ui(_value, 0u) != 0; }
    bool isSafeInt(void) const;
    bool isSafeUInt(void) const;

public:
    int64_t toInt(void) const { return mpz_get_sx(_value); }
    uint64_t toUInt(void) const { return mpz_get_ux(_value); }

public:
    uint64_t toHash(void) const;
    std::string toString(void) const;

};
}

#endif /* REDSCRIPT_UTILS_INTEGER_H */
