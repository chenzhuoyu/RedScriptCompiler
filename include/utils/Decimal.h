#ifndef REDSCRIPT_UTILS_DECIMAL_H
#define REDSCRIPT_UTILS_DECIMAL_H

#include <string>
#include <bid_dfp.h>

#include "exceptions/ValueError.h"

namespace RedScript::Utils
{
class Decimal
{
    BID_UINT128 _value;

private:
    static thread_local _IDEC_flags _flags;
    static thread_local _IDEC_round _round;

public:
    static const BID_UINT128 &maxFloat(void);
    static const BID_UINT128 &minFloat(void);
    static const BID_UINT128 &maxDouble(void);
    static const BID_UINT128 &minDouble(void);
    static const BID_UINT128 &maxLongDouble(void);
    static const BID_UINT128 &minLongDouble(void);

public:
    Decimal() : _value(bid128_from_int32(0)) {}
    Decimal(BID_UINT128 value) : _value(value) {}

public:
    Decimal(double value)             : Decimal(binary64_to_bid128(value        , _round, &_flags)) {}
    Decimal(const std::string &value) : Decimal(bid128_from_string(value.c_str(), _round, &_flags)) {}

public:
    Decimal(Decimal &&other)      : _value(bid128_from_int32(0)) { swap(other);   }
    Decimal(const Decimal &other) : _value(bid128_from_int32(0)) { assign(other); }

public:
    bool isInf(void) const { return bid128_isInf(_value) != 0; }
    bool isNaN(void) const { return bid128_isNaN(_value) != 0; }
    bool isZero(void) const { return bid128_isZero(_value) != 0; }
    bool isFinite(void) const { return bid128_isFinite(_value) != 0; }

public:
    bool isSafeFloat(void) const;
    bool isSafeDouble(void) const;
    bool isSafeLongDouble(void) const;

public:
    float toFloat(void) const { return bid128_to_binary32(_value, _round, &_flags); }
    double toDouble(void) const { return bid128_to_binary64(_value, _round, &_flags); }
    long double toLongDouble(void) const { return bid128_to_binary80(_value, _round, &_flags); }

public:
    uint64_t toHash(void) const;
    std::string toString(void) const;

private:
    static inline const BID_UINT128 &zeroChecked(const Decimal &val)
    {
        if (val.isZero())
            throw Exceptions::ValueError("Divide by zero");
        else
            return val._value;
    }

public:
    void swap(Decimal &other)         { std::swap(_value, other._value);    }
    void assign(const Decimal &other) { _value = bid128_copy(other._value); }

public:
    int     cmp(const Decimal &other) const { return (*this) == other ? 0 : (*this) > other ? 1 : -1;   }
    Decimal pow(const Decimal &other) const { return bid128_pow(_value, other._value, _round, &_flags); }

/** Assignment Operators **/

public:
    Decimal &operator=(Decimal &&other)      { swap(other);   return *this; }
    Decimal &operator=(const Decimal &other) { assign(other); return *this; }

/** Increment and Decrement Operators **/

public:
    Decimal operator++(int) { auto val = *this; ++(*this); return std::move(val); }
    Decimal operator--(int) { auto val = *this; --(*this); return std::move(val); }

public:
    Decimal &operator++(void) { *this = (*this) + 1.0; return *this; }
    Decimal &operator--(void) { *this = (*this) - 1.0; return *this; }

/** Arithmetic Operators **/

public:
    Decimal operator+(void) const { return bid128_copy(_value); }
    Decimal operator-(void) const { return bid128_negate(_value); }

public:
    Decimal operator+(const Decimal &other) const { return bid128_add (_value, other._value      , _round, &_flags); }
    Decimal operator-(const Decimal &other) const { return bid128_sub (_value, other._value      , _round, &_flags); }
    Decimal operator*(const Decimal &other) const { return bid128_mul (_value, other._value      , _round, &_flags); }
    Decimal operator/(const Decimal &other) const { return bid128_div (_value, zeroChecked(other), _round, &_flags); }
    Decimal operator%(const Decimal &other) const { return bid128_fmod(_value, zeroChecked(other),         &_flags); }

/** Inplace Arithmetic Operators **/

public:
    Decimal &operator+=(const Decimal &other) { *this = (*this) + other; return *this; }
    Decimal &operator-=(const Decimal &other) { *this = (*this) - other; return *this; }
    Decimal &operator*=(const Decimal &other) { *this = (*this) * other; return *this; }
    Decimal &operator/=(const Decimal &other) { *this = (*this) / other; return *this; }
    Decimal &operator%=(const Decimal &other) { *this = (*this) % other; return *this; }

/** Comparison Operators **/

public:
    bool operator< (const Decimal &other) const { return bid128_quiet_less          (_value, other._value, &_flags) != 0; }
    bool operator> (const Decimal &other) const { return bid128_quiet_greater       (_value, other._value, &_flags) != 0; }
    bool operator==(const Decimal &other) const { return bid128_quiet_equal         (_value, other._value, &_flags) != 0; }
    bool operator<=(const Decimal &other) const { return bid128_quiet_less_equal    (_value, other._value, &_flags) != 0; }
    bool operator>=(const Decimal &other) const { return bid128_quiet_greater_equal (_value, other._value, &_flags) != 0; }
    bool operator!=(const Decimal &other) const { return bid128_quiet_not_equal     (_value, other._value, &_flags) != 0; }

};
}

#endif /* REDSCRIPT_UTILS_DECIMAL_H */
