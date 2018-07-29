#include "utils/NFI.h"
#include "engine/Thread.h"

#include "runtime/IntObject.h"
#include "runtime/MapObject.h"
#include "runtime/BoolObject.h"
#include "runtime/NullObject.h"
#include "runtime/TupleObject.h"
#include "runtime/StringObject.h"
#include "runtime/ExceptionObject.h"
#include "runtime/UnboundMethodObject.h"

namespace RedScript::Runtime
{
/* built-in base exceptions */
TypeRef ExceptionTypeObject;
TypeRef BaseExceptionTypeObject;

/* built-in exception objects */
TypeRef NameErrorTypeObject;
TypeRef TypeErrorTypeObject;
TypeRef IndexErrorTypeObject;
TypeRef ValueErrorTypeObject;
TypeRef SyntaxErrorTypeObject;
TypeRef RuntimeErrorTypeObject;
TypeRef InternalErrorTypeObject;
TypeRef AttributeErrorTypeObject;
TypeRef ZeroDivisionErrorTypeObject;
TypeRef NativeSyntaxErrorTypeObject;

/* built-in non-error exceptions */
TypeRef SystemExitTypeObject;
TypeRef StopIterationTypeObject;

ObjectRef ExceptionType::nativeObjectNew(TypeRef type, Reference<TupleObject> args, Reference<MapObject> kwargs)
{
    return Object::newObject<ExceptionObject>(
        std::move(type),
        std::get<0>(Utils::NFI::ArgsUnboxer<const std::string &>::unbox(
            args,
            kwargs,
            {"message"},
            {StringObject::newEmpty()})
        )
    );
}

ObjectRef ExceptionType::nativeObjectInit(ObjectRef self, Reference<TupleObject> args, Reference<MapObject> kwargs)
{
    self->attrs().emplace("args", args);
    self->attrs().emplace("message", StringObject::fromString(self.as<ExceptionObject>()->message()));
    return self;
}

void ExceptionType::shutdown(void)
{
    /* built-in base exceptions */
    ExceptionTypeObject = nullptr;
    BaseExceptionTypeObject = nullptr;

    /* built-in exception objects */
    NameErrorTypeObject = nullptr;
    TypeErrorTypeObject = nullptr;
    IndexErrorTypeObject = nullptr;
    ValueErrorTypeObject = nullptr;
    SyntaxErrorTypeObject = nullptr;
    RuntimeErrorTypeObject = nullptr;
    InternalErrorTypeObject = nullptr;
    AttributeErrorTypeObject = nullptr;
    ZeroDivisionErrorTypeObject = nullptr;
    NativeSyntaxErrorTypeObject = nullptr;

    /* built-in non-error exceptions */
    SystemExitTypeObject = nullptr;
    StopIterationTypeObject = nullptr;
}

#define DECL_EXC(type)                                                          \
    static ExceptionType __exc_ ## type(#type, ExceptionTypeObject);            \
    type ## TypeObject = Reference<ExceptionType>::refStatic(__exc_ ## type)

#define DECL_EXC_NX(type)                                                       \
    static ExceptionType __exc_ ## type(#type, BaseExceptionTypeObject);        \
    type ## TypeObject = Reference<ExceptionType>::refStatic(__exc_ ## type)

void ExceptionType::initialize(void)
{
    /* base exception type */
    static ExceptionType baseExceptionType("BaseException", ObjectTypeObject);
    BaseExceptionTypeObject = Reference<ExceptionType>::refStatic(baseExceptionType);

    /* base error type */
    static ExceptionType exceptionType("Exception", BaseExceptionTypeObject);
    ExceptionTypeObject = Reference<ExceptionType>::refStatic(exceptionType);

    /* derivative exceptions */
    DECL_EXC(NameError);
    DECL_EXC(TypeError);
    DECL_EXC(IndexError);
    DECL_EXC(ValueError);
    DECL_EXC(SyntaxError);
    DECL_EXC(RuntimeError);
    DECL_EXC(InternalError);
    DECL_EXC(AttributeError);
    DECL_EXC(ZeroDivisionError);

    /* derivative non-error exceptions */
    DECL_EXC_NX(SystemExit);
    DECL_EXC_NX(StopIteration);

    /* native syntax error, it's a sub-class of `SyntaxError` */
    static ExceptionType nativeSyntaxErrorType("NativeSyntaxError", SyntaxErrorTypeObject);
    NativeSyntaxErrorTypeObject = Reference<ExceptionType>::refStatic(nativeSyntaxErrorType);
}

#undef DECL_EXC
#undef DECL_EXC_NX

void ExceptionType::addBuiltins(void)
{
    attrs().emplace(
        "__parent__",
        UnboundMethodObject::fromFunction([&](ObjectRef self)
        {
            auto parent = self.as<ExceptionObject>()->parent();
            return parent.isNull() ? NullObject : parent.as<Object>();
        })
    );

    attrs().emplace(
        "__traceback__",
        UnboundMethodObject::fromFunction([&](ObjectRef self)
        {
            // todo: dump traceback
        })
    );
}

ExceptionObject::ExceptionObject(TypeRef type, const std::string &message) : Object(type), _message(message)
{
    /* frame count and buffer */
    auto size = Engine::Thread::self()->frames.size();
    auto *stack = Engine::Thread::self()->frames.data();

    /* reserve space for traceback */
    _traceback.resize(size);

    /* traverse each frame */
    for (size_t i = 0; i < size; i++)
    {
        _traceback[i].row  = stack[i]->line().first;
        _traceback[i].col  = stack[i]->line().second;
        _traceback[i].file = stack[i]->file();
        _traceback[i].name = stack[i]->name();
    }
}

std::string ExceptionObject::format(void) const
{
    // TODO: implement this
    return std::string();
}

/** Special Exception :: NameError **/

ObjectRef NameErrorType::nativeObjectNew(TypeRef type, Reference<TupleObject> args, Reference<MapObject> kwargs)
{
    typedef Utils::NFI::MetaConstructor<NameErrorImpl, int, int, const std::string &> Constructor;
    return Constructor::construct(args, kwargs, {"row", "col", "message"}, {});
}

ObjectRef NameErrorType::nativeObjectInit(ObjectRef self, Reference<TupleObject> args, Reference<MapObject> kwargs)
{
    attrs().emplace("row", IntObject::fromInt(self.as<NameErrorImpl>()->row()));
    attrs().emplace("col", IntObject::fromInt(self.as<NameErrorImpl>()->col()));
    return ExceptionType::nativeObjectInit(self, args, kwargs);
}

/** Special Exception :: SyntaxError **/

ObjectRef SyntaxErrorType::nativeObjectNew(TypeRef type, Reference<TupleObject> args, Reference<MapObject> kwargs)
{
    typedef Utils::NFI::MetaConstructor<SyntaxErrorImpl, int, int, const std::string &> Constructor;
    return Constructor::construct(args, kwargs, {"row", "col", "message"}, {});
}

ObjectRef SyntaxErrorType::nativeObjectInit(ObjectRef self, Reference<TupleObject> args, Reference<MapObject> kwargs)
{
    attrs().emplace("row", IntObject::fromInt(self.as<SyntaxErrorImpl>()->row()));
    attrs().emplace("col", IntObject::fromInt(self.as<SyntaxErrorImpl>()->col()));
    return ExceptionType::nativeObjectInit(self, args, kwargs);
}

/** Special Exception :: NativeSyntaxError **/

ObjectRef NativeSyntaxErrorType::nativeObjectNew(TypeRef type, Reference<TupleObject> args, Reference<MapObject> kwargs)
{
    typedef Utils::NFI::MetaConstructor<NativeSyntaxErrorImpl, const std::string &, int, bool, const std::string &> Constructor;
    return Constructor::construct(args, kwargs, {"filename", "row", "is_warning", "message"}, {});
}

ObjectRef NativeSyntaxErrorType::nativeObjectInit(ObjectRef self, Reference<TupleObject> args, Reference<MapObject> kwargs)
{
    attrs().emplace("is_warning", BoolObject::fromBool(self.as<NativeSyntaxErrorImpl>()->isWarning()));
    attrs().emplace("filename"  , StringObject::fromString(self.as<NativeSyntaxErrorImpl>()->filename()));
    return SyntaxErrorType::nativeObjectInit(self, args, kwargs);
}

/** Non-error Exception :: SystemExit **/

ObjectRef SystemExitType::nativeObjectNew(TypeRef type, Reference<TupleObject> args, Reference<MapObject> kwargs)
{
    /* no arguments accepted */
    return Utils::NFI::MetaConstructor<SystemExitImpl>::construct(args, kwargs, {}, {});
}

/** Non-error Exception :: StopIteration **/

ObjectRef StopIterationType::nativeObjectNew(TypeRef type, Reference<TupleObject> args, Reference<MapObject> kwargs)
{
    /* no arguments accepted */
    return Utils::NFI::MetaConstructor<StopIterationImpl>::construct(args, kwargs, {}, {});
}
}
