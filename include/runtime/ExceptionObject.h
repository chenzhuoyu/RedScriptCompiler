#ifndef REDSCRIPT_RUNTIME_EXCEPTIONOBJECT_H
#define REDSCRIPT_RUNTIME_EXCEPTIONOBJECT_H

#include "utils/NFI.h"
#include "runtime/Object.h"
#include "runtime/MapObject.h"
#include "runtime/TupleObject.h"

namespace RedScript::Runtime
{
class ExceptionType : public NativeType
{
public:
    using NativeType::NativeType;
    virtual ObjectRef nativeObjectNew(TypeRef type, Reference<TupleObject> args, Reference<MapObject> kwargs) override;
    virtual ObjectRef nativeObjectInit(ObjectRef self, Reference<TupleObject> args, Reference<MapObject> kwargs) override;

};

class ExceptionObject : public Object
{
public:
    using Object::Object;
    virtual void throwObject(void) = 0;

};

template <typename Exception, typename ... Args>
class NativeExceptionType : public ExceptionType
{
    class NativeExceptionObject : public ExceptionObject
    {
        Exception _exc;

    public:
        virtual ~NativeExceptionObject() = default;
        explicit NativeExceptionObject(TypeRef type, Exception &&exc) : ExceptionObject(type), _exc(std::move(exc)) {}

    public:
        virtual void throwObject(void) override { throw _exc; }

    };

public:
    using ExceptionType::ExceptionType;
    virtual ObjectRef nativeObjectNew(TypeRef type, Reference<TupleObject> args, Reference<MapObject> kwargs) override
    {
        /* for convenience, importing these specific symbols we will be using */
        using Utils::NFI::construct;
        using Utils::NFI::ArgsUnboxer;
        using Utils::NFI::KeywordNames;

        /* type check */
        if (type.get() != this)
            return ExceptionType::nativeObjectNew(std::move(type), std::move(args), std::move(kwargs));

        /* construct the exception object */
        KeywordNames names(sizeof ... (Args));
        return Object::newObject<NativeExceptionObject>(type, construct<Exception>(ArgsUnboxer<Args ...>::unbox(args, kwargs, names, {})));
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
