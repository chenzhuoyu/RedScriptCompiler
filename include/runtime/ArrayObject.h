#ifndef REDSCRIPT_RUNTIME_ARRAYOBJECT_H
#define REDSCRIPT_RUNTIME_ARRAYOBJECT_H

#include <string>
#include <vector>
#include <cstdint>

#include "runtime/Object.h"

namespace RedScript::Runtime
{
class ArrayType : public Type
{
public:
    explicit ArrayType() : Type("array") {}

};

/* type object for arrays */
extern TypeRef ArrayTypeObject;

class ArrayObject : public Object
{
    std::vector<ObjectRef> _items;

public:
    virtual ~ArrayObject() = default;
    explicit ArrayObject() : ArrayObject(0) {}
    explicit ArrayObject(size_t size) : Object(ArrayTypeObject), _items(size) { track(); }

public:
    size_t size(void) const { return _items.size(); }
    std::vector<ObjectRef> &items(void) { return _items; }

public:
    virtual void referenceClear(void) override { _items.clear(); }
    virtual void referenceTraverse(VisitFunction visit) override { for (auto &x : _items) if (x) visit(x); }

public:
    static void shutdown(void) {}
    static void initialize(void);

};
}

#endif /* REDSCRIPT_RUNTIME_ARRAYOBJECT_H */
