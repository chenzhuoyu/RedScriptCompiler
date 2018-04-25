#ifndef REDSCRIPT_COMPILER_PREPROCESSOR_H
#define REDSCRIPT_COMPILER_PREPROCESSOR_H

#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedMacroInspection"

/*** Compiler Suggestions ***/

#define RSPP_LIKELY(cond)       __builtin_expect(static_cast<bool>(cond), 1)
#define RSPP_UNLIKELY(cond)     __builtin_expect(static_cast<bool>(cond), 0)

/*** Stringize and Concatenation ***/

#define __RSPP_CONCAT_P(a, b)           a ## b
#define __RSPP_CONCAT_I(a, b)           __RSPP_CONCAT_P(a, b)

#define __RSPP_STRINGIZE_P(token)       #token
#define __RSPP_STRINGIZE_I(token)       __RSPP_STRINGIZE_P(token)

#define RSPP_CONCAT(a, b)               __RSPP_CONCAT_I(a, b)
#define RSPP_STRINGIZE(token)           __RSPP_STRINGIZE_I(token)

/*** Varidic Macro Arguments ***/

#define __RSPP_VA_NARGS_I(                                                              \
     _1,  _2,  _3,  _4,  _5,  _6,  _7,  _8,  _9, _10, _11, _12, _13, _14, _15, _16,     \
    _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, _32,     \
    _33, _34, _35, _36, _37, _38, _39, _40, _41, _42, _43, _44, _45, _46, _47, _48,     \
    _49, _50, _51, _52, _53, _54, _55, _56, _57, _58, _59, _60, _61, _62, _63, _64,     \
    N, ...) N

#define RSPP_VA_NARGS(...)  __RSPP_VA_NARGS_I(__VA_ARGS__,          \
    64, 63, 62, 61, 60, 59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 49, \
    48, 47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, \
    32, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, \
    16, 15, 14, 13, 12, 11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1, \
    0                                                               \
)

/*** Macro For-each Loop ***/

#define __RSPP_FOR_EACH_L_0(macro, data, elem, ...)
#define __RSPP_FOR_EACH_L_1(macro, data, elem, ...)         macro(data, elem)
#define __RSPP_FOR_EACH_L_2(macro, data, elem, ...)         macro(data, elem) __RSPP_FOR_EACH_L_1(macro, data, __VA_ARGS__)
#define __RSPP_FOR_EACH_L_3(macro, data, elem, ...)         macro(data, elem) __RSPP_FOR_EACH_L_2(macro, data, __VA_ARGS__)
#define __RSPP_FOR_EACH_L_4(macro, data, elem, ...)         macro(data, elem) __RSPP_FOR_EACH_L_3(macro, data, __VA_ARGS__)
#define __RSPP_FOR_EACH_L_5(macro, data, elem, ...)         macro(data, elem) __RSPP_FOR_EACH_L_4(macro, data, __VA_ARGS__)
#define __RSPP_FOR_EACH_L_6(macro, data, elem, ...)         macro(data, elem) __RSPP_FOR_EACH_L_5(macro, data, __VA_ARGS__)
#define __RSPP_FOR_EACH_L_7(macro, data, elem, ...)         macro(data, elem) __RSPP_FOR_EACH_L_6(macro, data, __VA_ARGS__)
#define __RSPP_FOR_EACH_L_8(macro, data, elem, ...)         macro(data, elem) __RSPP_FOR_EACH_L_7(macro, data, __VA_ARGS__)
#define __RSPP_FOR_EACH_L_9(macro, data, elem, ...)         macro(data, elem) __RSPP_FOR_EACH_L_8(macro, data, __VA_ARGS__)
#define __RSPP_FOR_EACH_L_10(macro, data, elem, ...)        macro(data, elem) __RSPP_FOR_EACH_L_9(macro, data, __VA_ARGS__)
#define __RSPP_FOR_EACH_L_11(macro, data, elem, ...)        macro(data, elem) __RSPP_FOR_EACH_L_10(macro, data, __VA_ARGS__)
#define __RSPP_FOR_EACH_L_12(macro, data, elem, ...)        macro(data, elem) __RSPP_FOR_EACH_L_11(macro, data, __VA_ARGS__)
#define __RSPP_FOR_EACH_L_13(macro, data, elem, ...)        macro(data, elem) __RSPP_FOR_EACH_L_12(macro, data, __VA_ARGS__)
#define __RSPP_FOR_EACH_L_14(macro, data, elem, ...)        macro(data, elem) __RSPP_FOR_EACH_L_13(macro, data, __VA_ARGS__)
#define __RSPP_FOR_EACH_L_15(macro, data, elem, ...)        macro(data, elem) __RSPP_FOR_EACH_L_14(macro, data, __VA_ARGS__)
#define __RSPP_FOR_EACH_L_16(macro, data, elem, ...)        macro(data, elem) __RSPP_FOR_EACH_L_15(macro, data, __VA_ARGS__)
#define __RSPP_FOR_EACH_L_17(macro, data, elem, ...)        macro(data, elem) __RSPP_FOR_EACH_L_16(macro, data, __VA_ARGS__)
#define __RSPP_FOR_EACH_L_18(macro, data, elem, ...)        macro(data, elem) __RSPP_FOR_EACH_L_17(macro, data, __VA_ARGS__)
#define __RSPP_FOR_EACH_L_19(macro, data, elem, ...)        macro(data, elem) __RSPP_FOR_EACH_L_18(macro, data, __VA_ARGS__)
#define __RSPP_FOR_EACH_L_20(macro, data, elem, ...)        macro(data, elem) __RSPP_FOR_EACH_L_19(macro, data, __VA_ARGS__)
#define __RSPP_FOR_EACH_L_21(macro, data, elem, ...)        macro(data, elem) __RSPP_FOR_EACH_L_20(macro, data, __VA_ARGS__)
#define __RSPP_FOR_EACH_L_22(macro, data, elem, ...)        macro(data, elem) __RSPP_FOR_EACH_L_21(macro, data, __VA_ARGS__)
#define __RSPP_FOR_EACH_L_23(macro, data, elem, ...)        macro(data, elem) __RSPP_FOR_EACH_L_22(macro, data, __VA_ARGS__)
#define __RSPP_FOR_EACH_L_24(macro, data, elem, ...)        macro(data, elem) __RSPP_FOR_EACH_L_23(macro, data, __VA_ARGS__)
#define __RSPP_FOR_EACH_L_25(macro, data, elem, ...)        macro(data, elem) __RSPP_FOR_EACH_L_24(macro, data, __VA_ARGS__)
#define __RSPP_FOR_EACH_L_26(macro, data, elem, ...)        macro(data, elem) __RSPP_FOR_EACH_L_25(macro, data, __VA_ARGS__)
#define __RSPP_FOR_EACH_L_27(macro, data, elem, ...)        macro(data, elem) __RSPP_FOR_EACH_L_26(macro, data, __VA_ARGS__)
#define __RSPP_FOR_EACH_L_28(macro, data, elem, ...)        macro(data, elem) __RSPP_FOR_EACH_L_27(macro, data, __VA_ARGS__)
#define __RSPP_FOR_EACH_L_29(macro, data, elem, ...)        macro(data, elem) __RSPP_FOR_EACH_L_28(macro, data, __VA_ARGS__)
#define __RSPP_FOR_EACH_L_30(macro, data, elem, ...)        macro(data, elem) __RSPP_FOR_EACH_L_29(macro, data, __VA_ARGS__)
#define __RSPP_FOR_EACH_L_31(macro, data, elem, ...)        macro(data, elem) __RSPP_FOR_EACH_L_30(macro, data, __VA_ARGS__)
#define __RSPP_FOR_EACH_L_32(macro, data, elem, ...)        macro(data, elem) __RSPP_FOR_EACH_L_31(macro, data, __VA_ARGS__)
#define __RSPP_FOR_EACH_L_33(macro, data, elem, ...)        macro(data, elem) __RSPP_FOR_EACH_L_32(macro, data, __VA_ARGS__)
#define __RSPP_FOR_EACH_L_34(macro, data, elem, ...)        macro(data, elem) __RSPP_FOR_EACH_L_33(macro, data, __VA_ARGS__)
#define __RSPP_FOR_EACH_L_35(macro, data, elem, ...)        macro(data, elem) __RSPP_FOR_EACH_L_34(macro, data, __VA_ARGS__)
#define __RSPP_FOR_EACH_L_36(macro, data, elem, ...)        macro(data, elem) __RSPP_FOR_EACH_L_35(macro, data, __VA_ARGS__)
#define __RSPP_FOR_EACH_L_37(macro, data, elem, ...)        macro(data, elem) __RSPP_FOR_EACH_L_36(macro, data, __VA_ARGS__)
#define __RSPP_FOR_EACH_L_38(macro, data, elem, ...)        macro(data, elem) __RSPP_FOR_EACH_L_37(macro, data, __VA_ARGS__)
#define __RSPP_FOR_EACH_L_39(macro, data, elem, ...)        macro(data, elem) __RSPP_FOR_EACH_L_38(macro, data, __VA_ARGS__)
#define __RSPP_FOR_EACH_L_40(macro, data, elem, ...)        macro(data, elem) __RSPP_FOR_EACH_L_39(macro, data, __VA_ARGS__)
#define __RSPP_FOR_EACH_L_41(macro, data, elem, ...)        macro(data, elem) __RSPP_FOR_EACH_L_40(macro, data, __VA_ARGS__)
#define __RSPP_FOR_EACH_L_42(macro, data, elem, ...)        macro(data, elem) __RSPP_FOR_EACH_L_41(macro, data, __VA_ARGS__)
#define __RSPP_FOR_EACH_L_43(macro, data, elem, ...)        macro(data, elem) __RSPP_FOR_EACH_L_42(macro, data, __VA_ARGS__)
#define __RSPP_FOR_EACH_L_44(macro, data, elem, ...)        macro(data, elem) __RSPP_FOR_EACH_L_43(macro, data, __VA_ARGS__)
#define __RSPP_FOR_EACH_L_45(macro, data, elem, ...)        macro(data, elem) __RSPP_FOR_EACH_L_44(macro, data, __VA_ARGS__)
#define __RSPP_FOR_EACH_L_46(macro, data, elem, ...)        macro(data, elem) __RSPP_FOR_EACH_L_45(macro, data, __VA_ARGS__)
#define __RSPP_FOR_EACH_L_47(macro, data, elem, ...)        macro(data, elem) __RSPP_FOR_EACH_L_46(macro, data, __VA_ARGS__)
#define __RSPP_FOR_EACH_L_48(macro, data, elem, ...)        macro(data, elem) __RSPP_FOR_EACH_L_47(macro, data, __VA_ARGS__)
#define __RSPP_FOR_EACH_L_49(macro, data, elem, ...)        macro(data, elem) __RSPP_FOR_EACH_L_48(macro, data, __VA_ARGS__)
#define __RSPP_FOR_EACH_L_50(macro, data, elem, ...)        macro(data, elem) __RSPP_FOR_EACH_L_49(macro, data, __VA_ARGS__)
#define __RSPP_FOR_EACH_L_51(macro, data, elem, ...)        macro(data, elem) __RSPP_FOR_EACH_L_50(macro, data, __VA_ARGS__)
#define __RSPP_FOR_EACH_L_52(macro, data, elem, ...)        macro(data, elem) __RSPP_FOR_EACH_L_51(macro, data, __VA_ARGS__)
#define __RSPP_FOR_EACH_L_53(macro, data, elem, ...)        macro(data, elem) __RSPP_FOR_EACH_L_52(macro, data, __VA_ARGS__)
#define __RSPP_FOR_EACH_L_54(macro, data, elem, ...)        macro(data, elem) __RSPP_FOR_EACH_L_53(macro, data, __VA_ARGS__)
#define __RSPP_FOR_EACH_L_55(macro, data, elem, ...)        macro(data, elem) __RSPP_FOR_EACH_L_54(macro, data, __VA_ARGS__)
#define __RSPP_FOR_EACH_L_56(macro, data, elem, ...)        macro(data, elem) __RSPP_FOR_EACH_L_55(macro, data, __VA_ARGS__)
#define __RSPP_FOR_EACH_L_57(macro, data, elem, ...)        macro(data, elem) __RSPP_FOR_EACH_L_56(macro, data, __VA_ARGS__)
#define __RSPP_FOR_EACH_L_58(macro, data, elem, ...)        macro(data, elem) __RSPP_FOR_EACH_L_57(macro, data, __VA_ARGS__)
#define __RSPP_FOR_EACH_L_59(macro, data, elem, ...)        macro(data, elem) __RSPP_FOR_EACH_L_58(macro, data, __VA_ARGS__)
#define __RSPP_FOR_EACH_L_60(macro, data, elem, ...)        macro(data, elem) __RSPP_FOR_EACH_L_59(macro, data, __VA_ARGS__)
#define __RSPP_FOR_EACH_L_61(macro, data, elem, ...)        macro(data, elem) __RSPP_FOR_EACH_L_60(macro, data, __VA_ARGS__)
#define __RSPP_FOR_EACH_L_62(macro, data, elem, ...)        macro(data, elem) __RSPP_FOR_EACH_L_61(macro, data, __VA_ARGS__)
#define __RSPP_FOR_EACH_L_63(macro, data, elem, ...)        macro(data, elem) __RSPP_FOR_EACH_L_62(macro, data, __VA_ARGS__)
#define __RSPP_FOR_EACH_L_64(macro, data, elem, ...)        macro(data, elem) __RSPP_FOR_EACH_L_63(macro, data, __VA_ARGS__)
#define __RSPP_FOR_EACH_I(N, macro, data, elem, ...)        RSPP_CONCAT(__RSPP_FOR_EACH_L_, N)(macro, data, elem, __VA_ARGS__)

#define RSPP_FOR_EACH(macro, data, ...)                     __RSPP_FOR_EACH_I(RSPP_VA_NARGS(__VA_ARGS__), macro, data, __VA_ARGS__)

#pragma clang diagnostic pop

#endif /* REDSCRIPT_COMPILER_PREPROCESSOR_H */
