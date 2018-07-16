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

Runtime::ObjectRef Builtins::print(Utils::NFI::VariadicArgs args, Utils::NFI::KeywordArgs kwargs)
{
    /* check for "end" and "delim" arguments */
    Runtime::ObjectRef end = kwargs->find(Runtime::StringObject::fromStringInterned("end"));
    Runtime::ObjectRef delim = kwargs->find(Runtime::StringObject::fromStringInterned("delim"));

    /* convert to strings, assign a default value if not present */
    std::string endStr = end.isNull() ? "\n" : end->type()->objectStr(end);
    std::string delimStr = delim.isNull() ? " " : delim->type()->objectStr(delim);

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

Runtime::ObjectRef Builtins::getattr(Runtime::ObjectRef self, const std::string &name, Runtime::ObjectRef def)
{
    /* no default values provided, call the getter directly
     * this won't eat the call stack when exceptions occured */
    if (def.isNull())
        return self->type()->objectGetAttr(self, name);

    /* try getting the attributes from object, return default value if not found */
    try { return self->type()->objectGetAttr(self, name); }
    catch (const Exceptions::AttributeError &) { return std::move(def); }
}

void Builtins::shutdown(void)
{
    /* clear before garbage collector shutdown */
    Globals.clear();
}

void Builtins::initialize(void)
{
    /* built-in print function */
    Globals.emplace(
        "print",
        Closure::ref(Runtime::NativeFunctionObject::newVariadic(&print))
    );

    /* built-in `id()` function */
    Globals.emplace(
        "id",
        Closure::ref(Runtime::NativeFunctionObject::fromFunction(
            Utils::NFI::KeywordNames({"obj"}),
            [](Runtime::ObjectRef self){ return reinterpret_cast<uintptr_t>(self.get()); }
        ))
    );

    /* built-in `dir()` function */
    Globals.emplace(
        "dir",
        Closure::ref(Runtime::NativeFunctionObject::fromFunction(
            Utils::NFI::KeywordNames({"obj"}),
            [](Runtime::ObjectRef self){ return self->type()->objectDir(self); }
        ))
    );

    /* built-in `len()` function */
    Globals.emplace(
        "len",
        Closure::ref(Runtime::NativeFunctionObject::fromFunction(
            Utils::NFI::KeywordNames({"obj"}),
            [](Runtime::ObjectRef self){ return self->type()->sequenceLen(self); }
        ))
    );

    /* built-in `hash()` function */
    Globals.emplace(
        "hash",
        Closure::ref(Runtime::NativeFunctionObject::fromFunction(
            Utils::NFI::KeywordNames({"obj"}),
            [](Runtime::ObjectRef self){ return self->type()->objectHash(self); }
        ))
    );

    /* built-in `iter()` function */
    Globals.emplace(
        "iter",
        Closure::ref(Runtime::NativeFunctionObject::fromFunction(
            Utils::NFI::KeywordNames({"obj"}),
            [](Runtime::ObjectRef self){ return self->type()->iterableIter(self); }
        ))
    );

    /* built-in `next()` function */
    Globals.emplace(
        "next",
        Closure::ref(Runtime::NativeFunctionObject::fromFunction(
            Utils::NFI::KeywordNames({"iter"}),
            [](Runtime::ObjectRef self){ return self->type()->iterableNext(self); }
        ))
    );

    /* built-in `repr()` function */
    Globals.emplace(
        "repr",
        Closure::ref(Runtime::NativeFunctionObject::fromFunction(
            Utils::NFI::KeywordNames({"obj"}),
            [](Runtime::ObjectRef self){ return self->type()->objectRepr(self); }
        ))
    );

    /* built-in `intern()` function */
    Globals.emplace(
        "intern",
        Closure::ref(Runtime::NativeFunctionObject::fromFunction(
            Utils::NFI::KeywordNames({"str"}),
            [](const std::string &str){ return Runtime::StringObject::fromStringInterned(str); }
        ))
    );

    /* built-in `hasattr()` function */
    Globals.emplace(
        "hasattr",
        Closure::ref(Runtime::NativeFunctionObject::fromFunction(
            Utils::NFI::KeywordNames({"obj", "attr"}),
            [](Runtime::ObjectRef self, const std::string &name){ return self->type()->objectHasAttr(self, name); }
        ))
    );

    /* built-in `delattr()` function */
    Globals.emplace(
        "delattr",
        Closure::ref(Runtime::NativeFunctionObject::fromFunction(
            Utils::NFI::KeywordNames({"obj", "attr"}),
            [](Runtime::ObjectRef self, const std::string &name){ self->type()->objectDelAttr(self, name); }
        ))
    );

    /* built-in `setattr()` function */
    Globals.emplace(
        "setattr",
        Closure::ref(Runtime::NativeFunctionObject::fromFunction(
            Utils::NFI::KeywordNames({"obj", "attr", "value"}),
            [](Runtime::ObjectRef self, const std::string &name, Runtime::ObjectRef value){ self->type()->objectSetAttr(self, name, value); }
        ))
    );

    /* built-in `getattr()` function */
    Globals.emplace(
        "getattr",
        Closure::ref(Runtime::NativeFunctionObject::fromFunction(
            Utils::NFI::KeywordNames({"obj", "attr", "value"}),
            Utils::NFI::DefaultValues({nullptr}),
            &getattr
        ))
    );
}
}
