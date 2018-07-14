#ifndef REDSCRIPT_RUNTIME_STRINGOBJECT_H
#define REDSCRIPT_RUNTIME_STRINGOBJECT_H

#include <string>
#include <cstdint>

#include "runtime/Object.h"
#include "exceptions/StopIteration.h"

namespace RedScript::Runtime
{
class StringType : public Type
{
public:
    explicit StringType() : Type("str") {}

/*** Object Protocol ***/

public:
    virtual uint64_t    objectHash(ObjectRef self) override;
    virtual std::string objectStr (ObjectRef self) override;
    virtual std::string objectRepr(ObjectRef self) override;

public:
    virtual bool objectIsTrue(ObjectRef self) override;

/*** Numeric Protocol ***/

public:
    virtual ObjectRef numericAdd   (ObjectRef self, ObjectRef other) override;
    virtual ObjectRef numericMul   (ObjectRef self, ObjectRef other) override;
    virtual ObjectRef numericIncAdd(ObjectRef self, ObjectRef other) override { return numericAdd(self, other); }
    virtual ObjectRef numericIncMul(ObjectRef self, ObjectRef other) override { return numericMul(self, other); }

/*** Iterator Protocol ***/

public:
    virtual ObjectRef iterableIter(ObjectRef self) override;

/*** Sequence Protocol ***/

public:
    virtual ObjectRef sequenceLen     (ObjectRef self) override;
    virtual ObjectRef sequenceGetItem (ObjectRef self, ObjectRef other) override;
    virtual ObjectRef sequenceGetSlice(ObjectRef self, ObjectRef begin, ObjectRef end, ObjectRef step) override;

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

class StringIteratorType : public Type
{
public:
    explicit StringIteratorType() : Type("string_iterator") {}

/*** Iterator Protocol ***/

public:
    virtual ObjectRef iterableNext(ObjectRef self) override;

};

/* type object for string and string iterator */
extern TypeRef StringTypeObject;
extern TypeRef StringIteratorTypeObject;

class StringObject : public Object
{
    std::string _value;
    friend class StringType;
    friend class StringIteratorObject;

public:
    virtual ~StringObject() = default;
    explicit StringObject(const std::string &value) : Object(StringTypeObject), _value(value) {}

public:
    size_t size() { return _value.size(); }
    const std::string &value(void) { return _value; }

public:
    static ObjectRef fromString(const std::string &value);

public:
    static void shutdown(void) {}
    static void initialize(void);

};

class StringIteratorObject : public Object
{
    size_t _pos;
    Reference<StringObject> _string;

public:
    virtual ~StringIteratorObject() = default;
    explicit StringIteratorObject(Reference<StringObject> string) : Object(StringIteratorTypeObject), _pos(0), _string(string) {}

public:
    ObjectRef next(void)
    {
        /* check for string */
        if (_string.isNull())
            throw Exceptions::StopIteration();

        /* check for iterator position */
        if (_pos < _string->_value.size())
            return StringObject::fromString(std::string(1, _string->_value[_pos++]));

        /* clear the reference before throwing the exception
         * to prevent cyclic reference to the string object */
        _string = nullptr;
        throw Exceptions::StopIteration();
    }
};
}

#endif /* REDSCRIPT_RUNTIME_STRINGOBJECT_H */
