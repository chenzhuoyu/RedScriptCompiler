#include <cfloat>
#include <stdexcept>
#include <bid_dfp.h>

#include "utils/Decimal.h"
#include "utils/Strings.h"

static inline std::string repeat(char ch, ssize_t times)
{
    /* construct a string which contains `times` times repeats of `ch` */
    return std::string(static_cast<size_t>(times), ch);
}

namespace RedScript::Utils
{
const BID_UINT128 &Decimal::maxFloat(void)
{
    static thread_local BID_UINT128 value = withFlagsChecked(binary32_to_bid128, FLT_MAX);
    return value;
}

const BID_UINT128 &Decimal::minFloat(void)
{
    static thread_local BID_UINT128 value = withFlagsChecked(binary32_to_bid128, -FLT_MAX);
    return value;
}

const BID_UINT128 &Decimal::maxDouble(void)
{
    static thread_local BID_UINT128 value = withFlagsChecked(binary64_to_bid128, DBL_MAX);
    return value;
}

const BID_UINT128 &Decimal::minDouble(void)
{
    static thread_local BID_UINT128 value = withFlagsChecked(binary64_to_bid128, -DBL_MAX);
    return value;
}

const BID_UINT128 &Decimal::maxLongDouble(void)
{
    static thread_local BID_UINT128 value = withFlagsChecked(binary80_to_bid128, LDBL_MAX);
    return value;
}

const BID_UINT128 &Decimal::minLongDouble(void)
{
    static thread_local BID_UINT128 value = withFlagsChecked(binary80_to_bid128, -LDBL_MAX);
    return value;
}

bool Decimal::isSafeFloat(void) const
{
    /* -FLT_MAX <= value <= FLT_MAX */
    return withFlagsChecked(bid128_quiet_less_equal, _value, maxFloat()) &&
           withFlagsChecked(bid128_quiet_greater_equal, _value, minFloat());
}

bool Decimal::isSafeDouble(void) const
{
    /* -DBL_MAX <= value <= DBL_MAX */
    return withFlagsChecked(bid128_quiet_less_equal, _value, maxDouble()) &&
           withFlagsChecked(bid128_quiet_greater_equal, _value, minDouble());
}

bool Decimal::isSafeLongDouble(void) const
{
    /* -LDBL_MAX <= value <= LDBL_MAX */
    return withFlagsChecked(bid128_quiet_less_equal, _value, maxLongDouble()) &&
           withFlagsChecked(bid128_quiet_greater_equal, _value, minLongDouble());
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

    /* remove trailing zeros */
    while ((pos > start) && (pos[-1] == '0'))
    {
        exp++;
        pos--;
    }

    /* base part and sign string */
    std::string base(start + 1, pos - start - 1);
    std::string sign(start[0] == '+' ? "" : "-");

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
}
