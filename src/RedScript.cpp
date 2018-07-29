#include <cstdint>
#include <sys/resource.h>

#include "RedScript.h"

#include "engine/Builtins.h"
#include "engine/GarbageCollector.h"

#include "runtime/Object.h"
#include "runtime/IntObject.h"
#include "runtime/MapObject.h"
#include "runtime/BoolObject.h"
#include "runtime/CodeObject.h"
#include "runtime/NullObject.h"
#include "runtime/ArrayObject.h"
#include "runtime/ProxyObject.h"
#include "runtime/SliceObject.h"
#include "runtime/TupleObject.h"
#include "runtime/StringObject.h"
#include "runtime/DecimalObject.h"
#include "runtime/FunctionObject.h"
#include "runtime/ExceptionObject.h"
#include "runtime/BoundMethodObject.h"
#include "runtime/NativeClassObject.h"
#include "runtime/UnboundMethodObject.h"
#include "runtime/NativeFunctionObject.h"

static void setStackSize(size_t stack)
{
    struct rlimit rl;

    /* get current resource limit */
    if (getrlimit(RLIMIT_STACK, &rl) < 0)
    {
        fprintf(stderr, "*** ERROR :: cannot get stack size : [%d] %s", errno, strerror(errno));
        exit(-1);
    }

    /* update as needed */
    if (rl.rlim_cur < stack)
        rl.rlim_cur = stack;

    /* don't exceed the maximum limit */
    if (rl.rlim_cur >rl.rlim_max)
        rl.rlim_cur = rl.rlim_max;

    /* set current resource limit */
    if (setrlimit(RLIMIT_STACK, &rl) < 0)
    {
        fprintf(stderr, "*** ERROR :: cannot set stack size : [%d] %s", errno, strerror(errno));
        exit(-1);
    }
}

namespace RedScript
{
void shutdown(void)
{
    /** clear built-in attributes and functions **/

    /* shutdown built-in globals */
    Engine::Builtins::shutdown();

    /* built-in exceptions */
    // TODO: complete the exception list
    Runtime::NativeSyntaxErrorTypeObject->typeShutdown();
    Runtime::ZeroDivisionErrorTypeObject->typeShutdown();
    Runtime::AttributeErrorTypeObject->typeShutdown();
    Runtime::StopIterationTypeObject->typeShutdown();
    Runtime::BaseExceptionTypeObject->typeShutdown();
    Runtime::RuntimeErrorTypeObject->typeShutdown();
    Runtime::SyntaxErrorTypeObject->typeShutdown();
    Runtime::ValueErrorTypeObject->typeShutdown();
    Runtime::IndexErrorTypeObject->typeShutdown();
    Runtime::TypeErrorTypeObject->typeShutdown();
    Runtime::NameErrorTypeObject->typeShutdown();
    Runtime::ExceptionType::shutdown();

    /* generic objects */
    Runtime::NativeFunctionTypeObject->typeShutdown();
    Runtime::UnboundMethodTypeObject->typeShutdown();
    Runtime::NativeClassTypeObject->typeShutdown();
    Runtime::BoundMethodTypeObject->typeShutdown();
    Runtime::FunctionTypeObject->typeShutdown();
    Runtime::DecimalTypeObject->typeShutdown();
    Runtime::TupleTypeObject->typeShutdown();
    Runtime::ArrayTypeObject->typeShutdown();
    Runtime::CodeTypeObject->typeShutdown();
    Runtime::MapTypeObject->typeShutdown();

    /* pooled objects */
    Runtime::StringTypeObject->typeShutdown();
    Runtime::IntTypeObject->typeShutdown();

    /* singleton objects */
    Runtime::NullTypeObject->typeShutdown();
    Runtime::BoolTypeObject->typeShutdown();

    /* meta objects */
    Runtime::SliceTypeObject->typeShutdown();
    Runtime::ProxyTypeObject->typeShutdown();
    Runtime::TypeObject->typeShutdown();

    /** shutdown objects **/

    /* generic objects */
    Runtime::NativeFunctionObject::shutdown();
    Runtime::UnboundMethodObject::shutdown();
    Runtime::NativeClassObject::shutdown();
    Runtime::BoundMethodObject::shutdown();
    Runtime::FunctionObject::shutdown();
    Runtime::DecimalObject::shutdown();
    Runtime::TupleObject::shutdown();
    Runtime::ArrayObject::shutdown();
    Runtime::CodeObject::shutdown();
    Runtime::MapObject::shutdown();

    /* pooled objects */
    Runtime::StringObject::shutdown();
    Runtime::IntObject::shutdown();

    /* singleton objects */
    Runtime::_NullObject::shutdown();
    Runtime::BoolObject::shutdown();

    /* meta objects */
    Runtime::SliceObject::shutdown();
    Runtime::ProxyObject::shutdown();
    Runtime::Object::shutdown();

    /* perform a full garbage collection */
    Engine::GarbageCollector::collect(Engine::GarbageCollector::CollectionMode::Full);
}

void initialize(size_t stack)
{
    /* main program stack size */
    setStackSize(stack);

    /** initialize objects **/

    /* meta objects */
    Runtime::Object::initialize();
    Runtime::ProxyObject::initialize();
    Runtime::SliceObject::initialize();

    /* singleton objects */
    Runtime::BoolObject::initialize();
    Runtime::_NullObject::initialize();

    /* pooled objects */
    Runtime::IntObject::initialize();
    Runtime::StringObject::initialize();

    /* generic objects */
    Runtime::MapObject::initialize();
    Runtime::CodeObject::initialize();
    Runtime::ArrayObject::initialize();
    Runtime::TupleObject::initialize();
    Runtime::DecimalObject::initialize();
    Runtime::FunctionObject::initialize();
    Runtime::BoundMethodObject::initialize();
    Runtime::NativeClassObject::initialize();
    Runtime::UnboundMethodObject::initialize();
    Runtime::NativeFunctionObject::initialize();

    /** initialize built-in attributes and functions **/

    /* meta objects */
    Runtime::TypeObject->typeInitialize();
    Runtime::ProxyTypeObject->typeInitialize();
    Runtime::SliceTypeObject->typeInitialize();

    /* singleton objects */
    Runtime::BoolTypeObject->typeInitialize();
    Runtime::NullTypeObject->typeInitialize();

    /* pooled objects */
    Runtime::IntTypeObject->typeInitialize();
    Runtime::StringTypeObject->typeInitialize();

    /* generic objects */
    Runtime::MapTypeObject->typeInitialize();
    Runtime::CodeTypeObject->typeInitialize();
    Runtime::ArrayTypeObject->typeInitialize();
    Runtime::TupleTypeObject->typeInitialize();
    Runtime::DecimalTypeObject->typeInitialize();
    Runtime::FunctionTypeObject->typeInitialize();
    Runtime::BoundMethodTypeObject->typeInitialize();
    Runtime::NativeClassTypeObject->typeInitialize();
    Runtime::UnboundMethodTypeObject->typeInitialize();
    Runtime::NativeFunctionTypeObject->typeInitialize();

    /* built-in exceptions */
    Runtime::ExceptionType::initialize();
    Runtime::NameErrorTypeObject->typeInitialize();
    Runtime::TypeErrorTypeObject->typeInitialize();
    Runtime::IndexErrorTypeObject->typeInitialize();
    Runtime::ValueErrorTypeObject->typeInitialize();
    Runtime::SyntaxErrorTypeObject->typeInitialize();
    Runtime::RuntimeErrorTypeObject->typeInitialize();
    Runtime::BaseExceptionTypeObject->typeInitialize();
    Runtime::StopIterationTypeObject->typeInitialize();
    Runtime::AttributeErrorTypeObject->typeInitialize();
    Runtime::ZeroDivisionErrorTypeObject->typeInitialize();
    Runtime::NativeSyntaxErrorTypeObject->typeInitialize();

    /* built-in globals */
    Engine::Builtins::initialize();
}
}
