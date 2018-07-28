#ifndef REDSCRIPT_RUNTIME_OBJECT_H
#define REDSCRIPT_RUNTIME_OBJECT_H

#include <string>
#include <vector>
#include <cstdint>
#include <unordered_map>

#include "runtime/ReferenceCounted.h"

namespace RedScript::Runtime
{
class Type;
class Object;
class MapObject;
class TupleObject;

/* reference names */
typedef Reference<Type> TypeRef;
typedef Reference<Object> ObjectRef;
typedef std::vector<std::string> StringList;
typedef std::unordered_map<std::string, ObjectRef> Dict;

/* the meta class, and the base type object */
extern TypeRef TypeObject;
extern TypeRef ObjectTypeObject;

/* simple object */
class Object : public ReferenceCounted
{
    Dict _dict;
    Dict _attrs;
    TypeRef _type;

public:
    class Repr final
    {
        bool _inScope;
        Object *_object;

    public:
       ~Repr() { if (_inScope) _object->exitReprScope(); }
        Repr(ObjectRef object) : _object(object), _inScope(object->enterReprScope()) {}

    public:
        bool isExists(void) const { return !_inScope; }

    };

public:
    virtual ~Object() = default;
    explicit Object(TypeRef type) : _type(type) {}

public:
    /* used by `_HasComparator<T>` and `_ReferenceComparator<T, U>` to perform equality test */
    bool isTrue(void) __attribute__((always_inline));
    bool isEquals(Object *other) __attribute__((always_inline));
    bool isNotEquals(Object *other) __attribute__((always_inline));

private:
    /* used by `Object::Repr` to control infinite recursion in `repr` */
    void exitReprScope(void);
    bool enterReprScope(void);

public:
    Dict &dict(void) { return _dict; }
    Dict &attrs(void) { return _attrs; }

public:
    TypeRef &type(void) { return _type; }
    ObjectRef self(void) { return ObjectRef::borrow(this); }

public:
    bool isInstanceOf(TypeRef type) { return _type.isIdenticalWith(type); }
    bool isNotInstanceOf(TypeRef type) { return _type.isNotIdenticalWith(type); }

public:
    /* for object system initialization and destruction, internal use only! */
    static void shutdown(void);
    static void initialize(void);

public:
    template <typename T, typename ... Args>
    static inline Reference<T> newObject(Args && ... args)
    {
        /* just a shortcut function */
        return Reference<T>::newObject(std::forward<Args>(args) ...);
    }
};

/* class object */
class Type : public Object
{
    TypeRef _super;
    std::string _name;
    friend class Object;

public:
    explicit Type(const std::string &name) : Type(name, ObjectTypeObject) {}
    explicit Type(const std::string &name, TypeRef super) : Object(TypeObject), _name(name), _super(super) {}

public:
    TypeRef &super(void) { return _super; }
    std::string &name(void) { return _name; }

public:
    virtual void typeShutdown(void);
    virtual void typeInitialize(void);

protected:
    virtual void addBuiltins(void);
    virtual void clearBuiltins(void) {}

private:
    enum class DescriptorType
    {
        Native,
        Unbound,
        UserDefined,
        NotADescriptor,
    };

protected:
    ObjectRef findUserMethod(ObjectRef self, const char *name, const char *alternative);
    DescriptorType resolveDescriptor(ObjectRef obj, ObjectRef *getter, ObjectRef *setter, ObjectRef *deleter);

protected:
    ObjectRef applyUnaryMethod(ObjectRef method, ObjectRef self);
    ObjectRef applyBinaryMethod(ObjectRef method, ObjectRef self, ObjectRef other);
    ObjectRef applyTernaryMethod(ObjectRef method, ObjectRef self, ObjectRef second, ObjectRef third);

/*** Native Object Protocol ***/

public:
    virtual uint64_t    nativeObjectHash(ObjectRef self);
    virtual StringList  nativeObjectDir (ObjectRef self);
    virtual std::string nativeObjectStr (ObjectRef self) { return objectRepr(self); }
    virtual std::string nativeObjectRepr(ObjectRef self);

public:
    virtual bool nativeObjectIsTrue(ObjectRef self) { return true; }
    virtual bool nativeObjectIsSubclassOf(ObjectRef self, TypeRef type);
    virtual bool nativeObjectIsInstanceOf(ObjectRef self, TypeRef type) { return objectIsSubclassOf(self->type(), type); }

public:
    virtual bool      nativeObjectHasAttr   (ObjectRef self, const std::string &name);
    virtual void      nativeObjectDelAttr   (ObjectRef self, const std::string &name);
    virtual ObjectRef nativeObjectGetAttr   (ObjectRef self, const std::string &name);
    virtual void      nativeObjectSetAttr   (ObjectRef self, const std::string &name, ObjectRef value);
    virtual void      nativeObjectDefineAttr(ObjectRef self, const std::string &name, ObjectRef value);

public:
    virtual ObjectRef nativeObjectNew(TypeRef type, Reference<TupleObject> args, Reference<MapObject> kwargs);
    virtual ObjectRef nativeObjectInit(ObjectRef self, Reference<TupleObject> args, Reference<MapObject> kwargs);
    virtual ObjectRef nativeObjectInvoke(ObjectRef self, Reference<TupleObject> args, Reference<MapObject> kwargs);

/*** Native Boolean Protocol ***/

public:
    virtual ObjectRef nativeBoolOr (ObjectRef self, ObjectRef other);
    virtual ObjectRef nativeBoolAnd(ObjectRef self, ObjectRef other);
    virtual ObjectRef nativeBoolNot(ObjectRef self);

/*** Native Numeric Protocol ***/

public:
    virtual ObjectRef nativeNumericPos(ObjectRef self);
    virtual ObjectRef nativeNumericNeg(ObjectRef self);

public:
    virtual ObjectRef nativeNumericAdd  (ObjectRef self, ObjectRef other);
    virtual ObjectRef nativeNumericSub  (ObjectRef self, ObjectRef other);
    virtual ObjectRef nativeNumericMul  (ObjectRef self, ObjectRef other);
    virtual ObjectRef nativeNumericDiv  (ObjectRef self, ObjectRef other);
    virtual ObjectRef nativeNumericMod  (ObjectRef self, ObjectRef other);
    virtual ObjectRef nativeNumericPower(ObjectRef self, ObjectRef other);

public:
    virtual ObjectRef nativeNumericOr (ObjectRef self, ObjectRef other);
    virtual ObjectRef nativeNumericAnd(ObjectRef self, ObjectRef other);
    virtual ObjectRef nativeNumericXor(ObjectRef self, ObjectRef other);
    virtual ObjectRef nativeNumericNot(ObjectRef self);

public:
    virtual ObjectRef nativeNumericLShift(ObjectRef self, ObjectRef other);
    virtual ObjectRef nativeNumericRShift(ObjectRef self, ObjectRef other);

public:
    virtual ObjectRef nativeNumericIncAdd  (ObjectRef self, ObjectRef other);
    virtual ObjectRef nativeNumericIncSub  (ObjectRef self, ObjectRef other);
    virtual ObjectRef nativeNumericIncMul  (ObjectRef self, ObjectRef other);
    virtual ObjectRef nativeNumericIncDiv  (ObjectRef self, ObjectRef other);
    virtual ObjectRef nativeNumericIncMod  (ObjectRef self, ObjectRef other);
    virtual ObjectRef nativeNumericIncPower(ObjectRef self, ObjectRef other);

public:
    virtual ObjectRef nativeNumericIncOr (ObjectRef self, ObjectRef other);
    virtual ObjectRef nativeNumericIncAnd(ObjectRef self, ObjectRef other);
    virtual ObjectRef nativeNumericIncXor(ObjectRef self, ObjectRef other);

public:
    virtual ObjectRef nativeNumericIncLShift(ObjectRef self, ObjectRef other);
    virtual ObjectRef nativeNumericIncRShift(ObjectRef self, ObjectRef other);

/*** Native Iterator Protocol ***/

public:
    virtual ObjectRef nativeIterableIter(ObjectRef self);
    virtual ObjectRef nativeIterableNext(ObjectRef self);

/*** Native Sequence Protocol ***/

public:
    virtual ObjectRef nativeSequenceLen    (ObjectRef self);
    virtual void      nativeSequenceDelItem(ObjectRef self, ObjectRef other);
    virtual ObjectRef nativeSequenceGetItem(ObjectRef self, ObjectRef other);
    virtual void      nativeSequenceSetItem(ObjectRef self, ObjectRef second, ObjectRef third);

public:
    virtual void      nativeSequenceDelSlice(ObjectRef self, ObjectRef begin, ObjectRef end, ObjectRef step);
    virtual ObjectRef nativeSequenceGetSlice(ObjectRef self, ObjectRef begin, ObjectRef end, ObjectRef step);
    virtual void      nativeSequenceSetSlice(ObjectRef self, ObjectRef begin, ObjectRef end, ObjectRef step, ObjectRef value);

/*** Native Comparator Protocol ***/

public:
    virtual ObjectRef nativeComparableEq(ObjectRef self, ObjectRef other);
    virtual ObjectRef nativeComparableLt(ObjectRef self, ObjectRef other);
    virtual ObjectRef nativeComparableGt(ObjectRef self, ObjectRef other);

public:
    virtual ObjectRef nativeComparableNeq(ObjectRef self, ObjectRef other);
    virtual ObjectRef nativeComparableLeq(ObjectRef self, ObjectRef other);
    virtual ObjectRef nativeComparableGeq(ObjectRef self, ObjectRef other);

public:
    virtual ObjectRef nativeComparableCompare(ObjectRef self, ObjectRef other);
    virtual ObjectRef nativeComparableContains(ObjectRef self, ObjectRef other);

/*** Object Protocol ***/

public:
    virtual uint64_t    objectHash(ObjectRef self) { return nativeObjectHash(self); }
    virtual StringList  objectDir (ObjectRef self) { return nativeObjectDir (self); }
    virtual std::string objectStr (ObjectRef self) { return nativeObjectStr (self); }
    virtual std::string objectRepr(ObjectRef self) { return nativeObjectRepr(self); }

public:
    virtual bool objectIsTrue(ObjectRef self) { return nativeObjectIsTrue(self); }
    virtual bool objectIsSubclassOf(ObjectRef self, TypeRef type) { return nativeObjectIsSubclassOf(self, type); }
    virtual bool objectIsInstanceOf(ObjectRef self, TypeRef type) { return nativeObjectIsInstanceOf(self, type); }

public:
    virtual bool      objectHasAttr   (ObjectRef self, const std::string &name)                  { return nativeObjectHasAttr   (self, name);        }
    virtual void      objectDelAttr   (ObjectRef self, const std::string &name)                  {        nativeObjectDelAttr   (self, name);        }
    virtual ObjectRef objectGetAttr   (ObjectRef self, const std::string &name)                  { return nativeObjectGetAttr   (self, name);        }
    virtual void      objectSetAttr   (ObjectRef self, const std::string &name, ObjectRef value) {        nativeObjectSetAttr   (self, name, value); }
    virtual void      objectDefineAttr(ObjectRef self, const std::string &name, ObjectRef value) {        nativeObjectDefineAttr(self, name, value); }

public:
    virtual ObjectRef objectNew   (TypeRef   type, Reference<TupleObject> args, Reference<MapObject> kwargs);
    virtual ObjectRef objectInit  (ObjectRef self, Reference<TupleObject> args, Reference<MapObject> kwargs);
    virtual ObjectRef objectInvoke(ObjectRef self, Reference<TupleObject> args, Reference<MapObject> kwargs);

/*** Boolean Protocol ***/

public:
    virtual ObjectRef boolOr (ObjectRef self, ObjectRef other) { return nativeBoolOr (self, other); }
    virtual ObjectRef boolAnd(ObjectRef self, ObjectRef other) { return nativeBoolAnd(self, other); }
    virtual ObjectRef boolNot(ObjectRef self)                  { return nativeBoolNot(self);        }

/*** Numeric Protocol ***/

public:
    virtual ObjectRef numericPos(ObjectRef self)                        { return nativeNumericPos      (self);        }
    virtual ObjectRef numericNeg(ObjectRef self)                        { return nativeNumericNeg      (self);        }

public:
    virtual ObjectRef numericAdd  (ObjectRef self, ObjectRef other)     { return nativeNumericAdd      (self, other); }
    virtual ObjectRef numericSub  (ObjectRef self, ObjectRef other)     { return nativeNumericSub      (self, other); }
    virtual ObjectRef numericMul  (ObjectRef self, ObjectRef other)     { return nativeNumericMul      (self, other); }
    virtual ObjectRef numericDiv  (ObjectRef self, ObjectRef other)     { return nativeNumericDiv      (self, other); }
    virtual ObjectRef numericMod  (ObjectRef self, ObjectRef other)     { return nativeNumericMod      (self, other); }
    virtual ObjectRef numericPower(ObjectRef self, ObjectRef other)     { return nativeNumericPower    (self, other); }

public:
    virtual ObjectRef numericOr (ObjectRef self, ObjectRef other)       { return nativeNumericOr       (self, other); }
    virtual ObjectRef numericAnd(ObjectRef self, ObjectRef other)       { return nativeNumericAnd      (self, other); }
    virtual ObjectRef numericXor(ObjectRef self, ObjectRef other)       { return nativeNumericXor      (self, other); }
    virtual ObjectRef numericNot(ObjectRef self)                        { return nativeNumericNot      (self);        }

public:
    virtual ObjectRef numericLShift(ObjectRef self, ObjectRef other)    { return nativeNumericLShift   (self, other); }
    virtual ObjectRef numericRShift(ObjectRef self, ObjectRef other)    { return nativeNumericRShift   (self, other); }

public:
    virtual ObjectRef numericIncAdd  (ObjectRef self, ObjectRef other)  { return nativeNumericIncAdd   (self, other); }
    virtual ObjectRef numericIncSub  (ObjectRef self, ObjectRef other)  { return nativeNumericIncSub   (self, other); }
    virtual ObjectRef numericIncMul  (ObjectRef self, ObjectRef other)  { return nativeNumericIncMul   (self, other); }
    virtual ObjectRef numericIncDiv  (ObjectRef self, ObjectRef other)  { return nativeNumericIncDiv   (self, other); }
    virtual ObjectRef numericIncMod  (ObjectRef self, ObjectRef other)  { return nativeNumericIncMod   (self, other); }
    virtual ObjectRef numericIncPower(ObjectRef self, ObjectRef other)  { return nativeNumericIncPower (self, other); }

public:
    virtual ObjectRef numericIncOr (ObjectRef self, ObjectRef other)    { return nativeNumericIncOr    (self, other); }
    virtual ObjectRef numericIncAnd(ObjectRef self, ObjectRef other)    { return nativeNumericIncAnd   (self, other); }
    virtual ObjectRef numericIncXor(ObjectRef self, ObjectRef other)    { return nativeNumericIncXor   (self, other); }

public:
    virtual ObjectRef numericIncLShift(ObjectRef self, ObjectRef other) { return nativeNumericIncLShift(self, other); }
    virtual ObjectRef numericIncRShift(ObjectRef self, ObjectRef other) { return nativeNumericIncRShift(self, other); }

/*** Iterator Protocol ***/

public:
    virtual ObjectRef iterableIter(ObjectRef self) { return nativeIterableIter(self); }
    virtual ObjectRef iterableNext(ObjectRef self) { return nativeIterableNext(self); }

/*** Sequence Protocol ***/

public:
    virtual ObjectRef sequenceLen    (ObjectRef self)                                    { return nativeSequenceLen    (self);                }
    virtual void      sequenceDelItem(ObjectRef self, ObjectRef other)                   {        nativeSequenceDelItem(self, other);         }
    virtual ObjectRef sequenceGetItem(ObjectRef self, ObjectRef other)                   { return nativeSequenceGetItem(self, other);         }
    virtual void      sequenceSetItem(ObjectRef self, ObjectRef second, ObjectRef third) {        nativeSequenceSetItem(self, second, third); }

public:
    virtual void      sequenceDelSlice(ObjectRef self, ObjectRef begin, ObjectRef end, ObjectRef step)                  {        nativeSequenceDelSlice(self, begin, end, step       ); }
    virtual ObjectRef sequenceGetSlice(ObjectRef self, ObjectRef begin, ObjectRef end, ObjectRef step)                  { return nativeSequenceGetSlice(self, begin, end, step       ); }
    virtual void      sequenceSetSlice(ObjectRef self, ObjectRef begin, ObjectRef end, ObjectRef step, ObjectRef value) {        nativeSequenceSetSlice(self, begin, end, step, value); }

/*** Comparator Protocol ***/

public:
    virtual ObjectRef comparableEq(ObjectRef self, ObjectRef other)       { return nativeComparableEq(self, other); }
    virtual ObjectRef comparableLt(ObjectRef self, ObjectRef other)       { return nativeComparableLt(self, other); }
    virtual ObjectRef comparableGt(ObjectRef self, ObjectRef other)       { return nativeComparableGt(self, other); }

public:
    virtual ObjectRef comparableNeq(ObjectRef self, ObjectRef other)      { return nativeComparableNeq(self, other); }
    virtual ObjectRef comparableLeq(ObjectRef self, ObjectRef other)      { return nativeComparableLeq(self, other); }
    virtual ObjectRef comparableGeq(ObjectRef self, ObjectRef other)      { return nativeComparableGeq(self, other); }

public:
    virtual ObjectRef comparableCompare (ObjectRef self, ObjectRef other) { return nativeComparableCompare (self, other); }
    virtual ObjectRef comparableContains(ObjectRef self, ObjectRef other) { return nativeComparableContains(self, other); }

/*** Custom Class Creation Interface ***/

public:
    static TypeRef create(const std::string &name, Reference<MapObject> dict, TypeRef super);

};

/* base class for built-in native types */
class NativeType : public Type
{
public:
    using Type::Type;

/*** Native Object Protocol ***/

public:
    virtual ObjectRef nativeObjectNew(TypeRef type, Reference<TupleObject> args, Reference<MapObject> kwargs) override;
    virtual ObjectRef nativeObjectInit(ObjectRef self, Reference<TupleObject> args, Reference<MapObject> kwargs) override;
    virtual ObjectRef nativeObjectInvoke(ObjectRef self, Reference<TupleObject> args, Reference<MapObject> kwargs) override;

};

/* base class for user-defined types */
class ObjectType : public NativeType
{
public:
    using NativeType::NativeType;

/*** Object Protocol ***/

public:
    virtual uint64_t    objectHash(ObjectRef self) override;
    virtual StringList  objectDir (ObjectRef self) override;
    virtual std::string objectStr (ObjectRef self) override;
    virtual std::string objectRepr(ObjectRef self) override;

public:
    virtual bool objectIsTrue(ObjectRef self) override;
    virtual bool objectIsSubclassOf(ObjectRef self, TypeRef type) override;
    virtual bool objectIsInstanceOf(ObjectRef self, TypeRef type) override;

public:
    virtual void      objectDelAttr   (ObjectRef self, const std::string &name) override;
    virtual ObjectRef objectGetAttr   (ObjectRef self, const std::string &name) override;
    virtual void      objectSetAttr   (ObjectRef self, const std::string &name, ObjectRef value) override;

public:
    virtual ObjectRef objectNew(TypeRef type, Reference<TupleObject> args, Reference<MapObject> kwargs) override;
    virtual ObjectRef objectInit(ObjectRef self, Reference<TupleObject> args, Reference<MapObject> kwargs) override;
    virtual ObjectRef objectInvoke(ObjectRef self, Reference<TupleObject> args, Reference<MapObject> kwargs) override;

/*** Boolean Protocol ***/

public:
    virtual ObjectRef boolOr (ObjectRef self, ObjectRef other) override;
    virtual ObjectRef boolAnd(ObjectRef self, ObjectRef other) override;
    virtual ObjectRef boolNot(ObjectRef self) override;

/*** Numeric Protocol ***/

public:
    virtual ObjectRef numericPos(ObjectRef self) override;
    virtual ObjectRef numericNeg(ObjectRef self) override;

public:
    virtual ObjectRef numericAdd  (ObjectRef self, ObjectRef other) override;
    virtual ObjectRef numericSub  (ObjectRef self, ObjectRef other) override;
    virtual ObjectRef numericMul  (ObjectRef self, ObjectRef other) override;
    virtual ObjectRef numericDiv  (ObjectRef self, ObjectRef other) override;
    virtual ObjectRef numericMod  (ObjectRef self, ObjectRef other) override;
    virtual ObjectRef numericPower(ObjectRef self, ObjectRef other) override;

public:
    virtual ObjectRef numericOr (ObjectRef self, ObjectRef other) override;
    virtual ObjectRef numericAnd(ObjectRef self, ObjectRef other) override;
    virtual ObjectRef numericXor(ObjectRef self, ObjectRef other) override;
    virtual ObjectRef numericNot(ObjectRef self) override;

public:
    virtual ObjectRef numericLShift(ObjectRef self, ObjectRef other) override;
    virtual ObjectRef numericRShift(ObjectRef self, ObjectRef other) override;

public:
    virtual ObjectRef numericIncAdd  (ObjectRef self, ObjectRef other) override;
    virtual ObjectRef numericIncSub  (ObjectRef self, ObjectRef other) override;
    virtual ObjectRef numericIncMul  (ObjectRef self, ObjectRef other) override;
    virtual ObjectRef numericIncDiv  (ObjectRef self, ObjectRef other) override;
    virtual ObjectRef numericIncMod  (ObjectRef self, ObjectRef other) override;
    virtual ObjectRef numericIncPower(ObjectRef self, ObjectRef other) override;

public:
    virtual ObjectRef numericIncOr (ObjectRef self, ObjectRef other) override;
    virtual ObjectRef numericIncAnd(ObjectRef self, ObjectRef other) override;
    virtual ObjectRef numericIncXor(ObjectRef self, ObjectRef other) override;

public:
    virtual ObjectRef numericIncLShift(ObjectRef self, ObjectRef other) override;
    virtual ObjectRef numericIncRShift(ObjectRef self, ObjectRef other) override;

/*** Iterator Protocol ***/

public:
    virtual ObjectRef iterableIter(ObjectRef self) override;
    virtual ObjectRef iterableNext(ObjectRef self) override;

/*** Sequence Protocol ***/

public:
    virtual ObjectRef sequenceLen    (ObjectRef self) override;
    virtual void      sequenceDelItem(ObjectRef self, ObjectRef other) override;
    virtual ObjectRef sequenceGetItem(ObjectRef self, ObjectRef other) override;
    virtual void      sequenceSetItem(ObjectRef self, ObjectRef second, ObjectRef third) override;

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
}

/* hash function for STL
 * required by unordered data structures */
namespace std
{
template <>
struct hash<RedScript::Runtime::ObjectRef>
{
    size_t operator()(RedScript::Runtime::ObjectRef other) const
    {
        /* use object system hash function */
        static std::hash<uint64_t> hash;
        return hash(other->type()->objectHash(other));
    }
};
}

#endif /* REDSCRIPT_RUNTIME_OBJECT_H */
