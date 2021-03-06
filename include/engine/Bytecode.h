#ifndef REDSCRIPT_ENGINE_BYTECODE_H
#define REDSCRIPT_ENGINE_BYTECODE_H

#include <cstdint>

namespace RedScript::Engine
{
enum class OpCode : uint8_t
{
    LOAD_NULL       = 0x00,         // LOAD_NULL                                push null
    LOAD_TRUE       = 0x01,         // LOAD_TRUE                                push true
    LOAD_FALSE      = 0x02,         // LOAD_FALSE                               push false
    LOAD_CONST      = 0x03,         // LOAD_CONST       <const>                 push <const>
    LOAD_NAME       = 0x04,         // LOAD_NAME        <name>                  Load variable by <name> into stack
    LOAD_LOCAL      = 0x05,         // LOAD_LOCAL       <index>                 Load local variable <index> into stack
    STOR_LOCAL      = 0x06,         // STOR_LOCAL       <index>                 Store local variable <index> from stack
    DEL_LOCAL       = 0x07,         // DEL_LOCAL        <index>                 Delete local variable <index> (by setting it to NULL)

    DEF_ATTR        = 0x08,         // DEF_ATTR         <name>                  define <stack_top>.<name> = None
    GET_ATTR        = 0x09,         // GET_ATTR         <name>                  <stack_top> = <stack_top>.<name>
    SET_ATTR        = 0x0a,         // SET_ATTR         <name>                  <stack_top>.<name> = <stack_top + 1>
    DEL_ATTR        = 0x0b,         // DEL_ATTR         <name>                  delete <stack_top>.name

    GET_ITEM        = 0x0c,         // GET_ITEM                                 <stack_top> = <stack_top + 1>[<stack_top>]
    SET_ITEM        = 0x0d,         // SET_ITEM                                 <stack_top + 1>[<stack_top>] = <stack_top + 2>
    DEL_ITEM        = 0x0e,         // DEL_ITEM                                 delete <stack_top + 1>[<stack_top>]

    GET_SLICE       = 0x0f,         // GET_SLICE        <flags>                 <stack_top> = <stack_top + 3>[<stack_top + 2>:<stack_top + 1>:<stack_top>]
    SET_SLICE       = 0x10,         // SET_SLICE        <flags>                 <stack_top + 3>[<stack_top + 2>:<stack_top + 1>:<stack_top>] = <stack_top + 4>
    DEL_SLICE       = 0x11,         // DEL_SLICE        <flags>                 delete <stack_top + 3>[<stack_top + 2>:<stack_top + 1>:<stack_top>]

    POP_RETURN      = 0x18,         // POP_RETURN                               Pop and return <stack_top>
    CALL_FUNCTION   = 0x19,         // CALL_FUNCTION    <flags>                 Call function at stack top

    DUP             = 0x1b,         // DUP                                      Duplicate <stack_top>
    DUP2            = 0x1c,         // DUP2                                     Duplicate <stack_top> and <stack_top - 1>
    DROP            = 0x1d,         // DROP                                     Drop <stack_top>
    SWAP            = 0x1e,         // SWAP                                     Swap top 2 items on stack, so that <stack_top - 1> becomes <stack_top>
    ROTATE          = 0x1f,         // ROTATE                                   Rotate top 3 items on stack, so that <stack_top - 1> becomes <stack_top>

    ADD             = 0x20,         // ADD                                      <stack_top> = <stack_top + 1> + <stack_top>
    SUB             = 0x21,         // ...
    MUL             = 0x22,
    DIV             = 0x23,
    MOD             = 0x24,
    POWER           = 0x25,
    BIT_OR          = 0x26,
    BIT_AND         = 0x27,
    BIT_XOR         = 0x28,
    BIT_NOT         = 0x29,         // BIT_NOT                                  <stack_top> = ~<stack_top>
    LSHIFT          = 0x2a,
    RSHIFT          = 0x2b,

    INP_ADD         = 0x2c,         // INP_ADD                                  <stack_top + 1> += <stack_top>
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

    BOOL_OR         = 0x37,         // BOOL_OR                                  <stack_top> = <stack_top + 1> or <stack_top>
    BOOL_AND        = 0x38,         // ...
    BOOL_NOT        = 0x39,         // BOOL_NOT                                 <stack_top> = not <stack_top>

    POS             = 0x3a,         // POS                                      <stack_top> = +<stack_top>
    NEG             = 0x3b,         // NEG                                      <stack_top> = -<stack_top>

    EQ              = 0x40,         // EQ                                       <stack_top> = <stack_top + 1> == <stack_top>
    LT              = 0x41,         // ...
    GT              = 0x42,
    NEQ             = 0x43,
    LEQ             = 0x44,
    GEQ             = 0x45,
    IS              = 0x46,         // IS                                       <stack_top> is <stack_top - 1>
    IN              = 0x47,         // IN                                       <stack_top> in <stack_top - 1>

    BR              = 0x4a,         // BR               <pc>                    Branch to <pc>
    BRP_TRUE        = 0x4b,         // BRP_TRUE         <pc>                    Branch to <pc> if pop <stack_top> represents True
    BRP_FALSE       = 0x4c,         // BRP_FALSE        <pc>                    Branch to <pc> if pop <stack_top> represents False
    BRCP_TRUE       = 0x4d,         // BRCP_TRUE        <pc>                    Branch to <pc> if <stack_top> represents True, otherwise pop
    BRCP_FALSE      = 0x4e,         // BRCP_FALSE       <pc>                    Branch to <pc> if <stack_top> represents False, otherwise pop
    BRNP_TRUE       = 0x4f,         // BRNP_TRUE        <pc>                    Branch to <pc> if <stack_top> represents True
    BRNP_FALSE      = 0x50,         // BRNP_FALSE       <pc>                    Branch to <pc> if <stack_top> represents False

    RAISE           = 0x51,         // RAISE                                    Raise an exception at <stack_top>
    EXC_MATCH       = 0x52,         // EXC_MATCH        <pc>                    Exception in local state matches <stack_top>, jump if not matches
    EXC_STORE       = 0x53,         // EXC_STORE        <pc>, <local>           Exception in local state matches <stack_top>, store the exception to <local> if matches, jump if not
    SETUP_BLOCK     = 0x54,         // SETUP_BLOCK      <except>, <finally>     Setup exception rescure block with <except> and <finally>
    END_EXCEPT      = 0x55,         // END_EXCEPT                               End of the exception handling block
    END_FINALLY     = 0x56,         // END_FINALLY                              Mark the finally block ends

    ITER_NEXT       = 0x57,         // ITER_NEXT        <pc>                    push(<stack_top>.__next__()), if StopIteration, drop <stack_top> and goto <pc>
    EXPAND_SEQ      = 0x58,         // EXPAND_SEQ       <count>                 Expand sequence on <stack_top> in reverse order
    IMPORT_ALIAS    = 0x59,         // IMPORT_ALIAS     <name>                  Import a module <name>, from file <stack_top>

    MAKE_MAP        = 0x60,         // MAKE_MAP         <count>                 Construct a map literal that contains <count> keys and values
    MAKE_ARRAY      = 0x61,         // MAKE_ARRAY       <count>                 Construct an array literal that contains <count> items
    MAKE_TUPLE      = 0x62,         // MAKE_ARRAY       <count>                 Construct a tuple literal that contains <count> items
    MAKE_FUNCTION   = 0x63,         // MAKE_FUNCTION    <name>, <code>          Store bytecodes into new function, with default values at <stack_top>
    MAKE_CLASS      = 0x64,         // MAKE_CLASS       <name>, <code>          Construct a class object named <name>
    MAKE_ITER       = 0x65,         // MAKE_ITER                                <stack_top> <-- <stack_top>.__iter__()
    MAKE_NATIVE     = 0x66,         // MAKE_NATIVE      <name>, <code>          Construct a native class object with options on <stack_top>
};

/* opcode flags */
static const uint32_t OP_V          = 0x00000001;    /* has first operand */
static const uint32_t OP_V2         = 0x00000002;    /* has second operand */
static const uint32_t OP_REL        = 0x00000004;    /* operand 1 is relative to PC */
static const uint32_t OP_REL2       = 0x00000008;    /* operand 2 is relative to PC */

/* slicing flags */
static const uint32_t SL_BEGIN      = 0x00000001;    /* have beginning expression */
static const uint32_t SL_END        = 0x00000002;    /* have ending expression */
static const uint32_t SL_STEP       = 0x00000004;    /* have stepping expression */

/* function invocation flags */
static const uint32_t FI_ARGS       = 0x00000001;    /* have positional arguments */
static const uint32_t FI_NAMED      = 0x00000002;    /* have named arguments */
static const uint32_t FI_VARGS      = 0x00000004;    /* have variadic arguments */
static const uint32_t FI_KWARGS     = 0x00000008;    /* have keyword arguments */
static const uint32_t FI_DECORATOR  = 0x00000010;    /* decorator invocation, exclusive flag */

/* flags for each opcode */
static const uint32_t OpCodeFlags[256] = {
    0,                                  /* 0x00 :: LOAD_NULL     */
    0,                                  /* 0x01 :: LOAD_TRUE     */
    0,                                  /* 0x02 :: LOAD_FALSE    */
    OP_V,                               /* 0x03 :: LOAD_CONST    */
    OP_V,                               /* 0x04 :: LOAD_NAME     */
    OP_V,                               /* 0x05 :: LOAD_LOCAL    */
    OP_V,                               /* 0x06 :: STOR_LOCAL    */
    OP_V,                               /* 0x07 :: DEL_LOCAL     */

    OP_V,                               /* 0x08 :: DEF_ATTR      */
    OP_V,                               /* 0x09 :: GET_ATTR      */
    OP_V,                               /* 0x0a :: SET_ATTR      */
    OP_V,                               /* 0x0b :: DEL_ATTR      */

    0,                                  /* 0x0c :: GET_ITEM      */
    0,                                  /* 0x0d :: SET_ITEM      */
    0,                                  /* 0x0e :: DEL_ITEM      */

    OP_V,                               /* 0x0f :: GET_SLICE     */
    OP_V,                               /* 0x10 :: SET_SLICE     */
    OP_V,                               /* 0x11 :: DEL_SLICE     */

    0,
    0,
    0,
    0,
    0,
    0,

    0,                                  /* 0x18 :: POP_RETURN    */
    OP_V,                               /* 0x19 :: CALL_FUNCTION */

    0,

    0,                                  /* 0x1b :: DUP           */
    0,                                  /* 0x1c :: DUP2          */
    0,                                  /* 0x1d :: DROP          */
    0,                                  /* 0x1e :: SWAP          */
    0,                                  /* 0x1f :: ROTATE        */

    0,                                  /* 0x20 :: ADD           */
    0,                                  /* 0x21 :: SUB           */
    0,                                  /* 0x22 :: MUL           */
    0,                                  /* 0x23 :: DIV           */
    0,                                  /* 0x24 :: MOD           */
    0,                                  /* 0x25 :: POWER         */
    0,                                  /* 0x26 :: BIT_OR        */
    0,                                  /* 0x27 :: BIT_AND       */
    0,                                  /* 0x28 :: BIT_XOR       */
    0,                                  /* 0x29 :: BIT_NOT       */
    0,                                  /* 0x2a :: LSHIFT        */
    0,                                  /* 0x2b :: RSHIFT        */

    0,                                  /* 0x2c :: INP_ADD       */
    0,                                  /* 0x2d :: INP_SUB       */
    0,                                  /* 0x2e :: INP_MUL       */
    0,                                  /* 0x2f :: INP_DIV       */
    0,                                  /* 0x30 :: INP_MOD       */
    0,                                  /* 0x31 :: INP_POWER     */
    0,                                  /* 0x32 :: INP_BIT_OR    */
    0,                                  /* 0x33 :: INP_BIT_AND   */
    0,                                  /* 0x34 :: INP_BIT_XOR   */
    0,                                  /* 0x35 :: INP_LSHIFT    */
    0,                                  /* 0x36 :: INP_RSHIFT    */

    0,                                  /* 0x37 :: BOOL_OR       */
    0,                                  /* 0x38 :: BOOL_AND      */
    0,                                  /* 0x39 :: BOOL_NOT      */

    0,                                  /* 0x3a :: POS           */
    0,                                  /* 0x3b :: NEG           */

    0,
    0,
    0,
    0,

    0,                                  /* 0x40 :: EQ            */
    0,                                  /* 0x41 :: LT            */
    0,                                  /* 0x42 :: GT            */
    0,                                  /* 0x43 :: NEQ           */
    0,                                  /* 0x44 :: LEQ           */
    0,                                  /* 0x45 :: GEQ           */
    0,                                  /* 0x46 :: IS            */
    0,                                  /* 0x47 :: IN            */

    0,
    0,

    OP_V | OP_REL,                     /* 0x4a :: BR            */
    OP_V | OP_REL,                     /* 0x4b :: BRP_TRUE      */
    OP_V | OP_REL,                     /* 0x4c :: BRP_FALSE     */
    OP_V | OP_REL,                     /* 0x4d :: BRCP_TRUE     */
    OP_V | OP_REL,                     /* 0x4e :: BRCP_FALSE    */
    OP_V | OP_REL,                     /* 0x4f :: BRNP_TRUE     */
    OP_V | OP_REL,                     /* 0x50 :: BRNP_FALSE    */


    0,                                  /* 0x51 :: RAISE         */
    OP_V | OP_REL,                      /* 0x52 :: EXC_MATCH     */
    OP_V | OP_REL | OP_V2,              /* 0x53 :: EXC_STORE     */
    OP_V | OP_REL | OP_V2 | OP_REL2,    /* 0x54 :: SETUP_BLOCK   */
    0,                                  /* 0x55 :: END_EXCEPT    */
    0,                                  /* 0x56 :: END_FINALLY   */

    OP_V | OP_REL,                      /* 0x57 :: ITER_NEXT     */
    OP_V,                               /* 0x58 :: EXPAND_SEQ    */
    OP_V,                               /* 0x59 :: IMPORT_ALIAS  */

    0,
    0,
    0,
    0,
    0,
    0,

    OP_V,                               /* 0x60 :: MAKE_MAP      */
    OP_V,                               /* 0x61 :: MAKE_ARRAY    */
    OP_V,                               /* 0x62 :: MAKE_TUPLE    */
    OP_V | OP_V2,                       /* 0x63 :: MAKE_FUNCTION */
    OP_V | OP_V2,                       /* 0x64 :: MAKE_CLASS    */
    0,                                  /* 0x65 :: MAKE_ITER     */
    OP_V | OP_V2,                       /* 0x66 :: MAKE_NATIVE   */

    0,
    /* ... */
};

/* names for each opcode */
static const char *OpCodeNames[256] = {
    "LOAD_NULL",                        /* 0x00 */
    "LOAD_TRUE",                        /* 0x01 */
    "LOAD_FALSE",                       /* 0x02 */
    "LOAD_CONST",                       /* 0x03 */
    "LOAD_NAME",                        /* 0x04 */
    "LOAD_LOCAL",                       /* 0x05 */
    "STOR_LOCAL",                       /* 0x06 */
    "DEL_LOCAL",                        /* 0x07 */

    "DEF_ATTR",                         /* 0x08 */
    "GET_ATTR",                         /* 0x09 */
    "SET_ATTR",                         /* 0x0a */
    "DEL_ATTR",                         /* 0x0b */

    "GET_ITEM",                         /* 0x0c */
    "SET_ITEM",                         /* 0x0d */
    "DEL_ITEM",                         /* 0x0e */

    "GET_SLICE",                        /* 0x0f */
    "SET_SLICE",                        /* 0x10 */
    "DEL_SLICE",                        /* 0x11 */

    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,

    "POP_RETURN",                       /* 0x18 */
    "CALL_FUNCTION",                    /* 0x19 */

    nullptr,

    "DUP",                              /* 0x1b */
    "DUP2",                             /* 0x1c */
    "DROP",                             /* 0x1d */
    "SWAP",                             /* 0x1e */
    "ROTATE",                           /* 0x1f */

    "ADD",                              /* 0x20 */
    "SUB",                              /* 0x21 */
    "MUL",                              /* 0x22 */
    "DIV",                              /* 0x23 */
    "MOD",                              /* 0x24 */
    "POWER",                            /* 0x25 */
    "BIT_OR",                           /* 0x26 */
    "BIT_AND",                          /* 0x27 */
    "BIT_XOR",                          /* 0x28 */
    "BIT_NOT",                          /* 0x29 */
    "LSHIFT",                           /* 0x2a */
    "RSHIFT",                           /* 0x2b */

    "INP_ADD",                          /* 0x2c */
    "INP_SUB",                          /* 0x2d */
    "INP_MUL",                          /* 0x2e */
    "INP_DIV",                          /* 0x2f */
    "INP_MOD",                          /* 0x30 */
    "INP_POWER",                        /* 0x31 */
    "INP_BIT_OR",                       /* 0x32 */
    "INP_BIT_AND",                      /* 0x33 */
    "INP_BIT_XOR",                      /* 0x34 */
    "INP_LSHIFT",                       /* 0x35 */
    "INP_RSHIFT",                       /* 0x36 */

    "BOOL_OR",                          /* 0x37 */
    "BOOL_AND",                         /* 0x38 */
    "BOOL_NOT",                         /* 0x39 */

    "POS",                              /* 0x3a */
    "NEG",                              /* 0x3b */

    nullptr,
    nullptr,
    nullptr,
    nullptr,

    "EQ",                               /* 0x40 */
    "LT",                               /* 0x41 */
    "GT",                               /* 0x42 */
    "NEQ",                              /* 0x43 */
    "LEQ",                              /* 0x44 */
    "GEQ",                              /* 0x45 */
    "IS",                               /* 0x46 */
    "IN",                               /* 0x47 */

    nullptr,
    nullptr,

    "BR",                               /* 0x4a */
    "BRP_TRUE",                         /* 0x4b */
    "BRP_FALSE",                        /* 0x4c */
    "BRCP_TRUE",                        /* 0x4d */
    "BRCP_FALSE",                       /* 0x4e */
    "BRNP_TRUE",                        /* 0x4f */
    "BRNP_FALSE",                       /* 0x50 */

    "RAISE",                            /* 0x51 */
    "EXC_MATCH",                        /* 0x52 */
    "EXC_STORE",                        /* 0x53 */
    "SETUP_BLOCK",                      /* 0x54 */
    "END_EXCEPT",                       /* 0x55 */
    "END_FINALLY",                      /* 0x56 */

    "ITER_NEXT",                        /* 0x57 */
    "EXPAND_SEQ",                       /* 0x58 */
    "IMPORT_ALIAS",                     /* 0x59 */

    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,

    "MAKE_MAP",                         /* 0x60 */
    "MAKE_ARRAY",                       /* 0x61 */
    "MAKE_TUPLE",                       /* 0x62 */
    "MAKE_FUNCTION",                    /* 0x63 */
    "MAKE_CLASS",                       /* 0x64 */
    "MAKE_ITER",                        /* 0x65 */
    "MAKE_NATIVE",                      /* 0x66 */

    nullptr,
    /* ... */
};
}

#endif /* REDSCRIPT_ENGINE_BYTECODE_H */
