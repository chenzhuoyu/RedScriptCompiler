#ifndef REDSCRIPT_RUNTIME_EXCEPTIONOBJECT_H
#define REDSCRIPT_RUNTIME_EXCEPTIONOBJECT_H

#include "utils/NFI.h"
#include "runtime/Object.h"
#include "runtime/MapObject.h"
#include "runtime/TupleObject.h"

namespace RedScript::Runtime
{
class ExceptionObject : public Object
{
public:
    using Object::Object;
    virtual void throwObject(void) = 0;

};

template <typename Exception, typename ... Args>
class ExceptionWrapperType : public NativeType
{
    class ExceptionWrapperObject : public ExceptionObject
    {
        Exception _exc;

    public:
        virtual ~ExceptionWrapperObject() = default;
        explicit ExceptionWrapperObject(TypeRef type, Exception &&exc) : ExceptionObject(type), _exc(std::move(exc)) {}

    public:
        virtual void throwObject(void) override { throw _exc; }

    };

public:
    using NativeType::NativeType;
    virtual ObjectRef nativeObjectNew(TypeRef type, Reference<TupleObject> args, Reference<MapObject> kwargs) override
    {
        /* for convenience, using the specific symbols we use here */
        using Utils::NFI::construct;
        using Utils::NFI::ArgsUnboxer;
        using Utils::NFI::KeywordNames;

        /* type check */
        if (type.get() != this)
            throw Exceptions::InternalError("Invalid exception construction");

        /* construct the exception object */
        KeywordNames names(sizeof ... (Args));
        return Object::newObject<ExceptionWrapperObject>(type, construct<Exception>(ArgsUnboxer<Args ...>::unbox(args, kwargs, names, {})));
    }

public:
    virtual ObjectRef nativeObjectInit(ObjectRef self, Reference<TupleObject> args, Reference<MapObject> kwargs) override
    {
        /* already initialized properly, just return as is */
        return self;
    }
};

/* built-in exception objects */
extern TypeRef NameErrorTypeObject;
extern TypeRef TypeErrorTypeObject;
extern TypeRef IndexErrorTypeObject;
extern TypeRef ValueErrorTypeObject;
extern TypeRef SyntaxErrorTypeObject;
extern TypeRef RuntimeErrorTypeObject;
extern TypeRef BaseExceptionTypeObject;
extern TypeRef StopIterationTypeObject;
extern TypeRef InternalErrorTypeObject;
extern TypeRef AttributeErrorTypeObject;
extern TypeRef ZeroDivisionErrorTypeObject;
extern TypeRef NativeSyntaxErrorTypeObject;

namespace ExceptionWrapper
{
/* type class initializer */
void shutdown(void);
void initialize(void);

/* type instance initializer */
void typeShutdown(void);
void typeInitialize(void);
}
}

#endif /* REDSCRIPT_RUNTIME_EXCEPTIONOBJECT_H */
