const char *source = R"source(#!/usr/bin/env redscript
native 'C' class NativeClass()
{
struct tc_comp_t
{
    int val_1;
    int val_2;
    struct {
        int x;
        int y;
    };
};

typedef int (*ff)(long, float);
int *fun(ff);
int *fun(ff f) {
    static int x = 1;
    return &x;
}

struct tc_comp_t;
typedef struct tc_comp_t TestComposite;

static int b = 1000;

long test(TestComposite ts, float f);
static TestComposite test_func(int arg0, float arg1);

extern int scanf(const char *fmt, ...);
extern int printf(const char *fmt, ...);

typedef enum {
    item_1,
    item_2,
} test_enum_t;

long test(TestComposite ts, float f)
{
    typedef struct tc_comp_t Test123;
    printf("this is test\n");
    return test_func(ts.val_1, f).val_2;
}

static TestComposite test_func(int arg0, float arg1)
{
    b += arg0;
    printf("hello, world from native code, b = %d, &b = %p, this = %p, arg1 = %f\n", b, &b, (void *)test, arg1);
    TestComposite tc;
    tc.val_1 = 999;
    tc.val_2 = 12345;
    return tc;
}
}
)source";

#include <iostream>

#include <unistd.h>
#include <libtcc.h>
#include <sys/mman.h>

#include "RedScript.h"
#include "engine/Memory.h"

#include "utils/Strings.h"
#include "runtime/Object.h"
#include "runtime/CodeObject.h"

#include "compiler/Parser.h"
#include "compiler/Tokenizer.h"
#include "compiler/CodeGenerator.h"

static void dis(RedScript::Runtime::Reference<RedScript::Runtime::CodeObject> code)
{
    std::vector<RedScript::Runtime::Reference<RedScript::Runtime::CodeObject>> codes;
    std::cout << "--------------------- CONSTS ---------------------" << std::endl;
    for (size_t i = 0; i < code->consts().size(); i++)
    {
        printf(
            "%zu(%d) : %p (%s) :: %s\n", i,
            code->consts()[i]->refCount(),
            code->consts()[i].get(),
            code->consts()[i]->type()->name().c_str(),
            code->consts()[i]->type()->objectRepr(code->consts()[i]).c_str()
        );

        if (code->consts()[i]->type() == RedScript::Runtime::CodeTypeObject)
            codes.push_back(code->consts()[i].as<RedScript::Runtime::CodeObject>());
    }

    std::cout << "--------------------- NAMES ---------------------" << std::endl;
    for (size_t i = 0; i < code->names().size(); i++)
        printf("%zu : %s\n", i, code->names()[i].c_str());

    std::cout << "--------------------- CODE ---------------------" << std::endl;
    const char *s = code->buffer().data();
    const char *p = code->buffer().data();
    const char *e = code->buffer().data() + code->buffer().size();
    while (p < e)
    {
        uint8_t op = (uint8_t)*p;
        auto line = code->lineNums()[p - s];

        if (!(RedScript::Engine::OpCodeFlags[op] & RedScript::Engine::OP_V))
            printf("%.4lx %3d:%-3d %15s\n", p - s, line.first, line.second, RedScript::Engine::OpCodeNames[op]);
        else
        {
            int32_t opv = *(int32_t *)(p + 1);
            if (!(RedScript::Engine::OpCodeFlags[op] & RedScript::Engine::OP_REL))
                printf("%.4lx %3d:%-3d %15s    %d\n", p - s, line.first, line.second, RedScript::Engine::OpCodeNames[op], opv);
            else
                printf("%.4lx %3d:%-3d %15s    %d -> %#lx\n", p - s, line.first, line.second, RedScript::Engine::OpCodeNames[op], opv, p - s + opv);
            p += sizeof(int32_t);
        }
        p++;
    }
    printf("%.4lx  (HALT)\n", e - s);

    for (auto &x : codes)
        dis(x);
}

static void run(void)
{
    RedScript::Compiler::Parser parser(std::make_unique<RedScript::Compiler::Tokenizer>(source));
    RedScript::Compiler::CodeGenerator codegen(parser.parse());
    RedScript::Runtime::Reference<RedScript::Runtime::CodeObject> code = codegen.build().as<RedScript::Runtime::CodeObject>();
    std::cout << "--------------------- MEM ---------------------" << std::endl;
    std::cout << "raw usage: " << RedScript::Engine::Memory::rawUsage() << std::endl;
    std::cout << "array usage: " << RedScript::Engine::Memory::arrayUsage() << std::endl;
    std::cout << "object usage: " << RedScript::Engine::Memory::objectUsage() << std::endl;
    dis(code);
}

#if 0
static std::string type2str(TCCState *s, TCCType *t, const char *n)
{
    int id = tcc_type_get_id(t);
    std::string name = n ?: tcc_type_get_name(t);

    if (!n || !strcmp(n, tcc_type_get_name(t)))
    {
        if (IS_ENUM(id))
            name = "enum " + name;
        else if (IS_UNION(id))
            name = "union " + name;
        else if (IS_STRUCT(id))
            name = "struct " + name;
    }
    else
    {
        name += " (aka. \"";

        if (IS_ENUM(id))
            name += "enum ";
        else if (IS_UNION(id))
            name += "union ";
        else if (IS_STRUCT(id))
            name += "struct ";

        name += tcc_type_get_name(t);
        name += "\")";
    }

    if (IS_ENUM(id))
    {
        name += " { ";
        tcc_type_list_items(s, t, [](TCCState *s, TCCType *t, const char *name, long long val, void *pn) -> char
        {
            *(std::string *)pn += name;
            *(std::string *)pn += " = ";
            *(std::string *)pn += std::to_string(val);
            *(std::string *)pn += ", ";
            return 1;
        }, &name);
        name += "}";
    }

    else if (IS_UNION(id) || IS_STRUCT(id))
    {
        name += " { ";
        tcc_type_list_fields(s, t, [](TCCState *s, TCCType *t, const char *name, TCCType *type, void *pn) -> char
        {
            *(std::string *)pn += type2str(s, type, nullptr);
            *(std::string *)pn += " ";
            *(std::string *)pn += name;
            *(std::string *)pn += "; ";
            return 1;
        }, &name);
        name += "}";
    }

    return name;
}

const char *source = R"source(#!/usr/bin/env redscript
native 'C' class NativeClass()
{
struct tc_comp_t
{
    int val_1;
    int val_2;
    struct {
        int x;
        int y;
    };
};

typedef int (*ff)(long, float);
int *fun(ff);
int *fun(ff f) {
    static int x = 1;
    return &x;
}

struct tc_comp_t;
typedef struct tc_comp_t TestComposite;

static int b = 1000;

long test(TestComposite ts, float f);
static TestComposite test_func(int arg0, float arg1);

extern int scanf(const char *fmt, ...);
extern int printf(const char *fmt, ...);

typedef enum {
    item_1,
    item_2,
} test_enum_t;

long test(TestComposite ts, float f)
{
    typedef struct tc_comp_t Test123;
    printf("this is test\n");
    return test_func(ts.val_1, f).val_2;
}

static TestComposite test_func(int arg0, float arg1)
{
    b += arg0;
    printf("hello, world from native code, b = %d, &b = %p, this = %p, arg1 = %f\n", b, &b, (void *)test, arg1);
    TestComposite tc;
    tc.val_1 = 999;
    tc.val_2 = 12345;
    return tc;
}
}
)source";

static void run(void)
{
    RedScript::Compiler::Parser parser(std::make_unique<RedScript::Compiler::Tokenizer>(source));
    auto native = parser.parseNative();
    std::cout << "---------------- native code ----------------" << std::endl;
    std::cout << native->code << std::endl;
    std::cout << "---------------------------------------------" << std::endl;

    std::cout << "raw usage: " << RedScript::Engine::Memory::rawUsage() << std::endl;
    std::cout << "array usage: " << RedScript::Engine::Memory::arrayUsage() << std::endl;
    std::cout << "object usage: " << RedScript::Engine::Memory::objectUsage() << std::endl;
    std::cout << "---------------------------------------------" << std::endl;

    TCCState *tcc = tcc_new();
    tcc_set_options(tcc, "-nostdlib");
    tcc_set_output_type(tcc, TCC_OUTPUT_MEMORY);

    int ret = tcc_compile_string(tcc, native->code.c_str());
    std::cout << "tcc-compile: " << ret << std::endl;

    if (ret < 0)
    {
        tcc_delete(tcc);
        return;
    }

    ret = tcc_relocate(tcc);
    std::cout << "tcc-relocate(): " << ret << std::endl;

    if (ret < 0)
    {
        tcc_delete(tcc);
        return;
    }

    tcc_list_types(tcc, [](TCCState *s, const char *name, TCCType *type, void *) -> char
    {
        std::cout << "* TYPE :: " << type2str(s, type, name) << std::endl;
        return 1;
    }, nullptr);

    tcc_list_functions(tcc, [](TCCState *s, const char *name, TCCFunction *func, void *) -> char
    {
        void *fp = tcc_function_get_addr(s, func);
        size_t nargs = tcc_function_get_nargs(func);
        TCCType *rettype = tcc_function_get_return_type(func);
        std::cout << "----------- FUNCTION \"" << name << "\" -----------" << std::endl;
        std::cout << "tcc-function-get-addr(): " << fp << std::endl;
        std::cout << "tcc-function-get-nargs(): " << nargs << std::endl;
        std::cout << "tcc-function-get-return-type(): " << type2str(s, rettype, nullptr) << std::endl;

        for (size_t i = 0; i < nargs; i++)
        {
            TCCType *t = tcc_function_get_arg_type(func, i);
            const char *n = tcc_function_get_arg_name(func, i);
            std::cout << "- arg " << i << ": name=" << std::string(n) << ", type=" << type2str(s, t, nullptr) << std::endl;
        }

        return 1;
    }, nullptr);
    std::cout << "----------- END FUNCTION -----------" << std::endl;

    TCCFunction *func = tcc_find_function(tcc, "test");
    std::cout << "tcc-find-function(\"test\"): " << func << std::endl;

    void *fp = tcc_function_get_addr(tcc, func);
    std::cout << "tcc-function-get-addr(\"test\"): " << fp << std::endl;

    auto val = ((long (*)(int, float))fp)(555, 1.234);
    std::cout << "native.test(): " << val << std::endl;
    tcc_delete(tcc);
}
#endif

int main()
{
    size_t young = 1 * 1024 * 1024 * 1024;     /* Young,   1G */
    size_t old   =      512 * 1024 * 1024;     /* Old  , 512M */
    size_t perm  =      128 * 1024 * 1024;     /* Perm , 128M */

    RedScript::initialize(young, old, perm);
    run();

    std::cout << "--------------------- MEM ---------------------" << std::endl;
    std::cout << "raw usage: " << RedScript::Engine::Memory::rawUsage() << std::endl;
    std::cout << "array usage: " << RedScript::Engine::Memory::arrayUsage() << std::endl;
    std::cout << "object usage: " << RedScript::Engine::Memory::objectUsage() << std::endl;
    RedScript::shutdown();
    return 0;
}
