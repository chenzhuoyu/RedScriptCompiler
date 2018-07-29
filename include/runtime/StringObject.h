#ifndef REDSCRIPT_RUNTIME_STRINGOBJECT_H
#define REDSCRIPT_RUNTIME_STRINGOBJECT_H

#include <string>
#include <cstdint>

#include "runtime/Object.h"
#include "runtime/ExceptionObject.h"

namespace RedScript::Runtime
{
class StringType : public NativeType
{
public:
    explicit StringType() : NativeType("str") {}

protected:
    virtual void addBuiltins(void) override {}
    virtual void clearBuiltins(void) override {}

/*** Native Object Protocol ***/

public:
    virtual uint64_t    nativeObjectHash(ObjectRef self) override;
    virtual std::string nativeObjectStr (ObjectRef self) override;
    virtual std::string nativeObjectRepr(ObjectRef self) override;

public:
    virtual bool nativeObjectIsTrue(ObjectRef self) override;

/*** Native Numeric Protocol ***/

public:
    virtual ObjectRef nativeNumericAdd   (ObjectRef self, ObjectRef other) override;
    virtual ObjectRef nativeNumericMul   (ObjectRef self, ObjectRef other) override;
    virtual ObjectRef nativeNumericIncAdd(ObjectRef self, ObjectRef other) override { return nativeNumericAdd(self, other); }
    virtual ObjectRef nativeNumericIncMul(ObjectRef self, ObjectRef other) override { return nativeNumericMul(self, other); }

/*** Native Iterator Protocol ***/

public:
    virtual ObjectRef nativeIterableIter(ObjectRef self) override;

/*** Native Sequence Protocol ***/

public:
    virtual ObjectRef nativeSequenceLen     (ObjectRef self) override;
    virtual ObjectRef nativeSequenceGetItem (ObjectRef self, ObjectRef other) override;
    virtual ObjectRef nativeSequenceGetSlice(ObjectRef self, ObjectRef begin, ObjectRef end, ObjectRef step) override;

/*** Native Comparator Protocol ***/

public:
    virtual ObjectRef nativeComparableEq(ObjectRef self, ObjectRef other) override;
    virtual ObjectRef nativeComparableLt(ObjectRef self, ObjectRef other) override;
    virtual ObjectRef nativeComparableGt(ObjectRef self, ObjectRef other) override;

public:
    virtual ObjectRef nativeComparableNeq(ObjectRef self, ObjectRef other) override;
    virtual ObjectRef nativeComparableLeq(ObjectRef self, ObjectRef other) override;
    virtual ObjectRef nativeComparableGeq(ObjectRef self, ObjectRef other) override;

public:
    virtual ObjectRef nativeComparableCompare(ObjectRef self, ObjectRef other) override;
    virtual ObjectRef nativeComparableContains(ObjectRef self, ObjectRef other) override;

};

class StringIteratorType : public NativeType
{
public:
    explicit StringIteratorType() : NativeType("string_iterator") {}

protected:
    virtual void addBuiltins(void) override {}
    virtual void clearBuiltins(void) override {}

/*** Native Iterator Protocol ***/

public:
    virtual ObjectRef nativeIterableNext(ObjectRef self) override;

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
    explicit StringObject(std::string &&value)      : Object(StringTypeObject), _value(std::move(value)) {}
    explicit StringObject(const std::string &value) : Object(StringTypeObject), _value(value) {}

public:
    size_t size() { return _value.size(); }
    const std::string &value(void) { return _value; }

public:
    static ObjectRef newEmpty(void);
    static ObjectRef fromChar(char value);
    static ObjectRef fromString(std::string &&value);
    static ObjectRef fromString(const std::string &value);
    static ObjectRef fromStringInterned(std::string &&value);
    static ObjectRef fromStringInterned(const std::string &value);

public:
    static void shutdown(void);
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
            return StringObject::fromChar(_string->_value[_pos++]);

        /* clear the reference before throwing the exception
         * to prevent cyclic reference to the string object */
        _string = nullptr;
        throw Exceptions::StopIteration();
    }
};
}

#endif /* REDSCRIPT_RUNTIME_STRINGOBJECT_H */
