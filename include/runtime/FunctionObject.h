#ifndef REDSCRIPT_RUNTIME_FUNCTIONOBJECT_H
#define REDSCRIPT_RUNTIME_FUNCTIONOBJECT_H

#include <string>
#include <unordered_map>

#include "engine/Closure.h"
#include "runtime/Object.h"
#include "runtime/CodeObject.h"
#include "runtime/TupleObject.h"

namespace RedScript::Runtime
{
class FunctionType : public Type
{
public:
    explicit FunctionType() : Type("function") {}

protected:
    virtual void addBuiltins(void) override {}
    virtual void clearBuiltins(void) override {}

/*** Native Object Protocol ***/

public:
    virtual ObjectRef nativeObjectInvoke(ObjectRef self, ObjectRef args, ObjectRef kwargs) override;

};

/* type object for function */
extern TypeRef FunctionTypeObject;

class FunctionObject : public Object
{
    Reference<CodeObject> _code;
    Reference<TupleObject> _defaults;
    std::unordered_map<std::string, Engine::ClosureRef> _closure;

private:
    friend class FunctionType;

public:
    virtual ~FunctionObject() = default;
    explicit FunctionObject(
        Reference<CodeObject> code,
        Reference<TupleObject> defaults,
        std::unordered_map<std::string, Engine::ClosureRef> &closure
    ) : Object(FunctionTypeObject),
        _code(std::move(code)),
        _closure(std::move(closure)),
        _defaults(std::move(defaults)) { track(); }

public:
    virtual void referenceClear(void) override;
    virtual void referenceTraverse(VisitFunction visit) override;

public:
    ObjectRef invoke(Reference<TupleObject> args, Reference<MapObject> kwargs);

public:
    static void shutdown(void) {}
    static void initialize(void);

};
}

#endif /* REDSCRIPT_RUNTIME_FUNCTIONOBJECT_H */
