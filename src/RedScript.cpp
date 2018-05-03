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
#include "runtime/NativeClassObject.h"
#include "runtime/ExceptionBlockObject.h"

namespace RedScript
{
void shutdown(void)
{
    /* generic objects */
    RedScript::Runtime::ExceptionBlockObject::shutdown();
    RedScript::Runtime::NativeClassObject::shutdown();
    RedScript::Runtime::DecimalObject::shutdown();
    RedScript::Runtime::TupleObject::shutdown();
    RedScript::Runtime::ArrayObject::shutdown();
    RedScript::Runtime::CodeObject::shutdown();
    RedScript::Runtime::MapObject::shutdown();

    /* pooled objects */
    RedScript::Runtime::StringObject::shutdown();
    RedScript::Runtime::IntObject::shutdown();

    /* object sub-system */
    RedScript::Runtime::_NullObject::shutdown();
    RedScript::Runtime::BoolObject::shutdown();
    RedScript::Runtime::Object::shutdown();

    /* memory management and garbage collector */
    RedScript::Engine::GarbageCollector::shutdown();
}

void initialize(size_t young, size_t old, size_t perm)
{
    /* memory management and garbage collector */
    RedScript::Engine::GarbageCollector::initialize(young, old, perm);

    /* object sub-system */
    RedScript::Runtime::Object::initialize();
    RedScript::Runtime::BoolObject::initialize();
    RedScript::Runtime::_NullObject::initialize();

    /* pooled objects */
    RedScript::Runtime::IntObject::initialize();
    RedScript::Runtime::StringObject::initialize();

    /* generic objects */
    RedScript::Runtime::MapObject::initialize();
    RedScript::Runtime::CodeObject::initialize();
    RedScript::Runtime::ArrayObject::initialize();
    RedScript::Runtime::TupleObject::initialize();
    RedScript::Runtime::DecimalObject::initialize();
    RedScript::Runtime::NativeClassObject::initialize();
    RedScript::Runtime::ExceptionBlockObject::initialize();
}
}
