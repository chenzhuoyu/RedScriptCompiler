#include <iostream>

#include "engine/Builtins.h"
#include "runtime/IntObject.h"
#include "runtime/NullObject.h"
#include "runtime/TupleObject.h"
#include "runtime/StringObject.h"
#include "runtime/ExceptionObject.h"

#include "modules/FFI.h"
#include "modules/System.h"

namespace RedScript::Engine
{
/* built-in globals and modules */
std::unordered_map<std::string, ClosureRef> Builtins::Globals;
std::unordered_map<std::string, Runtime::ModuleRef> Builtins::Modules;

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
    catch (const Runtime::Exceptions::AttributeError &) { return std::move(def); }
}

void Builtins::shutdown(void)
{
    /* shutdown modules */
    Modules::FFI::shutdown();
    Modules::System::shutdown();

    /* clear all objects before collection */
    Runtime::ModuleObject::flush();
    Modules.clear();
    Globals.clear();
}

void Builtins::initialize(void)
{
    /* initialize modules */
    Modules::FFI::initialize();
    Modules::System::initialize();

    /* built-in objects */
    addObject("type"              , Runtime::TypeObject);
    addObject("object"            , Runtime::ObjectTypeObject);

    /* built-in base exceptions */
    addObject("Exception"         , Runtime::ExceptionTypeObject);
    addObject("BaseException"     , Runtime::BaseExceptionTypeObject);

    /* built-in exception objects */
    addObject("NameError"         , Runtime::NameErrorTypeObject);
    addObject("TypeError"         , Runtime::TypeErrorTypeObject);
    addObject("IndexError"        , Runtime::IndexErrorTypeObject);
    addObject("ValueError"        , Runtime::ValueErrorTypeObject);
    addObject("SyntaxError"       , Runtime::SyntaxErrorTypeObject);
    addObject("RuntimeError"      , Runtime::RuntimeErrorTypeObject);
    addObject("InternalError"     , Runtime::InternalErrorTypeObject);
    addObject("AttributeError"    , Runtime::AttributeErrorTypeObject);
    addObject("ZeroDivisionError" , Runtime::ZeroDivisionErrorTypeObject);
    addObject("NativeSyntaxError" , Runtime::NativeSyntaxErrorTypeObject);

    /* built-in non-error exceptions */
    addObject("SystemExit"        , Runtime::SystemExitTypeObject);
    addObject("StopIteration"     , Runtime::StopIterationTypeObject);

    /* built-in modules */
    addModule(Modules::FFIModule);
    addModule(Modules::SystemModule);

    /* built-in print function */
    addFunction(Runtime::NativeFunctionObject::newVariadic(
        "print",
        &print
    ));

    /* built-in `id()` function */
    addFunction(Runtime::NativeFunctionObject::fromFunction(
        "id",
        Utils::NFI::KeywordNames({"obj"}),
        [](Runtime::ObjectRef self){ return reinterpret_cast<uintptr_t>(self.get()); }
    ));

    /* built-in `dir()` function */
    addFunction(Runtime::NativeFunctionObject::fromFunction(
        "dir",
        Utils::NFI::KeywordNames({"obj"}),
        [](Runtime::ObjectRef self){ return self->type()->objectDir(self); }
    ));

    /* built-in `len()` function */
    addFunction(Runtime::NativeFunctionObject::fromFunction(
        "len",
        Utils::NFI::KeywordNames({"obj"}),
        [](Runtime::ObjectRef self){ return self->type()->sequenceLen(self); }
    ));

    /* built-in `hash()` function */
    addFunction(Runtime::NativeFunctionObject::fromFunction(
        "hash",
        Utils::NFI::KeywordNames({"obj"}),
        [](Runtime::ObjectRef self){ return self->type()->objectHash(self); }
    ));

    /* built-in `iter()` function */
    addFunction(Runtime::NativeFunctionObject::fromFunction(
        "iter",
        Utils::NFI::KeywordNames({"obj"}),
        [](Runtime::ObjectRef self){ return self->type()->iterableIter(self); }
    ));

    /* built-in `next()` function */
    addFunction(Runtime::NativeFunctionObject::fromFunction(
        "next",
        Utils::NFI::KeywordNames({"iter"}),
        [](Runtime::ObjectRef self){ return self->type()->iterableNext(self); }
    ));

    /* built-in `repr()` function */
    addFunction(Runtime::NativeFunctionObject::fromFunction(
        "repr",
        Utils::NFI::KeywordNames({"obj"}),
        [](Runtime::ObjectRef self){ return self->type()->objectRepr(self); }
    ));

    /* built-in `intern()` function */
    addFunction(Runtime::NativeFunctionObject::fromFunction(
        "intern",
        Utils::NFI::KeywordNames({"str"}),
        [](const std::string &str){ return Runtime::StringObject::fromStringInterned(str); }
    ));

    /* built-in `hasattr()` function */
    addFunction(Runtime::NativeFunctionObject::fromFunction(
        "hasattr",
        Utils::NFI::KeywordNames({"obj", "attr"}),
        [](Runtime::ObjectRef self, const std::string &name){ return self->type()->objectHasAttr(self, name); }
    ));

    /* built-in `delattr()` function */
    addFunction(Runtime::NativeFunctionObject::fromFunction(
        "delattr",
        Utils::NFI::KeywordNames({"obj", "attr"}),
        [](Runtime::ObjectRef self, const std::string &name){ self->type()->objectDelAttr(self, name); }
    ));

    /* built-in `setattr()` function */
    addFunction(Runtime::NativeFunctionObject::fromFunction(
        "setattr",
        Utils::NFI::KeywordNames({"obj", "attr", "value"}),
        [](Runtime::ObjectRef self, const std::string &name, Runtime::ObjectRef value){ self->type()->objectSetAttr(self, name, value); }
    ));

    /* built-in `getattr()` function */
    addFunction(Runtime::NativeFunctionObject::fromFunction(
        "getattr",
        Utils::NFI::KeywordNames({"obj", "attr", "value"}),
        Utils::NFI::DefaultValues({nullptr}),
        &getattr
    ));
}
}
