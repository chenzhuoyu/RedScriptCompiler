//const char *source = R"source(#!/usr/bin/env redscript
//native 'C' class NativeClass(clfags = '-Wall')
//{
//struct tc_comp_t
//{
//    int val_1;
//    int val_2;
//    struct {
//        int x;
//        int y;
//    };
//};
//
//typedef int (*ff)(long, float);
//int *fun(ff);
//int *fun(ff f) {
//    static int x = 1;
//    return &x;
//}
//
//struct tc_comp_t;
//typedef struct tc_comp_t TestComposite;
//
//static int b = 1000;
//
//long test(TestComposite ts, float f);
//static TestComposite test_func(int arg0, float arg1);
//
//extern int scanf(const char *fmt, ...);
//extern int printf(const char *fmt, ...);
//
//typedef enum {
//    item_1,
//    item_2,
//} test_enum_t;
//
//long test(TestComposite ts, float f)
//{
//    typedef struct tc_comp_t Test123;
//    printf("this is test\n");
//    return test_func(ts.val_1, f).val_2;
//}
//
//static TestComposite test_func(int arg0, float arg1)
//{
//    b += arg0;
//    printf("hello, world from native code, b = %d, &b = %p, this = %p, arg1 = %f\n", b, &b, (void *)test, arg1);
//    TestComposite tc;
//    tc.val_1 = 999;
//    tc.val_2 = 12345;
//    return tc;
//}
//}
//)source";

const char *source = R"source(
def fac(n) {
    if (n <= 1)
        return 1
    else
        return n * fac(n - 1)
}

print(fac(20))
)source";

#include <iostream>

#include <unistd.h>
#include <libtcc.h>
#include <sys/mman.h>

#include "RedScript.h"
#include "engine/Memory.h"
#include "engine/Builtins.h"
#include "engine/Interpreter.h"

#include "utils/Strings.h"
#include "runtime/Object.h"
#include "runtime/MapObject.h"
#include "runtime/CodeObject.h"
#include "runtime/NullObject.h"
#include "runtime/TupleObject.h"
#include "runtime/NativeFunctionObject.h"

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
            "%zu(%d) : %p (%s) :: %s\n",
            i,
            code->consts()[i]->refCount(),
            code->consts()[i].get(),
            code->consts()[i]->type()->name().c_str(),
            code->consts()[i]->type()->objectRepr(code->consts()[i]).c_str()
        );

        if (code->consts()[i]->isInstanceOf(RedScript::Runtime::CodeTypeObject))
            codes.push_back(code->consts()[i].as<RedScript::Runtime::CodeObject>());
    }

    std::cout << "--------------------- NAMES ---------------------" << std::endl;
    for (size_t i = 0; i < code->names().size(); i++)
        printf("%zu : %s\n", i, code->names()[i].c_str());

    std::cout << "--------------------- LOCALS ---------------------" << std::endl;
    for (size_t i = 0; i < code->locals().size(); i++)
        printf("%zu : %s\n", i, code->locals()[i].c_str());

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
    std::cout << "raw usage: "
              << RedScript::Engine::Memory::rawUsage() << " bytes, "
              << RedScript::Engine::Memory::rawCount() << " blocks" << std::endl;
    std::cout << "array usage: "
              << RedScript::Engine::Memory::arrayUsage() << " bytes, "
              << RedScript::Engine::Memory::arrayCount() << " blocks" << std::endl;
    std::cout << "object usage: "
              << RedScript::Engine::Memory::objectUsage() << " bytes, "
              << RedScript::Engine::Memory::objectCount() << " objects" << std::endl;
    dis(code);

    RedScript::Engine::Interpreter intp(code, RedScript::Engine::Builtins::Globals);
    std::cout << "--------------------- EVAL ---------------------" << std::endl;
    RedScript::Runtime::ObjectRef ret = intp.eval();

    std::cout << "--------------------- RETURN ---------------------" << std::endl;
    std::cout << ret->type()->objectRepr(ret) << std::endl;
}

int main()
{
    RedScript::initialize(16 * 1024 * 1024);
    run();
    RedScript::shutdown();

    std::cout << "--------------------- MEM ---------------------" << std::endl;
    std::cout << "raw usage: "
              << RedScript::Engine::Memory::rawUsage() << " bytes, "
              << RedScript::Engine::Memory::rawCount() << " blocks" << std::endl;
    std::cout << "array usage: "
              << RedScript::Engine::Memory::arrayUsage() << " bytes, "
              << RedScript::Engine::Memory::arrayCount() << " blocks" << std::endl;
    std::cout << "object usage: "
              << RedScript::Engine::Memory::objectUsage() << " bytes, "
              << RedScript::Engine::Memory::objectCount() << " objects" << std::endl;
    return 0;
}
