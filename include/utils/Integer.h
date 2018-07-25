#ifndef REDSCRIPT_UTILS_INTEGER_H
#define REDSCRIPT_UTILS_INTEGER_H

#include <string>
#include <cstdint>
#include <mpir.h>

#include "exceptions/ValueError.h"

namespace RedScript::Utils
{
class Integer
{
    mpz_t _value;

private:
    static const mpz_t &maxNegativeUInt(void);
    static const mpz_t &minNegativeUInt(void);

public:
    Integer() { mpz_init(_value);  }
   ~Integer() { mpz_clear(_value); }

public:
    Integer(size_t value) { mpz_init_set_ux(_value, value); }
    Integer(ssize_t value) { mpz_init_set_sx(_value, value); }

public:
    Integer(int32_t value)  { mpz_init_set_si(_value, value); }
    Integer(int64_t value)  { mpz_init_set_sx(_value, value); }
    Integer(uint32_t value) { mpz_init_set_ui(_value, value); }
    Integer(uint64_t value) { mpz_init_set_ux(_value, value); }

public:
    explicit Integer(const std::string &value) : Integer(value, 10) {}
    explicit Integer(const std::string &value, int radix);

public:
    Integer(Integer &&other)      { mpz_init(_value); swap(other); }
    Integer(const Integer &other) { mpz_init(_value); assign(other); }

private:
    template <typename Function>
    explicit Integer(Function function)
    {
        mpz_init(_value);
        function(_value);
    }

private:
    static inline uint32_t bitChecked(const Integer &val)
    {
        /* must be unsigned integer */
        if (!(val.isSafeUInt()))
            throw Exceptions::ValueError("Not a valid bit count");

        /* and must be a valid 32-bit integer */
        auto bits = val.toUInt();
        return bits < UINT32_MAX ? static_cast<uint32_t>(bits) : throw Exceptions::ValueError("Bit shifts too far");;
    }

private:
    static inline const mpz_t &zeroChecked(const Integer &val)
    {
        if (val.isZero())
            throw Exceptions::ValueError("Divide by zero");
        else
            return val._value;
    }

public:
    void swap(Integer &other);
    void assign(const Integer &other);

public:
    bool isZero(void) const { return mpz_cmp_ui(_value, 0u) == 0; }
    bool isSafeInt(void) const;
    bool isSafeUInt(void) const;
    bool isSafeNegativeUInt(void) const;

public:
    int64_t toInt(void) const { return mpz_get_sx(_value); }
    uint64_t toUInt(void) const { return mpz_get_ux(_value); }
    uint64_t toNegativeUInt(void) const { return (-(*this)).toUInt(); }

public:
    uint64_t toHash(void) const;
    std::string toString(void) const;

public:
    int32_t cmp(const Integer &other) const { return mpz_cmp(_value, other._value); }
    Integer pow(uint64_t exp)         const { return Integer([&](mpz_t &result){ mpz_pow_ui(result, _value, exp); }); }

/** Assignment Operators **/

public:
    Integer &operator=(Integer &&other)      { swap(other);   return *this; }
    Integer &operator=(const Integer &other) { assign(other); return *this; }

/** Increment and Decrement Operators **/

public:
    Integer operator++(int) { auto val = *this; ++(*this); return std::move(val); }
    Integer operator--(int) { auto val = *this; --(*this); return std::move(val); }

public:
    Integer &operator++(void) { mpz_add_ui(_value, _value, 1); return *this; }
    Integer &operator--(void) { mpz_sub_ui(_value, _value, 1); return *this; }

/** Arithmetic Operators **/

public:
    Integer operator+(void) const { return *this; }
    Integer operator-(void) const { return Integer([&](mpz_t &result){ mpz_neg(result, _value); }); }
    Integer operator~(void) const { return Integer([&](mpz_t &result){ mpz_com(result, _value); }); }

public:
    Integer operator+(const Integer &other) const { return Integer([&](mpz_t &result){ mpz_add   (result, _value, other._value      ); }); }
    Integer operator-(const Integer &other) const { return Integer([&](mpz_t &result){ mpz_sub   (result, _value, other._value      ); }); }
    Integer operator*(const Integer &other) const { return Integer([&](mpz_t &result){ mpz_mul   (result, _value, other._value      ); }); }
    Integer operator/(const Integer &other) const { return Integer([&](mpz_t &result){ mpz_tdiv_q(result, _value, zeroChecked(other)); }); }
    Integer operator%(const Integer &other) const { return Integer([&](mpz_t &result){ mpz_tdiv_r(result, _value, zeroChecked(other)); }); }

public:
    Integer operator^(const Integer &other) const { return Integer([&](mpz_t &result){ mpz_xor(result, _value, other._value); }); }
    Integer operator&(const Integer &other) const { return Integer([&](mpz_t &result){ mpz_and(result, _value, other._value); }); }
    Integer operator|(const Integer &other) const { return Integer([&](mpz_t &result){ mpz_ior(result, _value, other._value); }); }

public:
    Integer operator<<(const Integer &other) const { return Integer([&](mpz_t &result){ mpz_mul_2exp   (result, _value, bitChecked(other)); }); }
    Integer operator>>(const Integer &other) const { return Integer([&](mpz_t &result){ mpz_tdiv_q_2exp(result, _value, bitChecked(other)); }); }

/** Inplace Arithmetic Operators **/

public:
    Integer &operator+=(const Integer &other) { mpz_add   (_value, _value, other._value      ); return *this; }
    Integer &operator-=(const Integer &other) { mpz_sub   (_value, _value, other._value      ); return *this; }
    Integer &operator*=(const Integer &other) { mpz_mul   (_value, _value, other._value      ); return *this; }
    Integer &operator/=(const Integer &other) { mpz_tdiv_q(_value, _value, zeroChecked(other)); return *this; }
    Integer &operator%=(const Integer &other) { mpz_tdiv_r(_value, _value, zeroChecked(other)); return *this; }

public:
    Integer &operator^=(const Integer &other) { mpz_xor(_value, _value, other._value); return *this; }
    Integer &operator&=(const Integer &other) { mpz_and(_value, _value, other._value); return *this; }
    Integer &operator|=(const Integer &other) { mpz_ior(_value, _value, other._value); return *this; }

public:
    Integer &operator<<=(const Integer &other) { mpz_mul_2exp   (_value, _value, bitChecked(other)); return *this; }
    Integer &operator>>=(const Integer &other) { mpz_tdiv_q_2exp(_value, _value, bitChecked(other)); return *this; }

/** Comparison Operators **/

public:
    bool operator< (const Integer &other) const { return mpz_cmp(_value, other._value) <  0; }
    bool operator> (const Integer &other) const { return mpz_cmp(_value, other._value) >  0; }
    bool operator==(const Integer &other) const { return mpz_cmp(_value, other._value) == 0; }
    bool operator<=(const Integer &other) const { return mpz_cmp(_value, other._value) <= 0; }
    bool operator>=(const Integer &other) const { return mpz_cmp(_value, other._value) >= 0; }
    bool operator!=(const Integer &other) const { return mpz_cmp(_value, other._value) != 0; }

};
}

#endif /* REDSCRIPT_UTILS_INTEGER_H */
