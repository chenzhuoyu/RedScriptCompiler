#ifndef REDSCRIPT_COMPILER_BYTECODE_H
#define REDSCRIPT_COMPILER_BYTECODE_H

#include <stdint.h>

namespace RedScript::Engine::Bytecode
{
/* indicates that the opcode has an operand */
static const uint8_t OP_V    = 0x80;
static const uint8_t OP_MASK = ~OP_V;

enum OpCode : uint8_t
{
    LOAD_CONST      = 0x00 | OP_V,          // LOAD_CONST       <const>     push <const>
    LOAD_OBJECT     = 0x01 | OP_V,          // LOAD_OBJECT      <name>      push <name>
    STOR_OBJECT     = 0x02 | OP_V,          // STOR_OBJECT      <name>      pop -> <name>
    DEL_OBJECT      = 0x03 | OP_V,          // DEL_OBJECT       <name>      delete <name>

    DEF_ATTR        = 0x04 | OP_V,          // DEF_ATTR         <name>      define <stack_top>.<name> = None
    GET_ATTR        = 0x05 | OP_V,          // GET_ATTR         <name>      <stack_top> = <stack_top>.<name>
    SET_ATTR        = 0x06 | OP_V,          // SET_ATTR         <name>      <stack_top + 1>.<name> = <stack_top>
    DEL_ATTR        = 0x07 | OP_V,          // DEL_ATTR         <name>      delete <stack_top>.name

    GET_ITEM        = 0x08,                 // GET_ITEM                     <stack_top> = <stack_top + 1>[<stack_top>]
    SET_ITEM        = 0x09,                 // SET_ITEM                     <stack_top + 2>[<stack_top + 1>] = <stack_top>
    DEL_ITEM        = 0x0a,                 // DEL_ITEM                     delete <stack_top + 1>[<stack_top>]

    POP_RETURN      = 0x0b,                 // POP_RETURN                   return <stack_top>
    INVOKE_VARG     = 0x0c,                 // INVOKE_VARG                  <stack_top> = <stack_top>(expand(<stack_top + 1>))

    ADD             = 0x0d,                 // ADD                          <stack_top> = <stack_top + 1> + <stack_top>
    SUB             = 0x0e,                 // ...
    MUL             = 0x0f,
    DIV             = 0x00,
    MOD             = 0x11,
    POW             = 0x12,
    BIT_OR          = 0x13,
    BIT_AND         = 0x14,
    BIT_XOR         = 0x15,
    BIT_NOT         = 0x16,                 // BIT_NOT                      <stack_top> = ~<stack_top>
    LSHIFT          = 0x17,
    RSHIFT          = 0x18,

    INP_ADD         = 0x19,                 // INP_ADD                      <stack_top + 1> += <stack_top>
    INP_SUB         = 0x1a,                 // ...
    INP_MUL         = 0x1b,
    INP_DIV         = 0x1c,
    INP_MOD         = 0x1d,
    INP_POW         = 0x1e,
    INP_BIT_OR      = 0x1f,
    INP_BIT_AND     = 0x10,
    INP_BIT_XOR     = 0x21,
    INP_LSHIFT      = 0x22,
    INP_RSHIFT      = 0x23,

    BOOL_OR         = 0x24,                 // BOOL_OR                      <stack_top> = <stack_top + 1> or <stack_top>
    BOOL_AND        = 0x25,                 // ...
    BOOL_XOR        = 0x26,
    BOOL_NOT        = 0x27,                 // BOOL_NOT                     <stack_top> = not <stack_top>

    EQ              = 0x28,                 // EQ                           <stack_top> = <stack_top + 1> == <stack_top>
    LE              = 0x29,                 // ...
    GE              = 0x2a,
    NEQ             = 0x2b,
    LEQ             = 0x2c,
    GEQ             = 0x2d,

    POS             = 0x2e,                 // POS                          <stack_top> = +<stack_top>
    NEG             = 0x2f,                 // NEG                          <stack_top> = -<stack_top>

    DUP             = 0x30,                 // DUP                          Duplicate <stack_top>
    DUP2            = 0x31,                 // DUP2                         Duplicate <stack_top> and <stack_top - 1>
    DROP            = 0x32,                 // DROP                         Drop <stack_top>

    LOAD_ARG        = 0x33 | OP_V,          // LOAD_ARG         <arg>       Store <stack_top> as argument
    MAKE_FUNCTION   = 0x34 | OP_V,          // MAKE_FUNCTION    <nargs>     Store bytecodes into new function

    BR              = 0x35 | OP_V,          // BR               <pc>        Branch to <pc>
    BRTRUE          = 0x36 | OP_V,          // BRTRUE           <pc>        Branch to <pc> if <stack_top> represents True
    BRFALSE         = 0x37 | OP_V,          // BRFALSE          <pc>        Branch to <pc> if <stack_top> represents False

    PUSH_BLOCK      = 0x38 | OP_V,          // PUSH_BLOCK       <block>     Load exception rescure block
    POP_BLOCK       = 0x39,                 // POP_BLOCK                    Restore stack and destroy rescure block

    PUSH_SEQ        = 0x40 | OP_V,          // PUSH_SEQ         <offset>    Push sequence
    CHECK_SEQ       = 0x41,                 // CHECK_SEQ                    Check sequence and restore stack
    STORE_SEQ       = 0x42,                 // STORE_SEQ                    Store sequence and restore stack
    EXPAND_SEQ      = 0x43 | OP_V,          // EXPAND_SEQ       <count>     Expand sequence on <stack_top>

    RAISE           = 0x44,                 // RAISE                        Raise an exception at <stack_top>
    IMPORT_ALIAS    = 0x45 | OP_V,          // IMPORT_ALIAS     <name>      Import a module as <name>

    MAKE_MAP        = 0x46 | OP_V,          // MAKE_MAP         <count>     Construct a map literal that contains <count> keys and values
    MAKE_ARRAY      = 0x47 | OP_V,          // MAKE_ARRAY       <count>     Construct an array literal that contains <count> items
    MAKE_TUPLE      = 0x48 | OP_V,          // MAKE_ARRAY       <count>     Construct a tuple literal that contains <count> items
    MAKE_CLASS      = 0x49 | OP_V,          // MAKE_CLASS       <name>      Construct a class object named <name>

    MAKE_ITER       = 0x4a,                 // MAKE_ITER                    <stack_top> <-- <stack_top>.__iter__()
    ITER_NEXT       = 0x4b | OP_V,          // ITER_NEXT        <pc>        push(<stack_top>.__next__()), if StopIteration, goto <pc>
};
}

#endif /* REDSCRIPT_COMPILER_BYTECODE_H */
