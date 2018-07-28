#include "runtime/MapObject.h"
#include "runtime/NullObject.h"
#include "runtime/StringObject.h"
#include "runtime/FunctionObject.h"
#include "runtime/UnboundMethodObject.h"

#include "exceptions/TypeError.h"
#include "exceptions/InternalError.h"

#include "utils/Strings.h"
#include "engine/Interpreter.h"

namespace RedScript::Runtime
{
/* type object for function */
TypeRef FunctionTypeObject;

void FunctionType::addBuiltins(void)
{
    attrs().emplace(
        "__invoke__",
        UnboundMethodObject::newUnboundVariadic([](ObjectRef self, Reference<TupleObject> args, Reference<MapObject> kwargs)
        {
            /* invoke the object protocol */
            return self->type()->objectInvoke(self, args, kwargs);
        })
    );
}

/*** Native Object Protocol ***/

std::string FunctionType::nativeObjectRepr(ObjectRef self)
{
    auto func = self.as<FunctionObject>();
    return Utils::Strings::format("<function \"%s\" at %p>", func->name(), static_cast<void *>(func.get()));
}

ObjectRef FunctionType::nativeObjectInvoke(ObjectRef self, Reference<TupleObject> args, Reference<MapObject> kwargs)
{
    if (self->isNotInstanceOf(FunctionTypeObject))
        throw Exceptions::InternalError("Invalid function call");
    else
        return self.as<FunctionObject>()->invoke(std::move(args), std::move(kwargs));
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
    Engine::Interpreter vm(_name, _code, _closure);
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
    size_t count = argv.size() < args->size() ? argv.size() : args->size();

    /* extra positional arguments and keyword arguments */
    auto namedArgs = MapObject::newOrdered();
    auto indexArgs = TupleObject::fromSize(args->size() <= argv.size() ? 0 : args->size() - argv.size());

    /* fill positional arguments */
    while (argc < count)
        argv[n++] = args->items()[argc++];

    /* fill extra positional arguments, if needed */
    if (!(_code->vargs().empty()))
        for (size_t i = count; i < args->size(); i++)
            indexArgs->items()[i - count] = args->items()[i];

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
            namedArgs->insert(key, value);
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
        vm.locals(id) = argv[id];

    /* set variadic arguments if any */
    if (!(_code->vargs().empty()))
        vm.locals(_code->vargs()) = std::move(indexArgs);

    /* set keyword arguments if any */
    if (!(_code->kwargs().empty()))
        vm.locals(_code->kwargs()) = std::move(namedArgs);

    /* run the bytecodes */
    return vm.eval();
}

void FunctionObject::referenceClear(void)
{
    /* clear every closure */
    for (auto &item : _closure)
        item.second = nullptr;

    /* clear code object and default map */
    _code = nullptr;
    _defaults = nullptr;
}

void FunctionObject::referenceTraverse(VisitFunction visit)
{
    /* closure objects */
    for (const auto &item : _closure)
        visit(item.second->get());

    /* code object and default map */
    visit(_code);
    visit(_defaults);
}

void FunctionObject::shutdown(void)
{
    /* clear type instance */
    FunctionTypeObject = nullptr;
}

void FunctionObject::initialize(void)
{
    /* function type object */
    static FunctionType functionType;
    FunctionTypeObject = Reference<FunctionType>::refStatic(functionType);
}
}
