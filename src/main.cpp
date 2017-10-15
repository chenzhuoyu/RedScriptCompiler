#include <iostream>

#include "engine/Memory.h"
#include "engine/GarbageCollector.h"

#include "utils/Strings.h"
#include "runtime/Object.h"
#include "compiler/Parser.h"
#include "compiler/Tokenizer.h"

static std::ostream &margin(std::ostream &out, size_t level)
{
    for (size_t i = 0; i < level; i++)
        out << "|  ";
    return out;
}

static void printName(std::ostream &out, const std::unique_ptr<RedScript::Compiler::AST::Name> &name, size_t level = 0)
{
    margin(out, level) << "Name '" << name->name << "'" << std::endl;
}

static void printUnpack(std::ostream &out, const std::unique_ptr<RedScript::Compiler::AST::Unpack> &unpack, size_t level = 0)
{
    margin(out, level) << "Name (" << unpack->items.size() << " items)" << std::endl;
    for (const auto &item : unpack->items)
    {
        switch (item.type)
        {
            case RedScript::Compiler::AST::Unpack::Target::Type::Name:
            {
                margin(out, level + 1) << "Name" << std::endl;
                printName(out, item.name, level + 2);
                break;
            }

            case RedScript::Compiler::AST::Unpack::Target::Type::Subset:
            {
                margin(out, level + 1) << "Nested Unpack" << std::endl;
                printUnpack(out, item.subset, level + 2);
                break;
            }
        }
    }
}

static void printLiteral(std::ostream &out, const std::unique_ptr<RedScript::Compiler::AST::Literal> &literal, size_t level = 0)
{
    switch (literal->type)
    {
        case RedScript::Compiler::AST::Literal::Type::String  : margin(out, level) << "String " << RedScript::Utils::Strings::repr(literal->string.data(), literal->string.size()) << std::endl; break;
        case RedScript::Compiler::AST::Literal::Type::Decimal : margin(out, level) << "Decimal " << literal->decimal << std::endl; break;
        case RedScript::Compiler::AST::Literal::Type::Integer : margin(out, level) << "Integer " << literal->integer << std::endl; break;
    }
}

static void printComposite(std::ostream &out, const std::unique_ptr<RedScript::Compiler::AST::Composite> &compo, size_t level = 0)
{
    margin(out, level) << "Composite" << std::endl;

    switch (compo->vtype)
    {
        case RedScript::Compiler::AST::Composite::ValueType::Map: break;
        case RedScript::Compiler::AST::Composite::ValueType::Name:
        {
            printName(out, compo->name, level + 1);
            break;
        }

        case RedScript::Compiler::AST::Composite::ValueType::Array: break;
        case RedScript::Compiler::AST::Composite::ValueType::Tuple: break;

        case RedScript::Compiler::AST::Composite::ValueType::Literal:
        {
            printLiteral(out, compo->literal, level + 1);
            break;
        }

        case RedScript::Compiler::AST::Composite::ValueType::Function: break;
    }
}

static void printExpression(std::ostream &out, const std::unique_ptr<RedScript::Compiler::AST::Expression> &expr, size_t level = 0)
{
    if (!(expr->hasOp))
        margin(out, level) << "Expression" << std::endl;
    else
        margin(out, level) << "Unary Expression " << RedScript::Compiler::Token::toString(expr->first.op) << std::endl;

    switch (expr->first.type)
    {
        case RedScript::Compiler::AST::Expression::Operand::Type::Composite:
        {
            printComposite(out, expr->first.composite, level + 1);
            break;
        }

        case RedScript::Compiler::AST::Expression::Operand::Type::Expression:
        {
            printExpression(out, expr->first.expression, level + 1);
            break;
        }
    }

    for (const auto &term : expr->follows)
    {
        margin(out, level + 1) << "Following " << RedScript::Compiler::Token::toString(term.op) << std::endl;
        switch (term.type)
        {
            case RedScript::Compiler::AST::Expression::Operand::Type::Composite:
            {
                printComposite(out, term.composite, level + 2);
                break;
            }

            case RedScript::Compiler::AST::Expression::Operand::Type::Expression:
            {
                printExpression(out, term.expression, level + 2);
                break;
            }
        }
    }
}

void run(void)
{
    const char *source = R"source(#!/usr/bin/env redscript

# class Foo : Bar
# {
#     def __init__(self, a, b = 10, *c, **d) {}
#
#     def func(self, x, y)
#     {
#         if (x > y)
#             println('hello, world %d' % (self.a + self.b + 1))
#
#         for (i in x .. y)
#         {
#             self.a += x
#             println(self.b + y)
#         }
#     }
# }

1 * (x ^ y)

)source";

    RedScript::Compiler::Parser parser(std::make_unique<RedScript::Compiler::Tokenizer>(source));
    printExpression(std::cout, parser.parseExpression());
}

int main()
{
    RedScript::Engine::GarbageCollector::init();
    run();
    RedScript::Engine::GarbageCollector::shutdown();
    return 0;
}
