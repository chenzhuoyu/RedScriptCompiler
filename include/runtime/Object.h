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

private:
    template <typename> friend class Reference;
    template <typename> friend class _HasComparatorMethods;
    template <typename, bool> friend struct _HasComparatorImpl;
    template <typename, typename, bool> friend struct _ReferenceComparatorImpl;

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
    explicit Object(TypeRef type) : _type(type) { _attrs.emplace("__class__", _type); }

private:
    /* used by `_HasComparator<T>` and `_ReferenceComparator<T, U>` to perform equality test */
    bool isEquals(Object *other);
    bool isNotEquals(Object *other);

private:
    /* used by `Object::Repr` to control infinite recursion in `repr` */
    void exitReprScope(void);
    bool enterReprScope(void);

public:
    Dict &dict(void) { return _dict; }
    Dict &attrs(void) { return _attrs; }

public:
    TypeRef type(void) { return _type; }
    ObjectRef self(void) { return ObjectRef::borrow(this); }

public:
    bool isInstanceOf(TypeRef type) { return _type.isIdenticalWith(type); }
    bool isNotInstanceOf(TypeRef type) { return _type.isNotIdenticalWith(type); }

public:
    /* for object system initialization and destruction, internal use only! */
    static void shutdown(void) {}
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
    TypeRef super(void) { return _super; }
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

private:
    ObjectRef findUserMethod(ObjectRef self, const char *name, const char *alt);
    DescriptorType resolveDescriptor(ObjectRef obj, ObjectRef *getter, ObjectRef *setter, ObjectRef *deleter);

private:
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
    uint64_t    objectHash(ObjectRef self);
    StringList  objectDir (ObjectRef self);
    std::string objectStr (ObjectRef self);
    std::string objectRepr(ObjectRef self);

public:
    bool objectIsTrue(ObjectRef self);
    bool objectIsSubclassOf(ObjectRef self, TypeRef type) { return nativeObjectIsSubclassOf(self, type); }
    bool objectIsInstanceOf(ObjectRef self, TypeRef type) { return nativeObjectIsInstanceOf(self, type); }

public:
    bool      objectHasAttr   (ObjectRef self, const std::string &name) { return nativeObjectHasAttr(self, name); }
    void      objectDelAttr   (ObjectRef self, const std::string &name);
    ObjectRef objectGetAttr   (ObjectRef self, const std::string &name);
    void      objectSetAttr   (ObjectRef self, const std::string &name, ObjectRef value);
    void      objectDefineAttr(ObjectRef self, const std::string &name, ObjectRef value) { nativeObjectDefineAttr(self, name, value); }

public:
    ObjectRef objectNew(TypeRef type, Reference<TupleObject> args, Reference<MapObject> kwargs);
    ObjectRef objectInit(ObjectRef self, Reference<TupleObject> args, Reference<MapObject> kwargs);
    ObjectRef objectInvoke(ObjectRef self, Reference<TupleObject> args, Reference<MapObject> kwargs);

/*** Boolean Protocol ***/

public:
    ObjectRef boolOr (ObjectRef self, ObjectRef other);
    ObjectRef boolAnd(ObjectRef self, ObjectRef other);
    ObjectRef boolNot(ObjectRef self);

/*** Numeric Protocol ***/

public:
    ObjectRef numericPos(ObjectRef self);
    ObjectRef numericNeg(ObjectRef self);

public:
    ObjectRef numericAdd  (ObjectRef self, ObjectRef other);
    ObjectRef numericSub  (ObjectRef self, ObjectRef other);
    ObjectRef numericMul  (ObjectRef self, ObjectRef other);
    ObjectRef numericDiv  (ObjectRef self, ObjectRef other);
    ObjectRef numericMod  (ObjectRef self, ObjectRef other);
    ObjectRef numericPower(ObjectRef self, ObjectRef other);

public:
    ObjectRef numericOr (ObjectRef self, ObjectRef other);
    ObjectRef numericAnd(ObjectRef self, ObjectRef other);
    ObjectRef numericXor(ObjectRef self, ObjectRef other);
    ObjectRef numericNot(ObjectRef self);

public:
    ObjectRef numericLShift(ObjectRef self, ObjectRef other);
    ObjectRef numericRShift(ObjectRef self, ObjectRef other);

public:
    ObjectRef numericIncAdd  (ObjectRef self, ObjectRef other);
    ObjectRef numericIncSub  (ObjectRef self, ObjectRef other);
    ObjectRef numericIncMul  (ObjectRef self, ObjectRef other);
    ObjectRef numericIncDiv  (ObjectRef self, ObjectRef other);
    ObjectRef numericIncMod  (ObjectRef self, ObjectRef other);
    ObjectRef numericIncPower(ObjectRef self, ObjectRef other);

public:
    ObjectRef numericIncOr (ObjectRef self, ObjectRef other);
    ObjectRef numericIncAnd(ObjectRef self, ObjectRef other);
    ObjectRef numericIncXor(ObjectRef self, ObjectRef other);

public:
    ObjectRef numericIncLShift(ObjectRef self, ObjectRef other);
    ObjectRef numericIncRShift(ObjectRef self, ObjectRef other);

/*** Iterator Protocol ***/

public:
    ObjectRef iterableIter(ObjectRef self);
    ObjectRef iterableNext(ObjectRef self);

/*** Sequence Protocol ***/

public:
    ObjectRef sequenceLen    (ObjectRef self);
    void      sequenceDelItem(ObjectRef self, ObjectRef other);
    ObjectRef sequenceGetItem(ObjectRef self, ObjectRef other);
    void      sequenceSetItem(ObjectRef self, ObjectRef second, ObjectRef third);

public:
    void      sequenceDelSlice(ObjectRef self, ObjectRef begin, ObjectRef end, ObjectRef step)                  {        nativeSequenceDelSlice(self, begin, end, step       ); }
    ObjectRef sequenceGetSlice(ObjectRef self, ObjectRef begin, ObjectRef end, ObjectRef step)                  { return nativeSequenceGetSlice(self, begin, end, step       ); }
    void      sequenceSetSlice(ObjectRef self, ObjectRef begin, ObjectRef end, ObjectRef step, ObjectRef value) {        nativeSequenceSetSlice(self, begin, end, step, value); }

/*** Comparator Protocol ***/

public:
    ObjectRef comparableEq(ObjectRef self, ObjectRef other);
    ObjectRef comparableLt(ObjectRef self, ObjectRef other);
    ObjectRef comparableGt(ObjectRef self, ObjectRef other);

public:
    ObjectRef comparableNeq(ObjectRef self, ObjectRef other);
    ObjectRef comparableLeq(ObjectRef self, ObjectRef other);
    ObjectRef comparableGeq(ObjectRef self, ObjectRef other);

public:
    ObjectRef comparableCompare(ObjectRef self, ObjectRef other);
    ObjectRef comparableContains(ObjectRef self, ObjectRef other);

/*** Custom Class Creation Interface ***/

public:
    static TypeRef create(const std::string &name, Reference<MapObject> dict, TypeRef super);

};

class ObjectType : public Type
{
public:
    using Type::Type;

/*** Native Object Protocol ***/

public:
    virtual ObjectRef nativeObjectNew(TypeRef type, Reference<TupleObject> args, Reference<MapObject> kwargs) override;
    virtual ObjectRef nativeObjectInit(ObjectRef self, Reference<TupleObject> args, Reference<MapObject> kwargs) override;
    virtual ObjectRef nativeObjectInvoke(ObjectRef self, Reference<TupleObject> args, Reference<MapObject> kwargs) override;

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
