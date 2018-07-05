#include <cfloat>
#include <stdexcept>
#include <bid_dfp.h>

#include "utils/Decimal.h"
#include "utils/Strings.h"

namespace RedScript::Utils
{
thread_local _IDEC_flags Decimal::_flags = 0;
thread_local _IDEC_round Decimal::_round = BID_ROUNDING_TO_NEAREST;

const BID_UINT128 &Decimal::maxFloat(void)
{
    static thread_local BID_UINT128 value = binary32_to_bid128(FLT_MAX, _round, &_flags);
    return value;
}

const BID_UINT128 &Decimal::minFloat(void)
{
    static thread_local BID_UINT128 value = binary32_to_bid128(-FLT_MAX, _round, &_flags);
    return value;
}

const BID_UINT128 &Decimal::maxDouble(void)
{
    static thread_local BID_UINT128 value = binary64_to_bid128(DBL_MAX, _round, &_flags);
    return value;
}

const BID_UINT128 &Decimal::minDouble(void)
{
    static thread_local BID_UINT128 value = binary64_to_bid128(-DBL_MAX, _round, &_flags);
    return value;
}

const BID_UINT128 &Decimal::maxLongDouble(void)
{
    static thread_local BID_UINT128 value = binary80_to_bid128(LDBL_MAX, _round, &_flags);
    return value;
}

const BID_UINT128 &Decimal::minLongDouble(void)
{
    static thread_local BID_UINT128 value = binary80_to_bid128(-LDBL_MAX, _round, &_flags);
    return value;
}

bool Decimal::isSafeFloat(void) const
{
    /* -FLT_MAX <= value <= FLT_MAX */
    return bid128_quiet_less_equal(_value, maxFloat(), &_flags) &&
           bid128_quiet_greater_equal(_value, minFloat(), &_flags);
}

bool Decimal::isSafeDouble(void) const
{
    /* -DBL_MAX <= value <= DBL_MAX */
    return bid128_quiet_less_equal(_value, maxDouble(), &_flags) &&
           bid128_quiet_greater_equal(_value, minDouble(), &_flags);
}

bool Decimal::isSafeLongDouble(void) const
{
    /* -LDBL_MAX <= value <= LDBL_MAX */
    return bid128_quiet_less_equal(_value, maxLongDouble(), &_flags) &&
           bid128_quiet_greater_equal(_value, minLongDouble(), &_flags);
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

    /* convert to internal string representation */
    bid128_to_string(s, _value, &_flags);

    /* find the delimiter */
    if (!(delim = strchr(s, 'E')))
        throw std::logic_error("invalid float representation");

    /* extract three parts */
    char sign = *start++;
    ssize_t exp = std::atoll(delim + 1);
    std::string base(start, delim - start);

    /* check for exponent signess */
    if (exp >= 0)
        base += std::string(static_cast<size_t>(exp), '0');
    else
        base.insert(static_cast<size_t>(base.length() + exp), ".");

    /* add sign as needed */
    return (sign == '+' ? "" : "-") + base;
}
}
