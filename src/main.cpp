#include <string>
#include <fstream>
#include <iostream>
#include <streambuf>

#include <unistd.h>
#include <libtcc.h>
#include <sys/mman.h>

#include "RedScript.h"
#include "engine/Memory.h"
#include "engine/Builtins.h"
#include "engine/Interpreter.h"
#include "compiler/CodeGenerator.h"

#include "utils/Strings.h"
#include "runtime/Object.h"
#include "runtime/MapObject.h"
#include "runtime/CodeObject.h"
#include "runtime/NullObject.h"
#include "runtime/TupleObject.h"
#include "runtime/ExceptionObject.h"
#include "runtime/NativeFunctionObject.h"

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

    std::cout << "--------------------- FREEVARS ---------------------" << std::endl;
    for (const auto &freeVar : code->freeVars())
        printf("%u : %s\n", code->nameMap()[freeVar], freeVar.c_str());

    std::cout << "--------------------- CODE ---------------------" << std::endl;
    const char *s = code->buffer().data();
    const char *p = code->buffer().data();
    const char *e = code->buffer().data() + code->buffer().size();
    while (p < e)
    {
        auto line = code->lineNums()[p - s];
        uint8_t op = (uint8_t)*p++;

        /* mnemonic */
        printf("%.4lx %3d:%-3d %15s", p - s - 1, line.first, line.second, RedScript::Engine::OpCodeNames[op]);

        /* operand 1 */
        if (RedScript::Engine::OpCodeFlags[op] & RedScript::Engine::OP_V)
        {
            int32_t opv = *(int32_t *)p;
            if (!(RedScript::Engine::OpCodeFlags[op] & RedScript::Engine::OP_REL1))
                printf("    %d", opv);
            else
                printf("    %d -> %#lx", opv, p - s + opv - 1);
            p += sizeof(int32_t);
        }

        /* operand 2 */
        if (RedScript::Engine::OpCodeFlags[op] & RedScript::Engine::OP_V2)
        {
            int32_t opv = *(int32_t *)p;
            if (!(RedScript::Engine::OpCodeFlags[op] & RedScript::Engine::OP_REL2))
                printf(", %d", opv);
            else
                printf(", %d -> %#lx", opv, p - s + opv - sizeof(int32_t) - 1);
            p += sizeof(int32_t);
        }

        printf("\n");
    }
    printf("%.4lx  (HALT)\n", e - s);

    for (auto &x : codes)
        dis(x);
}

static void run(void)
{
    const char *fname = "/Users/Oxygen/ClionProjects/RedScriptCompiler/test.red";
    std::ifstream ifs(fname);
    std::string source((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

    auto code = RedScript::Compiler::CodeGenerator::compile(source, fname);
    dis(code);

    RedScript::Engine::Interpreter intp("__main__", code, RedScript::Engine::Builtins::Globals);
    std::cout << "--------------------- EVAL ---------------------" << std::endl;
    timespec begin;
    clock_gettime(CLOCK_MONOTONIC, &begin);
    RedScript::Runtime::ObjectRef ret = intp.eval();

    std::cout << "--------------------- ELAPSED ---------------------" << std::endl;
    timespec end;
    clock_gettime(CLOCK_MONOTONIC, &end);
    std::cout << (end.tv_sec - begin.tv_sec) * 1000.0 + (end.tv_nsec - begin.tv_nsec) / 1000000.0 << " ms" << std::endl;

    std::cout << "--------------------- RETURN ---------------------" << std::endl;
    std::cout << ret->type()->objectRepr(ret) << std::endl;
}

int main()
{
    RedScript::initialize(16 * 1024 * 1024);
    run();
    RedScript::shutdown();
    return 0;
}
