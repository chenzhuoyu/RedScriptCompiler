#ifndef REDSCRIPT_RUNTIME_BOUNDMETHODOBJECT_H
#define REDSCRIPT_RUNTIME_BOUNDMETHODOBJECT_H

#include "runtime/Object.h"
#include "runtime/MapObject.h"
#include "runtime/TupleObject.h"

namespace RedScript::Runtime
{
class BoundMethodType : public Type
{
public:
    explicit BoundMethodType() : Type("bound_method") {}

protected:
    virtual void addBuiltins(void) override {}
    virtual void clearBuiltins(void) override {}

/*** Native Object Protocol ***/

public:
    virtual ObjectRef nativeObjectInvoke(ObjectRef self, ObjectRef args, ObjectRef kwargs) override;

};

/* type object for bound method */
extern TypeRef BoundMethodTypeObject;

class BoundMethodObject : public Object
{
    ObjectRef _self;
    ObjectRef _func;

public:
    virtual ~BoundMethodObject() = default;
    explicit BoundMethodObject(ObjectRef self, ObjectRef func) : Object(BoundMethodTypeObject), _self(self), _func(func) {}

public:
    ObjectRef invoke(Reference<TupleObject> args, Reference<MapObject> kwargs);

public:
    static void shutdown(void) {}
    static void initialize(void);

};
}

#endif /* REDSCRIPT_RUNTIME_BOUNDMETHODOBJECT_H */
