#ifndef REDSCRIPT_UTILS_INTEGER_H
#define REDSCRIPT_UTILS_INTEGER_H

#include <string>
#include <cstdint>
#include <mpir.h>

#include "exceptions/ValueError.h"
#include "exceptions/ZeroDivisionError.h"

namespace RedScript::Utils
{
class Integer
{
    mpz_t _value;
    static_assert(GMP_LIMB_BITS == sizeof(uint64_t) * 8, "Unsupported limb size");

public:
   ~Integer() { mpz_clear(_value); }
    Integer() { mpz_init2(_value, 1024); }

public:
    Integer(size_t value)  { mpz_init_set_ux(_value, value); }
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
    Integer(Integer &&other)      { memset(_value, 0, sizeof(mpz_t)); swap(other); }
    Integer(const Integer &other) { memset(_value, 0, sizeof(mpz_t)); assign(other); }

private:
    template <typename F, typename ... Args>
    explicit Integer(F operation, Args && ... args)
    {
        mpz_init2(_value, 1024);
        operation(_value, std::forward<Args>(args) ...);
    }

private:
    static inline uint32_t bitChecked(const Integer &val)
    {
        /* must be unsigned integer */
        if (!(val.isSafeUInt()))
            throw Exceptions::ValueError("Not a valid bit count");

        /* and must be a valid 32-bit integer */
        auto bits = val.toUInt();
        return bits < UINT32_MAX ? static_cast<uint32_t>(bits) : throw Exceptions::ValueError("Bit shifts too far");
    }

private:
    static inline const mpz_t &zeroChecked(const Integer &val)
    {
        if (val.isZero())
            throw Exceptions::ZeroDivisionError("Integer division or modulo by zero");
        else
            return val._value;
    }

public:
    void swap(Integer &other) { std::swap(_value, other._value); }
    void assign(const Integer &other) { mpz_set(_value, other._value); }

#define D (_value->_mp_d[0])
#define N (_value->_mp_size)

public:
    bool isZero(void)             const { return (N == 0); }
    bool isSafeInt(void)          const { return (N == 0) || ((N == 1) && (D <= INT64_MAX)) || ((N == -1) && (D <= -INT64_MIN)); }
    bool isSafeUInt(void)         const { return (N == 0) || (N == 1); }
    bool isSafeNegativeUInt(void) const { return (N == 0) || (N == -1); }

public:
    int64_t  toInt(void)          const { return static_cast< int64_t>((N == 0) ? 0 : (N > 0) ? D : -D); }
    uint64_t toUInt(void)         const { return static_cast<uint64_t>((N == 0) ? 0 : D); }
    uint64_t toNegativeUInt(void) const { return static_cast<uint64_t>((N == 0) ? 0 : D); }

#undef D
#undef N

public:
    uint64_t toHash(void) const;
    std::string toString(void) const;

public:
    int32_t cmp(const Integer &other) const { return mpz_cmp(_value, other._value); }
    Integer pow(uint64_t exp)         const { return Integer(mpz_pow_ui, _value, exp); }

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
    Integer operator-(void) const { return Integer(mpz_neg, _value); }
    Integer operator~(void) const { return Integer(mpz_com, _value); }

public:
    Integer operator+(const Integer &other) const { return Integer(mpz_add   , _value, other._value); }
    Integer operator-(const Integer &other) const { return Integer(mpz_sub   , _value, other._value); }
    Integer operator*(const Integer &other) const { return Integer(mpz_mul   , _value, other._value); }
    Integer operator/(const Integer &other) const { return Integer(mpz_tdiv_q, _value, zeroChecked(other)); }
    Integer operator%(const Integer &other) const { return Integer(mpz_tdiv_r, _value, zeroChecked(other)); }

public:
    Integer operator^(const Integer &other) const { return Integer(mpz_xor, _value, other._value); }
    Integer operator&(const Integer &other) const { return Integer(mpz_and, _value, other._value); }
    Integer operator|(const Integer &other) const { return Integer(mpz_ior, _value, other._value); }

public:
    Integer operator<<(const Integer &other) const { return Integer(mpz_mul_2exp   , _value, bitChecked(other)); }
    Integer operator>>(const Integer &other) const { return Integer(mpz_tdiv_q_2exp, _value, bitChecked(other)); }

/** Inplace Arithmetic Operators **/

public:
    Integer &operator+=(const Integer &other) { mpz_add   (_value, _value, other._value);       return *this; }
    Integer &operator-=(const Integer &other) { mpz_sub   (_value, _value, other._value);       return *this; }
    Integer &operator*=(const Integer &other) { mpz_mul   (_value, _value, other._value);       return *this; }
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

/** Integer Initialization **/

public:
    static void shutdown(void) {}
    static void initialize(void);

};
}

#endif /* REDSCRIPT_UTILS_INTEGER_H */
