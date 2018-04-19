#include "RedScript.h"

#include "engine/GarbageCollector.h"

#include "runtime/Object.h"
#include "runtime/IntObject.h"
#include "runtime/CodeObject.h"
#include "runtime/StringObject.h"
#include "runtime/DecimalObject.h"

namespace RedScript
{
void shutdown(void)
{
    /* memory management and garbage collector */
    RedScript::Engine::GarbageCollector::shutdown();
}

void initialize(size_t young, size_t old, size_t perm)
{
    /* memory management and garbage collector */
    RedScript::Engine::GarbageCollector::initialize(young, old, perm);

    /* object sub-system */
    RedScript::Runtime::Object::initialize();
    RedScript::Runtime::IntObject::initialize();
    RedScript::Runtime::CodeObject::initialize();
    RedScript::Runtime::StringObject::initialize();
    RedScript::Runtime::DecimalObject::initialize();

}
}
