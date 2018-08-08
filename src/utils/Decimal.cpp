#include "utils/Decimal.h"
#include "utils/Strings.h"

static inline std::string repeat(char ch, ssize_t times)
{
    /* construct a string which contains `times` times repeats of `ch` */
    return std::string(static_cast<size_t>(times), ch);
}

namespace RedScript::Utils
{
BID_UINT128 Decimal::_zero;
BID_UINT128 Decimal::_maxInt64;
BID_UINT128 Decimal::_minInt64;
BID_UINT128 Decimal::_maxUInt64;

BID_UINT128 Decimal::_maxFloat;
BID_UINT128 Decimal::_minFloat;
BID_UINT128 Decimal::_maxDouble;
BID_UINT128 Decimal::_minDouble;
BID_UINT128 Decimal::_maxLongDouble;
BID_UINT128 Decimal::_minLongDouble;

Decimal::Decimal(Integer other)
{
    if (other.isSafeInt())
        _value = bid128_from_int64(other.toInt());
    else if (other.isSafeUInt())
        _value = bid128_from_uint64(other.toUInt());
    else
        _value = withFlagsChecked(bid128_from_string, other.toString().c_str());
}

bool Decimal::isSafeFloat(void) const
{
    /* -FLT_MAX <= value <= FLT_MAX */
    return withFlagsChecked(bid128_quiet_less_equal, _value, _maxFloat) &&
           withFlagsChecked(bid128_quiet_greater_equal, _value, _minFloat);
}

bool Decimal::isSafeDouble(void) const
{
    /* -DBL_MAX <= value <= DBL_MAX */
    return withFlagsChecked(bid128_quiet_less_equal, _value, _maxDouble) &&
           withFlagsChecked(bid128_quiet_greater_equal, _value, _minDouble);
}

bool Decimal::isSafeLongDouble(void) const
{
    /* -LDBL_MAX <= value <= LDBL_MAX */
    return withFlagsChecked(bid128_quiet_less_equal, _value, _maxLongDouble) &&
           withFlagsChecked(bid128_quiet_greater_equal, _value, _minLongDouble);
}

Integer Decimal::toInt(void) const
{
    /* INT64_MIN <= value <= INT64_MAX */
    if (withFlagsChecked(bid128_quiet_less_equal, _value, _maxInt64) &&
        withFlagsChecked(bid128_quiet_greater_equal, _value, _minInt64))
        return withFlagsChecked(bid128_to_int64_int, _value);

    /* 0 <= value <= UINT64_MAX */
    if (withFlagsChecked(bid128_quiet_greater, _value, _zero) &&
        withFlagsChecked(bid128_quiet_less_equal, _value, _maxUInt64))
        return withFlagsChecked(bid128_to_uint64_int, _value);

    /* try converting through string */
    char s[256] = {};
    char *delim = nullptr;
    char *start = &(s[0]);
    _IDEC_flags flags = 0;

    /* convert to internal string representation */
    bid128_to_string(s, _value, &flags);
    throwByFlags(flags);

    /* find the delimiter, maybe special values */
    if (!(delim = strchr(s, 'E')))
        throw Runtime::Exceptions::ValueError(Utils::Strings::format("Cannot convert %s to int", s));

    /* extract three parts */
    char *pos = delim;
    ssize_t exp = std::atoll(delim + 1);

    /* remove trailing zeros for small numbers */
    while ((exp < 0) && (pos > start) && (pos[-1] == '0'))
    {
        exp++;
        pos--;
    }

    /* base part and sign string */
    std::string base(start + 1, pos - start - 1);
    std::string sign(start[0] == '+' ? "" : "-");

    /* floating point number with nopn-negative exponent */
    if (exp >= 0)
        return Integer(sign + base + repeat('0', exp));

    /* floating point number which absolute value smaller than one */
    if (base.size() <= -exp)
        return 0;

    /* other "normal" sized floating point numbers */
    base.erase(static_cast<size_t>(base.size() + exp));
    return Integer(sign + base);
}

uint64_t Decimal::toHash(void) const
{
    /* +0.0 and -0.0 should gives the same hash */
    if (isZero())
        return 0;
    else
        return _value.w[0] ^ _value.w[1];
}

std::string Decimal::toString(void) const
{
    char s[256] = {};
    char *delim = nullptr;
    char *start = &(s[0]);
    _IDEC_flags flags = 0;

    /* convert to internal string representation */
    bid128_to_string(s, _value, &flags);
    throwByFlags(flags);

    /* find the delimiter, maybe special values */
    if (!(delim = strchr(s, 'E')))
        return std::string(s);

    /* extract three parts */
    char *pos = delim;
    ssize_t exp = std::atoll(delim + 1);

    /* remove trailing zeros for small numbers */
    while ((exp < 0) && (pos > start + 1) && (pos[-1] == '0'))
    {
        exp++;
        pos--;
    }

    /* base part and sign string */
    std::string base;
    std::string sign(start[0] == '+' ? "" : "-");

    /* empty base */
    if (pos == start + 1)
        base = "0";
    else
        base.assign(start + 1, pos - start - 1);

    /* floating point number with nopn-negative exponent */
    if (exp >= 0)
        return sign + base + repeat('0', exp) + ".0";

    /* floating point number which absolute value smaller than one */
    if (base.size() <= -exp)
        return sign + "0." + repeat('0', -exp - base.size()) + base;

    /* other "normal" sized floating point numbers */
    base.insert(static_cast<size_t>(base.size() + exp), ".");
    return sign + base;
}

void Decimal::initialize(void)
{
    /* integer constants */
    _zero       = bid128_from_int32(0);
    _maxInt64   = bid128_from_int64(INT64_MAX);
    _minInt64   = bid128_from_int64(INT64_MIN);
    _maxUInt64  = bid128_from_uint64(UINT64_MAX);

    /* decimal constants */
    _maxFloat       = withFlagsChecked(binary32_to_bid128,  FLT_MAX);
    _minFloat       = withFlagsChecked(binary32_to_bid128, -FLT_MAX);
    _maxDouble      = withFlagsChecked(binary64_to_bid128,  DBL_MAX);
    _minDouble      = withFlagsChecked(binary64_to_bid128, -DBL_MAX);
    _maxLongDouble  = withFlagsChecked(binary80_to_bid128,  LDBL_MAX);
    _minLongDouble  = withFlagsChecked(binary80_to_bid128, -LDBL_MAX);
}
}
