#include "runtime/ExceptionObject.h"

#include "exceptions/NameError.h"
#include "exceptions/TypeError.h"
#include "exceptions/IndexError.h"
#include "exceptions/ValueError.h"
#include "exceptions/SyntaxError.h"
#include "exceptions/RuntimeError.h"
#include "exceptions/BaseException.h"
#include "exceptions/StopIteration.h"
#include "exceptions/InternalError.h"
#include "exceptions/AttributeError.h"
#include "exceptions/UserDefinedError.h"
#include "exceptions/ZeroDivisionError.h"
#include "exceptions/NativeSyntaxError.h"

namespace RedScript::Runtime
{
/* built-in exception objects */
TypeRef NameErrorTypeObject;
TypeRef TypeErrorTypeObject;
TypeRef IndexErrorTypeObject;
TypeRef ValueErrorTypeObject;
TypeRef SyntaxErrorTypeObject;
TypeRef RuntimeErrorTypeObject;
TypeRef BaseExceptionTypeObject;
TypeRef StopIterationTypeObject;
TypeRef InternalErrorTypeObject;
TypeRef AttributeErrorTypeObject;
TypeRef ZeroDivisionErrorTypeObject;
TypeRef NativeSyntaxErrorTypeObject;

class UserExceptionObject : public ExceptionObject
{
public:
    using ExceptionObject::ExceptionObject;
    virtual void throwObject(void) override { throw Exceptions::UserDefinedError(self()); }

};

ObjectRef ExceptionType::nativeObjectNew(TypeRef type, Reference<TupleObject> args, Reference<MapObject> kwargs)
{
    /* create the exception object */
    return Object::newObject<UserExceptionObject>(type);
}

ObjectRef ExceptionType::nativeObjectInit(ObjectRef self, Reference<TupleObject> args, Reference<MapObject> kwargs)
{
    /* store the exception arguments */
    self->attrs().emplace("args", args);
    return self;
}

namespace ExceptionWrapper
{
void shutdown(void)
{
    NameErrorTypeObject = nullptr;
    TypeErrorTypeObject = nullptr;
    IndexErrorTypeObject = nullptr;
    ValueErrorTypeObject = nullptr;
    SyntaxErrorTypeObject = nullptr;
    RuntimeErrorTypeObject = nullptr;
    BaseExceptionTypeObject = nullptr;
    StopIterationTypeObject = nullptr;
    InternalErrorTypeObject = nullptr;
    AttributeErrorTypeObject = nullptr;
    ZeroDivisionErrorTypeObject = nullptr;
    NativeSyntaxErrorTypeObject = nullptr;
}

#define DECL_EXC(type, ...)                                                                                             \
    static NativeExceptionType<Exceptions::type, ## __VA_ARGS__> __exc_ ## type(#type, BaseExceptionTypeObject);        \
    type ## TypeObject = Reference<NativeExceptionType<Exceptions::type, ## __VA_ARGS__>>::refStatic(__exc_ ## type)

void initialize(void)
{
    /* base exception type */
    static NativeExceptionType<Exceptions::BaseException, const std::string &> baseExceptionType("BaseException", ObjectTypeObject);
    BaseExceptionTypeObject = Reference<NativeExceptionType<Exceptions::BaseException, const std::string &>>::refStatic(baseExceptionType);

    /* derivative exceptions */
    DECL_EXC(NameError, int, int, const std::string &);
    DECL_EXC(TypeError, const std::string &);
    DECL_EXC(IndexError, const std::string &);
    DECL_EXC(ValueError, const std::string &);
    DECL_EXC(SyntaxError, int, int, const std::string &);
    DECL_EXC(RuntimeError, const std::string &);
    DECL_EXC(StopIteration);
    DECL_EXC(InternalError, const std::string &);
    DECL_EXC(AttributeError, const std::string &);
    DECL_EXC(ZeroDivisionError, const std::string &);
    DECL_EXC(NativeSyntaxError, const std::string &, int, bool, const std::string &);
}

#undef DECL_EXC

void typeShutdown(void)
{
    NativeSyntaxErrorTypeObject->typeShutdown();
    ZeroDivisionErrorTypeObject->typeShutdown();
    AttributeErrorTypeObject->typeShutdown();
    InternalErrorTypeObject->typeShutdown();
    StopIterationTypeObject->typeShutdown();
    BaseExceptionTypeObject->typeShutdown();
    RuntimeErrorTypeObject->typeShutdown();
    SyntaxErrorTypeObject->typeShutdown();
    ValueErrorTypeObject->typeShutdown();
    IndexErrorTypeObject->typeShutdown();
    TypeErrorTypeObject->typeShutdown();
    NameErrorTypeObject->typeShutdown();
}

void typeInitialize(void)
{
    NameErrorTypeObject->typeInitialize();
    TypeErrorTypeObject->typeInitialize();
    IndexErrorTypeObject->typeInitialize();
    ValueErrorTypeObject->typeInitialize();
    SyntaxErrorTypeObject->typeInitialize();
    RuntimeErrorTypeObject->typeInitialize();
    BaseExceptionTypeObject->typeInitialize();
    StopIterationTypeObject->typeInitialize();
    InternalErrorTypeObject->typeInitialize();
    AttributeErrorTypeObject->typeInitialize();
    ZeroDivisionErrorTypeObject->typeInitialize();
    NativeSyntaxErrorTypeObject->typeInitialize();
}
}
}
