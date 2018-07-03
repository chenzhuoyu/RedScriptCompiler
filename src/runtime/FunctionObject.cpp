#include <algorithm>

#include "runtime/MapObject.h"
#include "runtime/NullObject.h"
#include "runtime/StringObject.h"
#include "runtime/FunctionObject.h"

#include "exceptions/TypeError.h"
#include "exceptions/InternalError.h"

#include "utils/Strings.h"
#include "engine/Interpreter.h"

namespace RedScript::Runtime
{
/* type object for function */
TypeRef FunctionTypeObject;

ObjectRef FunctionType::objectInvoke(ObjectRef self, ObjectRef args, ObjectRef kwargs)
{
    /* check object type */
    if (self->isNotInstanceOf(FunctionTypeObject))
        throw Exceptions::InternalError("Invalid function call");

    /* check tuple type */
    if (args->isNotInstanceOf(TupleTypeObject))
        throw Exceptions::InternalError("Invalid args tuple object");

    /* check map type */
    if (kwargs->isNotInstanceOf(MapTypeObject))
        throw Exceptions::InternalError("Invalid kwargs map object");

    /* call the function handler */
    return self.as<FunctionObject>()->invoke(args.as<TupleObject>(), kwargs.as<MapObject>());
}

ObjectRef FunctionObject::invoke(Reference<TupleObject> args, Reference<MapObject> kwargs)
{
    /* check for argument count */
    if (_code->args().size() > UINT32_MAX)
        throw Exceptions::TypeError("Too many arguments");

    /* check for default value count */
    if (_code->args().size() < _defaults->size())
        throw Exceptions::TypeError("Too many default values");

    /* create a new interpreter */
    Engine::Interpreter intp(_code, _closure);
    std::vector<ObjectRef> argv(_code->args().size());

    /* check positional arguments */
    if (_code->vargs().empty() && (args->size() > argv.size()))
    {
        if (_defaults->size() == 0)
        {
            throw Exceptions::TypeError(Utils::Strings::format(
                "Function takes exact %zu argument(s), but %zu given",
                argv.size(),
                args->size()
            ));
        }
        else
        {
            throw Exceptions::TypeError(Utils::Strings::format(
                "Function takes at most %zu argument(s), but %zu given",
                argv.size(),
                args->size()
            ));
        }
    }

    /* calculate positional argument count */
    size_t n = 0;
    size_t argc = 0;
    size_t count = std::min(argv.size(), args->size());

    /* extra positional arguments and keyword arguments */
    auto argsMap = Object::newObject<MapObject>(MapObject::Mode::Ordered);
    auto argsTuple = Object::newObject<TupleObject>(args->size() <= argv.size() ? 0 : args->size() - argv.size());

    /* fill positional arguments */
    while (argc < count)
        argv[n++] = args->items()[argc++];

    /* fill extra positional arguments, if needed */
    if (!(_code->vargs().empty()))
        for (size_t i = count; i < args->size(); i++)
            argsTuple->items()[i - count] = args->items()[i];

    /* fill named arguments */
    kwargs->enumerate([&](ObjectRef key, ObjectRef value)
    {
        /* name must be string */
        if (key->isNotInstanceOf(StringTypeObject))
            throw Exceptions::InternalError("Invalid \"kwargs\" key type");

        /* read it's name */
        auto keyStr = key.as<StringObject>();
        auto keyIter = _code->localMap().find(keyStr->value());

        /* check for name and name ID */
        if ((keyIter == _code->localMap().end()) || (keyIter->second >= argv.size()))
        {
            /* check if the function accepts kwargs */
            if (_code->kwargs().empty())
            {
                throw Exceptions::TypeError(Utils::Strings::format(
                    "Function does not accept keyword argument \"%s\"",
                    keyStr->value()
                ));
            }

            /* add to keyword arguments list */
            argsMap->insert(key, value);
            return true;
        }
        else
        {
            /* increase counter if not present */
            if (argv[keyIter->second].isNull())
            {
                /* non-default argument counter */
                if (keyIter->second < argv.size() - _defaults->size())
                    argc++;

                /* total argument counter */
                count++;
            }

            /* set into arguments */
            argv[keyIter->second] = value;
            return true;
        }
    });

    /* fill missing values using default value */
    for (size_t i = _defaults->size(); i > 0; i--)
    {
        if (argv[argv.size() - i].isNull())
        {
            count++;
            argv[argv.size() - i] = _defaults->items()[_defaults->size() - i];
        }
    }

    /* check for argument count */
    if (count < argv.size())
    {
        if (_defaults->size() == 0)
        {
            throw Exceptions::TypeError(Utils::Strings::format(
                "Function takes exact %zu argument(s), but %zu given",
                argv.size(),
                count
            ));
        }
        else
        {
            throw Exceptions::TypeError(Utils::Strings::format(
                "Function takes at least %zu argument(s), but %zu given",
                argv.size() - _defaults->size(),
                argc
            ));
        }
    }

    /* set into locals, arguments start from ID 0 */
    for (uint32_t id = 0; id < argv.size(); id++)
        intp.setLocal(id, argv[id]);

    /* set variadic arguments if any */
    if (!(_code->vargs().empty()))
        intp.setLocal(_code->vargs(), argsTuple);

    /* set keyword arguments if any */
    if (!(_code->kwargs().empty()))
        intp.setLocal(_code->kwargs(), argsMap);

    /* run the bytecodes */
    return intp.eval();
}

void FunctionObject::initialize(void)
{
    /* function type object */
    static FunctionType functionType;
    FunctionTypeObject = Reference<FunctionType>::refStatic(functionType);
}
}
