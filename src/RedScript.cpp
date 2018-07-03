#include "RedScript.h"

#include "engine/GarbageCollector.h"

#include "runtime/Object.h"
#include "runtime/IntObject.h"
#include "runtime/MapObject.h"
#include "runtime/BoolObject.h"
#include "runtime/CodeObject.h"
#include "runtime/NullObject.h"
#include "runtime/ArrayObject.h"
#include "runtime/TupleObject.h"
#include "runtime/StringObject.h"
#include "runtime/DecimalObject.h"
#include "runtime/FunctionObject.h"
#include "runtime/NativeClassObject.h"
#include "runtime/NativeFunctionObject.h"
#include "runtime/ExceptionBlockObject.h"

namespace RedScript
{
void shutdown(void)
{
    /* generic objects */
    Runtime::ExceptionBlockObject::shutdown();
    Runtime::NativeFunctionObject::shutdown();
    Runtime::NativeClassObject::shutdown();
    Runtime::FunctionObject::shutdown();
    Runtime::DecimalObject::shutdown();
    Runtime::TupleObject::shutdown();
    Runtime::ArrayObject::shutdown();
    Runtime::CodeObject::shutdown();
    Runtime::MapObject::shutdown();

    /* pooled objects */
    Runtime::StringObject::shutdown();
    Runtime::IntObject::shutdown();

    /* object sub-system */
    Runtime::_NullObject::shutdown();
    Runtime::BoolObject::shutdown();
    Runtime::Object::shutdown();

    /* memory management and garbage collector */
    Engine::GarbageCollector::shutdown();
}

void initialize(size_t young, size_t old, size_t perm)
{
    /* memory management and garbage collector */
    Engine::GarbageCollector::initialize(young, old, perm);

    /* object sub-system */
    Runtime::Object::initialize();
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
    Runtime::NativeClassObject::initialize();
    Runtime::NativeFunctionObject::initialize();
    Runtime::ExceptionBlockObject::initialize();
}
}
