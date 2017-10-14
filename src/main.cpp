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

static void printComposite(std::ostream &out, const std::unique_ptr<RedScript::Compiler::AST::Composite> &expr, size_t level = 0)
{

}

static void printExpression(std::ostream &out, const std::unique_ptr<RedScript::Compiler::AST::Expression> &expr, size_t level = 0)
{
    if (!(expr->right))
        margin(out, level) << "Expression" << std::endl;
    else if (!(expr->left))
        margin(out, level) << "Unary Expression " << RedScript::Compiler::Token::toString(expr->op) << std::endl;
    else
        margin(out, level) << "Binary Expression " << RedScript::Compiler::Token::toString(expr->op) << std::endl;

    if (expr->left)
    {
        switch (expr->left->type)
        {
            case RedScript::Compiler::AST::Expression::Operand::Type::Composite:
            {
                margin(out, level + 1) << "Left Composite" << std::endl;
                printComposite(out, expr->left->composite, level + 2);
                break;
            }

            case RedScript::Compiler::AST::Expression::Operand::Type::Expression:
            {
                margin(out, level + 1) << "Left Expression" << std::endl;
                printExpression(out, expr->left->expression, level + 2);
                break;
            }
        }
    }

    if (expr->right)
    {
        margin(out, level + 1) << "Right" << std::endl;
        printExpression(out, expr->right, level + 2);
    }
}

void run(void)
{
    RedScript::Compiler::Parser parser(std::make_unique<RedScript::Compiler::Tokenizer>(R"source(#!/usr/bin/env redscript

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

1 + 1

)source"));

    printExpression(std::cerr, parser.parseExpression());
}

int main()
{
    RedScript::Engine::GarbageCollector::init();
    run();
    RedScript::Engine::GarbageCollector::shutdown();
    return 0;
}
