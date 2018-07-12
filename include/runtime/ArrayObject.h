#ifndef REDSCRIPT_RUNTIME_ARRAYOBJECT_H
#define REDSCRIPT_RUNTIME_ARRAYOBJECT_H

#include <string>
#include <vector>
#include <cstdint>

#include "utils/Lists.h"
#include "runtime/Object.h"

namespace RedScript::Runtime
{
class ArrayType : public Type
{
public:
    explicit ArrayType() : Type("array") {}

/*** Object Protocol ***/

public:
    virtual uint64_t    objectHash(ObjectRef self) override;
    virtual std::string objectRepr(ObjectRef self) override;

public:
    virtual bool objectIsTrue(ObjectRef self) override;

/*** Numeric Protocol ***/

public:
    virtual ObjectRef numericAdd   (ObjectRef self, ObjectRef other) override;
    virtual ObjectRef numericMul   (ObjectRef self, ObjectRef other) override;
    virtual ObjectRef numericIncAdd(ObjectRef self, ObjectRef other) override;
    virtual ObjectRef numericIncMul(ObjectRef self, ObjectRef other) override;

/*** Iterator Protocol ***/

public:
    virtual ObjectRef iterableIter(ObjectRef self) override;

/*** Sequence Protocol ***/

public:
    virtual ObjectRef sequenceLen    (ObjectRef self) override;
    virtual ObjectRef sequenceGetItem(ObjectRef self, ObjectRef other) override;

/*** Comparator Protocol ***/

public:
    virtual ObjectRef comparableEq(ObjectRef self, ObjectRef other) override;
    virtual ObjectRef comparableLt(ObjectRef self, ObjectRef other) override;
    virtual ObjectRef comparableGt(ObjectRef self, ObjectRef other) override;

public:
    virtual ObjectRef comparableNeq(ObjectRef self, ObjectRef other) override;
    virtual ObjectRef comparableLeq(ObjectRef self, ObjectRef other) override;
    virtual ObjectRef comparableGeq(ObjectRef self, ObjectRef other) override;

public:
    virtual ObjectRef comparableCompare(ObjectRef self, ObjectRef other) override;
    virtual ObjectRef comparableContains(ObjectRef self, ObjectRef other) override;

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
    static Reference<ArrayObject> newEmpty(void) { return fromSize(0); }
    static Reference<ArrayObject> fromSize(size_t size) { return Object::newObject<ArrayObject>(size); }

public:
    template <typename ... Args>
    static Reference<ArrayObject> fromObjects(Args && ... items)
    {
        Reference<ArrayObject> result = fromSize(sizeof ... (Args));
        Utils::Lists::fill(result->_items, std::forward<Args>(items) ...);
        return std::move(result);
    }

public:
    virtual void referenceClear(void) override { _items.clear(); }
    virtual void referenceTraverse(VisitFunction visit) override { for (auto &x : _items) if (x) visit(x); }

public:
    static void shutdown(void) {}
    static void initialize(void);

};
}

#endif /* REDSCRIPT_RUNTIME_ARRAYOBJECT_H */
