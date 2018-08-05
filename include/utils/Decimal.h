#ifndef REDSCRIPT_UTILS_DECIMAL_H
#define REDSCRIPT_UTILS_DECIMAL_H

#include <cfloat>
#include <string>
#include <bid_dfp.h>

#include "utils/Integer.h"
#include "runtime/ExceptionObject.h"

namespace RedScript::Utils
{
class Decimal
{
    BID_UINT128 _value;

private:
    static inline void throwByFlags(_IDEC_flags flags)
    {
        if (flags & DEC_FE_INVALID  ) throw Runtime::Exceptions::ValueError("Undefined decimal operation");
        if (flags & DEC_FE_DIVBYZERO) throw Runtime::Exceptions::ZeroDivisionError("Decimal division by zero");
    }

private:
    template <typename Func, typename ... Args>
    static inline auto withFlagsChecked(Func func, Args && ... args)
    {
        /* execute the function with flags */
        auto flags = 0u;
        auto result = func(std::forward<Args>(args) ..., &flags);

        /* check the flags */
        throwByFlags(flags);
        return std::move(result);
    }

private:
    static BID_UINT128 _zero;
    static BID_UINT128 _maxInt64;
    static BID_UINT128 _minInt64;
    static BID_UINT128 _maxUInt64;

private:
    static BID_UINT128 _maxFloat;
    static BID_UINT128 _minFloat;
    static BID_UINT128 _maxDouble;
    static BID_UINT128 _minDouble;
    static BID_UINT128 _maxLongDouble;
    static BID_UINT128 _minLongDouble;

public:
    Decimal() : _value(_zero) {}
    Decimal(BID_UINT128 value) : _value(value) {}

public:
    Decimal(double value)             : Decimal(withFlagsChecked(binary64_to_bid128, value)) {}
    Decimal(long double value)        : Decimal(withFlagsChecked(binary80_to_bid128, value)) {}
    Decimal(const std::string &value) : Decimal(withFlagsChecked(bid128_from_string, value.c_str())) {}

public:
    Decimal(Integer other);
    Decimal(Decimal &&other)      : _value(_zero) { swap(other);   }
    Decimal(const Decimal &other) : _value(_zero) { assign(other); }

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
    float toFloat(void) const { return withFlagsChecked(bid128_to_binary32, _value); }
    double toDouble(void) const { return withFlagsChecked(bid128_to_binary64, _value); }
    long double toLongDouble(void) const { return withFlagsChecked(bid128_to_binary80, _value); }

public:
    Integer toInt(void) const;
    uint64_t toHash(void) const;
    std::string toString(void) const;

public:
    void swap(Decimal &other)         { std::swap(_value, other._value); }
    void assign(const Decimal &other) { _value = other._value; }

public:
    int32_t cmp(const Decimal &other) const { return (*this) == other ? 0 : (*this) > other ? 1 : -1;    }
    Decimal pow(const Decimal &other) const { return withFlagsChecked(bid128_pow, _value, other._value); }

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
    Decimal operator+(void) const { return _value; }
    Decimal operator-(void) const { return bid128_negate(_value); }

public:
    Decimal operator+(const Decimal &other) const { return withFlagsChecked(bid128_add , _value, other._value); }
    Decimal operator-(const Decimal &other) const { return withFlagsChecked(bid128_sub , _value, other._value); }
    Decimal operator*(const Decimal &other) const { return withFlagsChecked(bid128_mul , _value, other._value); }
    Decimal operator/(const Decimal &other) const { return withFlagsChecked(bid128_div , _value, other._value); }
    Decimal operator%(const Decimal &other) const { return withFlagsChecked(bid128_fmod, _value, other._value); }

/** Inplace Arithmetic Operators **/

public:
    Decimal &operator+=(const Decimal &other) { *this = (*this) + other; return *this; }
    Decimal &operator-=(const Decimal &other) { *this = (*this) - other; return *this; }
    Decimal &operator*=(const Decimal &other) { *this = (*this) * other; return *this; }
    Decimal &operator/=(const Decimal &other) { *this = (*this) / other; return *this; }
    Decimal &operator%=(const Decimal &other) { *this = (*this) % other; return *this; }

/** Comparison Operators **/

public:
    bool operator< (const Decimal &other) const { return withFlagsChecked(bid128_quiet_less         , _value, other._value) != 0; }
    bool operator> (const Decimal &other) const { return withFlagsChecked(bid128_quiet_greater      , _value, other._value) != 0; }
    bool operator==(const Decimal &other) const { return withFlagsChecked(bid128_quiet_equal        , _value, other._value) != 0; }
    bool operator<=(const Decimal &other) const { return withFlagsChecked(bid128_quiet_less_equal   , _value, other._value) != 0; }
    bool operator>=(const Decimal &other) const { return withFlagsChecked(bid128_quiet_greater_equal, _value, other._value) != 0; }
    bool operator!=(const Decimal &other) const { return withFlagsChecked(bid128_quiet_not_equal    , _value, other._value) != 0; }

/** Decimal Initialization **/

public:
    static void shutdown(void) {}
    static void initialize(void);

};
}

#endif /* REDSCRIPT_UTILS_DECIMAL_H */
