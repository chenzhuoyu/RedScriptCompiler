#include <iostream>

#include "runtime/NullObject.h"
#include "runtime/TupleObject.h"
#include "runtime/StringObject.h"

#include "engine/Builtins.h"
#include "exceptions/TypeError.h"

namespace RedScript::Engine
{
/* built-in globals */
std::unordered_map<std::string, ClosureRef> Builtins::Globals;

Runtime::ObjectRef Builtins::dir(Runtime::ObjectRef obj)
{
    /* get the attribute list */
    auto attrs = obj->type()->objectDir(obj);
    auto tuple = Runtime::TupleObject::fromSize(attrs.size());

    /* convert each item into string object */
    for (size_t i = 0; i < attrs.size(); i++)
        tuple->items()[i] = Runtime::StringObject::fromString(attrs[i]);

    /* move to prevent copy */
    return std::move(tuple);
}

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

void Builtins::shutdown(void)
{
    /* clear before garbage collector shutdown */
    Globals.clear();
}

void Builtins::initialize(void)
{
    /* built-in functions */
    Globals.emplace("dir"   , Closure::ref(Runtime::NativeFunctionObject::newUnary(&dir)));
    Globals.emplace("print" , Closure::ref(Runtime::NativeFunctionObject::newVariadic(&print)));
}
}
