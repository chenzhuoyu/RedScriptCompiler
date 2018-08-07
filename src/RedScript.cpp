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
#include "runtime/ModuleObject.h"
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

    /* perform a full collection */
    Engine::GarbageCollector::collect(Engine::GarbageCollector::CollectionMode::Full);

    /* shutdown built-in globals and exceptions */
    Engine::Builtins::shutdown();
    Runtime::ExceptionType::shutdown();

    /* generic objects */
    Runtime::UnboundMethodTypeObject->typeShutdown();
    Runtime::NativeClassTypeObject->typeShutdown();
    Runtime::BoundMethodTypeObject->typeShutdown();
    Runtime::FunctionTypeObject->typeShutdown();
    Runtime::DecimalTypeObject->typeShutdown();
    Runtime::ModuleTypeObject->typeShutdown();
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
    Runtime::NativeFunctionTypeObject->typeShutdown();
    Runtime::SliceTypeObject->typeShutdown();
    Runtime::ProxyTypeObject->typeShutdown();
    Runtime::TypeObject->typeShutdown();

    /** shutdown objects **/

    /* generic objects */
    Runtime::UnboundMethodObject::shutdown();
    Runtime::NativeClassObject::shutdown();
    Runtime::BoundMethodObject::shutdown();
    Runtime::FunctionObject::shutdown();
    Runtime::DecimalObject::shutdown();
    Runtime::ModuleObject::shutdown();
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
    Runtime::NativeFunctionObject::shutdown();
    Runtime::SliceObject::shutdown();
    Runtime::ProxyObject::shutdown();
    Runtime::Object::shutdown();
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
    Runtime::NativeFunctionObject::initialize();

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
    Runtime::ModuleObject::initialize();
    Runtime::DecimalObject::initialize();
    Runtime::FunctionObject::initialize();
    Runtime::BoundMethodObject::initialize();
    Runtime::NativeClassObject::initialize();
    Runtime::UnboundMethodObject::initialize();

    /** initialize built-in attributes and functions **/

    /* meta objects */
    Runtime::TypeObject->typeInitialize();
    Runtime::ProxyTypeObject->typeInitialize();
    Runtime::SliceTypeObject->typeInitialize();
    Runtime::NativeFunctionTypeObject->typeInitialize();

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
    Runtime::ModuleTypeObject->typeInitialize();
    Runtime::DecimalTypeObject->typeInitialize();
    Runtime::FunctionTypeObject->typeInitialize();
    Runtime::BoundMethodTypeObject->typeInitialize();
    Runtime::NativeClassTypeObject->typeInitialize();
    Runtime::UnboundMethodTypeObject->typeInitialize();

    /* built-in globals and exceptions */
    Runtime::ExceptionType::initialize();
    Engine::Builtins::initialize();
}
}
