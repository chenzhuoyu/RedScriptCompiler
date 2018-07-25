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
class FunctionType : public NativeType
{
public:
    explicit FunctionType() : NativeType("function") {}

protected:
    virtual void addBuiltins(void) override;
    virtual void clearBuiltins(void) override {}

/*** Native Object Protocol ***/

public:
    virtual std::string nativeObjectRepr(ObjectRef self) override;

public:
    virtual ObjectRef nativeObjectInvoke(ObjectRef self, Reference<TupleObject> args, Reference<MapObject> kwargs) override;

};

/* type object for function */
extern TypeRef FunctionTypeObject;

class FunctionObject : public Object
{
    Reference<CodeObject> _code;
    Reference<TupleObject> _defaults;

private:
    std::string _name;
    std::unordered_map<std::string, Engine::ClosureRef> _closure;

private:
    friend class FunctionType;

public:
    virtual ~FunctionObject() = default;
    explicit FunctionObject(
        const std::string &name,
        Reference<CodeObject> code,
        Reference<TupleObject> defaults,
        std::unordered_map<std::string, Engine::ClosureRef> &closure
    ) : Object(FunctionTypeObject),
        _name(name),
        _code(std::move(code)),
        _closure(std::move(closure)),
        _defaults(std::move(defaults)) { track(); }

public:
    virtual void referenceClear(void) override;
    virtual void referenceTraverse(VisitFunction visit) override;

public:
    std::string &name(void) { return _name; }
    Reference<CodeObject> &code(void) { return _code; }

public:
    ObjectRef invoke(Reference<TupleObject> args, Reference<MapObject> kwargs);

public:
    static void shutdown(void) {}
    static void initialize(void);

};
}

#endif /* REDSCRIPT_RUNTIME_FUNCTIONOBJECT_H */
