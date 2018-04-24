#ifndef REDSCRIPT_COMPILER_BYTECODE_H
#define REDSCRIPT_COMPILER_BYTECODE_H

#include <cstdint>

namespace RedScript::Engine
{
enum class OpCode : uint8_t
{
    LOAD_CONST      = 0x00,         // LOAD_CONST       <const>     push <const>
    LOAD_NAME       = 0x01,         // LOAD_NAME        <name>      Load variable by <name> into stack
    LOAD_LOCAL      = 0x02,         // LOAD_LOCAL       <index>     Load local variable <index> into stack
    STOR_LOCAL      = 0x03,         // STOR_LOCAL       <index>     Store local variable <index> from stack
    DEL_LOCAL       = 0x04,         // DEL_LOCAL        <index>     Delete local variable <index> (by setting it to NULL)

    DEF_ATTR        = 0x07,         // DEF_ATTR         <name>      define <stack_top>.<name> = None
    GET_ATTR        = 0x08,         // GET_ATTR         <name>      <stack_top> = <stack_top>.<name>
    SET_ATTR        = 0x09,         // SET_ATTR         <name>      <stack_top>.<name> = <stack_top + 1>
    DEL_ATTR        = 0x0a,         // DEL_ATTR         <name>      delete <stack_top>.name

    GET_ITEM        = 0x0b,         // GET_ITEM                     <stack_top> = <stack_top + 1>[<stack_top>]
    SET_ITEM        = 0x0c,         // SET_ITEM                     <stack_top + 1>[<stack_top>] = <stack_top + 2>
    DEL_ITEM        = 0x0d,         // DEL_ITEM                     delete <stack_top + 1>[<stack_top>]

    POP_RETURN      = 0x0e,         // POP_RETURN                   Pop and return <stack_top>
    CALL_FUNCTION   = 0x0f,         // CALL_FUNCTION    <flags>     Call function at stack top

    DUP             = 0x10,         // DUP                          Duplicate <stack_top>
    DUP2            = 0x11,         // DUP2                         Duplicate <stack_top> and <stack_top - 1>
    DROP            = 0x12,         // DROP                         Drop <stack_top>

    ADD             = 0x20,         // ADD                          <stack_top> = <stack_top + 1> + <stack_top>
    SUB             = 0x21,         // ...
    MUL             = 0x22,
    DIV             = 0x23,
    MOD             = 0x24,
    POWER           = 0x25,
    BIT_OR          = 0x26,
    BIT_AND         = 0x27,
    BIT_XOR         = 0x28,
    BIT_NOT         = 0x29,         // BIT_NOT                      <stack_top> = ~<stack_top>
    LSHIFT          = 0x2a,
    RSHIFT          = 0x2b,

    INP_ADD         = 0x2c,         // INP_ADD                      <stack_top + 1> += <stack_top>
    INP_SUB         = 0x2d,         // ...
    INP_MUL         = 0x2e,
    INP_DIV         = 0x2f,
    INP_MOD         = 0x30,
    INP_POWER       = 0x31,
    INP_BIT_OR      = 0x32,
    INP_BIT_AND     = 0x33,
    INP_BIT_XOR     = 0x34,
    INP_LSHIFT      = 0x35,
    INP_RSHIFT      = 0x36,

    BOOL_OR         = 0x37,         // BOOL_OR                      <stack_top> = <stack_top + 1> or <stack_top>
    BOOL_AND        = 0x38,         // ...
    BOOL_NOT        = 0x39,         // BOOL_NOT                     <stack_top> = not <stack_top>

    POS             = 0x3a,         // POS                          <stack_top> = +<stack_top>
    NEG             = 0x3b,         // NEG                          <stack_top> = -<stack_top>

    EQ              = 0x40,         // EQ                           <stack_top> = <stack_top + 1> == <stack_top>
    LE              = 0x41,         // ...
    GE              = 0x42,
    NEQ             = 0x43,
    LEQ             = 0x44,
    GEQ             = 0x45,
    IN              = 0x46,         // IN                           <stack_top> in <stack_top - 1>

    BR              = 0x50,         // BR               <pc>        Branch to <pc>
    BRTRUE          = 0x51,         // BRTRUE           <pc>        Branch to <pc> if <stack_top> represents True
    BRFALSE         = 0x52,         // BRFALSE          <pc>        Branch to <pc> if <stack_top> represents False

    RAISE           = 0x53,         // RAISE                        Raise an exception at <stack_top>
    PUSH_BLOCK      = 0x54,         // PUSH_BLOCK       <block>     Load exception rescure block
    POP_BLOCK       = 0x55,         // POP_BLOCK                    Restore stack and destroy rescure block

    ITER_NEXT       = 0x56,         // ITER_NEXT        <pc>        push(<stack_top>.__next__()), if StopIteration, goto <pc>
    EXPAND_SEQ      = 0x57,         // EXPAND_SEQ       <count>     Expand sequence on <stack_top> in reverse order
    IMPORT_ALIAS    = 0x58,         // IMPORT_ALIAS     <name>      Import a module <name>, from file <stack_top>

    MAKE_MAP        = 0x60,         // MAKE_MAP         <count>     Construct a map literal that contains <count> keys and values
    MAKE_ARRAY      = 0x61,         // MAKE_ARRAY       <count>     Construct an array literal that contains <count> items
    MAKE_TUPLE      = 0x62,         // MAKE_ARRAY       <count>     Construct a tuple literal that contains <count> items
    MAKE_FUNCTION   = 0x63,         // MAKE_FUNCTION    <code>      Store bytecodes into new function, with default values at <stack_top>
    MAKE_CLASS      = 0x64,         // MAKE_CLASS       <name>      Construct a class object named <name>
    MAKE_ITER       = 0x65,         // MAKE_ITER                    <stack_top> <-- <stack_top>.__iter__()
};

/* opcode flags */
static const uint32_t OP_V          = 0x00000001;    /* has operand */
static const uint32_t OP_REL        = 0x00000002;    /* relative to PC */

/* function invocation flags */
static const uint32_t FI_ARGS       = 0x00000001;    /* have arguments */
static const uint32_t FI_NAMED      = 0x00000002;    /* have named arguments */
static const uint32_t FI_VARGS      = 0x00000004;    /* have variable arguments */
static const uint32_t FI_KWARGS     = 0x00000008;    /* have keyword arguments */
static const uint32_t FI_DECORATOR  = 0x00000010;    /* decorator invocation */

/* flags for each opcode */
static uint32_t OpCodeFlags[256] = {
    OP_V,               /* 0x00 :: LOAD_CONST    */
    OP_V,               /* 0x01 :: LOAD_NAME     */
    OP_V,               /* 0x02 :: LOAD_LOCAL    */
    OP_V,               /* 0x03 :: STOR_LOCAL    */
    OP_V,               /* 0x04 :: DEL_LOCAL     */

    0,
    0,

    OP_V,               /* 0x07 :: DEF_ATTR      */
    OP_V,               /* 0x08 :: GET_ATTR      */
    OP_V,               /* 0x09 :: SET_ATTR      */
    OP_V,               /* 0x0a :: DEL_ATTR      */

    0,                  /* 0x0b :: GET_ITEM      */
    0,                  /* 0x0c :: SET_ITEM      */
    0,                  /* 0x0d :: DEL_ITEM      */

    0,                  /* 0x0e :: POP_RETURN    */
    OP_V,               /* 0x0f :: CALL_FUNCTION */

    0,                  /* 0x10 :: DUP           */
    0,                  /* 0x11 :: DUP2          */
    0,                  /* 0x12 :: DROP          */

    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,

    0,                  /* 0x20 :: ADD           */
    0,                  /* 0x21 :: SUB           */
    0,                  /* 0x22 :: MUL           */
    0,                  /* 0x23 :: DIV           */
    0,                  /* 0x24 :: MOD           */
    0,                  /* 0x25 :: POWER         */
    0,                  /* 0x26 :: BIT_OR        */
    0,                  /* 0x27 :: BIT_AND       */
    0,                  /* 0x28 :: BIT_XOR       */
    0,                  /* 0x29 :: BIT_NOT       */
    0,                  /* 0x2a :: LSHIFT        */
    0,                  /* 0x2b :: RSHIFT        */

    0,                  /* 0x2c :: INP_ADD       */
    0,                  /* 0x2d :: INP_SUB       */
    0,                  /* 0x2e :: INP_MUL       */
    0,                  /* 0x2f :: INP_DIV       */
    0,                  /* 0x30 :: INP_MOD       */
    0,                  /* 0x31 :: INP_POWER     */
    0,                  /* 0x32 :: INP_BIT_OR    */
    0,                  /* 0x33 :: INP_BIT_AND   */
    0,                  /* 0x34 :: INP_BIT_XOR   */
    0,                  /* 0x35 :: INP_LSHIFT    */
    0,                  /* 0x36 :: INP_RSHIFT    */

    0,                  /* 0x37 :: BOOL_OR       */
    0,                  /* 0x38 :: BOOL_AND      */
    0,                  /* 0x39 :: BOOL_NOT      */

    0,                  /* 0x3a :: POS           */
    0,                  /* 0x3b :: NEG           */

    0,
    0,
    0,
    0,

    0,                  /* 0x40 :: EQ            */
    0,                  /* 0x41 :: LE            */
    0,                  /* 0x42 :: GE            */
    0,                  /* 0x43 :: NEQ           */
    0,                  /* 0x44 :: LEQ           */
    0,                  /* 0x45 :: GEQ           */
    0,                  /* 0x46 :: IN            */

    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,

    OP_V | OP_REL,      /* 0x50 :: BR            */
    OP_V | OP_REL,      /* 0x51 :: BRTRUE        */
    OP_V | OP_REL,      /* 0x52 :: BRFALSE       */

    0,                  /* 0x53 :: RAISE         */
    OP_V,               /* 0x54 :: PUSH_BLOCK    */
    0,                  /* 0x55 :: POP_BLOCK     */

    OP_V,               /* 0x56 :: ITER_NEXT     */
    OP_V,               /* 0x57 :: EXPAND_SEQ    */
    OP_V,               /* 0x58 :: IMPORT_ALIAS  */

    0,
    0,
    0,
    0,
    0,
    0,
    0,

    OP_V,               /* 0x60 :: MAKE_MAP      */
    OP_V,               /* 0x61 :: MAKE_ARRAY    */
    OP_V,               /* 0x62 :: MAKE_TUPLE    */
    OP_V,               /* 0x63 :: MAKE_FUNCTION */
    OP_V,               /* 0x64 :: MAKE_CLASS    */
    0,                  /* 0x65 :: MAKE_ITER     */

    0,
    /* ... */
};

/* names for each opcode */
static const char *OpCodeNames[256] = {
    "LOAD_CONST",           /* 0x00 */
    "LOAD_NAME",            /* 0x01 */
    "LOAD_LOCAL",           /* 0x02 */
    "STOR_LOCAL",           /* 0x03 */
    "DEL_LOCAL",            /* 0x04 */

    nullptr,
    nullptr,

    "DEF_ATTR",             /* 0x07 */
    "GET_ATTR",             /* 0x08 */
    "SET_ATTR",             /* 0x09 */
    "DEL_ATTR",             /* 0x0a */

    "GET_ITEM",             /* 0x0b */
    "SET_ITEM",             /* 0x0c */
    "DEL_ITEM",             /* 0x0d */

    "POP_RETURN",           /* 0x0e */
    "CALL_FUNCTION",        /* 0x0f */

    "DUP",                  /* 0x10 */
    "DUP2",                 /* 0x11 */
    "DROP",                 /* 0x12 */

    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,

    "ADD",                  /* 0x20 */
    "SUB",                  /* 0x21 */
    "MUL",                  /* 0x22 */
    "DIV",                  /* 0x23 */
    "MOD",                  /* 0x24 */
    "POWER",                /* 0x25 */
    "BIT_OR",               /* 0x26 */
    "BIT_AND",              /* 0x27 */
    "BIT_XOR",              /* 0x28 */
    "BIT_NOT",              /* 0x29 */
    "LSHIFT",               /* 0x2a */
    "RSHIFT",               /* 0x2b */

    "INP_ADD",              /* 0x2c */
    "INP_SUB",              /* 0x2d */
    "INP_MUL",              /* 0x2e */
    "INP_DIV",              /* 0x2f */
    "INP_MOD",              /* 0x30 */
    "INP_POWER",            /* 0x31 */
    "INP_BIT_OR",           /* 0x32 */
    "INP_BIT_AND",          /* 0x33 */
    "INP_BIT_XOR",          /* 0x34 */
    "INP_LSHIFT",           /* 0x35 */
    "INP_RSHIFT",           /* 0x36 */

    "BOOL_OR",              /* 0x37 */
    "BOOL_AND",             /* 0x38 */
    "BOOL_NOT",             /* 0x39 */

    "POS",                  /* 0x3a */
    "NEG",                  /* 0x3b */

    nullptr,
    nullptr,
    nullptr,
    nullptr,

    "EQ",                   /* 0x40 */
    "LE",                   /* 0x41 */
    "GE",                   /* 0x42 */
    "NEQ",                  /* 0x43 */
    "LEQ",                  /* 0x44 */
    "GEQ",                  /* 0x45 */
    "IN",                   /* 0x46 */

    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,

    "BR",                   /* 0x50 */
    "BRTRUE",               /* 0x51 */
    "BRFALSE",              /* 0x52 */

    "RAISE",                /* 0x53 */
    "PUSH_BLOCK",           /* 0x54 */
    "POP_BLOCK",            /* 0x55 */

    "ITER_NEXT",            /* 0x56 */
    "EXPAND_SEQ",           /* 0x57 */
    "IMPORT_ALIAS",         /* 0x58 */

    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,

    "MAKE_MAP",             /* 0x60 */
    "MAKE_ARRAY",           /* 0x61 */
    "MAKE_TUPLE",           /* 0x62 */
    "MAKE_FUNCTION",        /* 0x63 */
    "MAKE_CLASS",           /* 0x64 */
    "MAKE_ITER",            /* 0x65 */

    nullptr,
    /* ... */
};
}

#endif /* REDSCRIPT_COMPILER_BYTECODE_H */
