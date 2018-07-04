#ifndef REDSCRIPT_UTILS_DECIMAL_H
#define REDSCRIPT_UTILS_DECIMAL_H

#include <string>
#include <bid_dfp.h>

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
    Decimal(double value) : Decimal(binary64_to_bid128(value, BID_ROUNDING_TO_NEAREST, &_flags)) {}
    Decimal(const std::string &value) : Decimal(bid128_from_string(value.c_str(), BID_ROUNDING_TO_NEAREST, &_flags)) {}

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
    float toFloat(void) const { return bid128_to_binary32(_value, BID_ROUNDING_TO_NEAREST, &_flags); }
    double toDouble(void) const { return bid128_to_binary64(_value, BID_ROUNDING_TO_NEAREST, &_flags); }
    long double toLongDouble(void) const { return bid128_to_binary80(_value, BID_ROUNDING_TO_NEAREST, &_flags); }

public:
    uint64_t toHash(void) const;
    std::string toString(void) const;

};
}

#endif /* REDSCRIPT_UTILS_DECIMAL_H */
