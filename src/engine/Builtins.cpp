#include <iostream>

#include "runtime/NullObject.h"
#include "runtime/StringObject.h"

#include "engine/Builtins.h"
#include "exceptions/TypeError.h"

namespace RedScript::Engine
{
Runtime::ObjectRef Builtins::print(Runtime::VariadicArgs args, Runtime::KeywordArgs kwargs)
{
    /* check for "end" and "delim" arguments */
    auto end = kwargs->find(Runtime::StringObject::fromString("end"));
    auto delim = kwargs->find(Runtime::StringObject::fromString("delim"));

    /* assign a default value if not present */
    if (end.isNull()) end = Runtime::StringObject::fromString("\n");
    if (delim.isNull()) delim = Runtime::StringObject::fromString(" ");

    /* convert to strings */
    auto endStr = end->type()->objectStr(end);
    auto delimStr = delim->type()->objectStr(delim);

    /* print each item */
    for (size_t i = 0; i < args->size(); i++)
    {
        /* print delimiter if not the first element */
        if (i != 0)
            std::cout << delimStr;

        /* print the item */
        auto item = args->items()[i];
        std::cout << item->type()->objectStr(item);
    }

    /* print the end string */
    std::cout << endStr;
    return Runtime::NullObject;
}

Builtins::BuiltinClosure &Builtins::closure(void)
{
    /* create the built-in closure */
    static BuiltinClosure builtinClosure = {
        { "print", Closure::ref(Runtime::NativeFunctionObject::newVariadic(&print)) },
    };

    /* return it's reference */
    return builtinClosure;
}
}
