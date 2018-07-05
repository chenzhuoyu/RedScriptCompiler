#include <iostream>

#include "runtime/IntObject.h"
#include "runtime/NullObject.h"
#include "runtime/TupleObject.h"
#include "runtime/StringObject.h"

#include "engine/Builtins.h"
#include "exceptions/TypeError.h"
#include "exceptions/AttributeError.h"

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

Runtime::ObjectRef Builtins::len(Runtime::ObjectRef obj)
{
    /* get it's length, and wrap with int object */
    return Runtime::IntObject::fromInt(obj->type()->sequenceLen(obj));
}

Runtime::ObjectRef Builtins::hash(Runtime::ObjectRef obj)
{
    /* get it's hash, and wrap with int object */
    return Runtime::IntObject::fromInt(obj->type()->objectHash(obj));
}

Runtime::ObjectRef Builtins::repr(Runtime::ObjectRef obj)
{
    /* get it's representation, and wrap with string object */
    return Runtime::StringObject::fromString(obj->type()->objectRepr(obj));
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

bool Builtins::hasattr(Runtime::ObjectRef self, const std::string &name)
{
    try
    {
        /* try get the attributes from object */
        self->type()->objectGetAttr(self, name);
        return true;
    }
    catch (const Exceptions::AttributeError &)
    {
        /* attribute not found */
        return false;
    }
}

void Builtins::delattr(Runtime::ObjectRef self, const std::string &name)
{
    /* call the object's `__delattr__` */
    self->type()->objectDelAttr(self, name);
}

void Builtins::setattr(Runtime::ObjectRef self, const std::string &name, Runtime::ObjectRef value)
{
    /* call the object's `__setattr__` */
    self->type()->objectSetAttr(self, name, value);
}

Runtime::ObjectRef Builtins::getattr(Runtime::ObjectRef self, const std::string &name, Runtime::ObjectRef def)
{
    try
    {
        /* try get the attributes from object */
        return self->type()->objectGetAttr(self, name);
    }

    /* attribute not found, return default value if any */
    catch (const Exceptions::AttributeError &)
    {
        if (def.isNull())
            throw;
        else
            return def;
    }
}

void Builtins::shutdown(void)
{
    /* clear before garbage collector shutdown */
    Globals.clear();
}

void Builtins::initialize(void)
{
    /* built-in basic functions */
    Globals.emplace("dir"   , Closure::ref(Runtime::NativeFunctionObject::newUnary(&dir)));
    Globals.emplace("len"   , Closure::ref(Runtime::NativeFunctionObject::newUnary(&len)));
    Globals.emplace("hash"  , Closure::ref(Runtime::NativeFunctionObject::newUnary(&hash)));
    Globals.emplace("repr"  , Closure::ref(Runtime::NativeFunctionObject::newUnary(&repr)));
    Globals.emplace("print" , Closure::ref(Runtime::NativeFunctionObject::newVariadic(&print)));

    /* built-in `hasattr()` function */
    Globals.emplace(
        "hasattr",
        Closure::ref(Runtime::NativeFunctionObject::fromFunction(
            Runtime::KeywordNames({"obj", "attr"}),
            &hasattr
        ))
    );

    /* built-in `delattr()` function */
    Globals.emplace(
        "delattr",
        Closure::ref(Runtime::NativeFunctionObject::fromFunction(
            Runtime::KeywordNames({"obj", "attr"}),
            &delattr
        ))
    );

    /* built-in `setattr()` function */
    Globals.emplace(
        "setattr",
        Closure::ref(Runtime::NativeFunctionObject::fromFunction(
            Runtime::KeywordNames({"obj", "attr", "value"}),
            &setattr
        ))
    );

    /* built-in `getattr()` function */
    Globals.emplace(
        "getattr",
        Closure::ref(Runtime::NativeFunctionObject::fromFunction(
            Runtime::KeywordNames({"obj", "attr", "value"}),
            Runtime::DefaultValues({nullptr}),
            &getattr
        ))
    );
}
}
