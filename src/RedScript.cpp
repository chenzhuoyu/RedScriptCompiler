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
#include "runtime/BoundMethodObject.h"
#include "runtime/NativeClassObject.h"
#include "runtime/UnboundMethodObject.h"
#include "runtime/NativeFunctionObject.h"
#include "runtime/ExceptionBlockObject.h"

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

    /* generic objects */
    Runtime::ExceptionBlockTypeObject->clearBuiltins();
    Runtime::NativeFunctionTypeObject->clearBuiltins();
    Runtime::UnboundMethodTypeObject->clearBuiltins();
    Runtime::NativeClassTypeObject->clearBuiltins();
    Runtime::BoundMethodTypeObject->clearBuiltins();
    Runtime::FunctionTypeObject->clearBuiltins();
    Runtime::DecimalTypeObject->clearBuiltins();
    Runtime::TupleTypeObject->clearBuiltins();
    Runtime::ArrayTypeObject->clearBuiltins();
    Runtime::CodeTypeObject->clearBuiltins();
    Runtime::MapTypeObject->clearBuiltins();

    /* pooled objects */
    Runtime::StringTypeObject->clearBuiltins();
    Runtime::IntTypeObject->clearBuiltins();

    /* singleton objects */
    Runtime::NullTypeObject->clearBuiltins();
    Runtime::BoolTypeObject->clearBuiltins();

    /* meta objects */
    Runtime::SliceTypeObject->clearBuiltins();
    Runtime::ProxyTypeObject->clearBuiltins();
    Runtime::TypeObject->clearBuiltins();

    /** shutdown objects **/

    /* generic objects */
    Runtime::ExceptionBlockObject::shutdown();
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
    Runtime::ExceptionBlockObject::initialize();

    /** initialize built-in attributes and functions **/

    /* meta objects */
    Runtime::TypeObject->addBuiltins();
    Runtime::ProxyTypeObject->addBuiltins();
    Runtime::SliceTypeObject->addBuiltins();

    /* singleton objects */
    Runtime::BoolTypeObject->addBuiltins();
    Runtime::NullTypeObject->addBuiltins();

    /* pooled objects */
    Runtime::IntTypeObject->addBuiltins();
    Runtime::StringTypeObject->addBuiltins();

    /* generic objects */
    Runtime::MapTypeObject->addBuiltins();
    Runtime::CodeTypeObject->addBuiltins();
    Runtime::ArrayTypeObject->addBuiltins();
    Runtime::TupleTypeObject->addBuiltins();
    Runtime::DecimalTypeObject->addBuiltins();
    Runtime::FunctionTypeObject->addBuiltins();
    Runtime::BoundMethodTypeObject->addBuiltins();
    Runtime::NativeClassTypeObject->addBuiltins();
    Runtime::UnboundMethodTypeObject->addBuiltins();
    Runtime::NativeFunctionTypeObject->addBuiltins();
    Runtime::ExceptionBlockTypeObject->addBuiltins();

    /* built-in globals */
    Engine::Builtins::initialize();
}
}
