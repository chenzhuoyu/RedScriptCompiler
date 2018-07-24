#include <algorithm>
#include <unordered_set>

#include "runtime/Object.h"
#include "runtime/IntObject.h"
#include "runtime/MapObject.h"
#include "runtime/BoolObject.h"
#include "runtime/NullObject.h"
#include "runtime/ProxyObject.h"
#include "runtime/SliceObject.h"
#include "runtime/TupleObject.h"
#include "runtime/StringObject.h"
#include "runtime/FunctionObject.h"
#include "runtime/UnboundMethodObject.h"

#include "utils/Strings.h"
#include "exceptions/TypeError.h"
#include "exceptions/InternalError.h"
#include "exceptions/AttributeError.h"

namespace RedScript::Runtime
{
/* the meta class, and the base type object */
TypeRef TypeObject;
TypeRef ObjectTypeObject;

/* per-thread scope object to control infinite recursion in repr */
static thread_local std::unordered_set<Object *> _reprScope;

/*** Object Implementations ***/

bool Object::isEquals(Object *other)
{
    /* same object, absolutely equals */
    if (this == other)
        return true;

    /* delegate the comparison to type objects */
    ObjectRef ref = other->self();
    ObjectRef result = _type->comparableEq(self(), ref);

    /* check it's truth value */
    return result->type()->objectIsTrue(result);
}

bool Object::isNotEquals(Object *other)
{
    /* same object, absolutely equals, so return false */
    if (this == other)
        return false;

    /* delegate the comparison to type objects */
    ObjectRef ref = other->self();
    ObjectRef result = _type->comparableNeq(self(), ref);

    /* check it's truth value */
    return result->type()->objectIsTrue(result);
}

void Object::exitReprScope(void)
{
    /* remove from repr scope */
    if (!(_reprScope.erase(this)))
        throw std::logic_error("object not found in repr scope");
}

bool Object::enterReprScope(void)
{
    /* already exists */
    if (_reprScope.find(this) != _reprScope.end())
        return false;

    /* add to object list */
    _reprScope.emplace(this);
    return true;
}

void Object::initialize(void)
{
    /* the meta class */
    static Type metaClass("type", nullptr);
    metaClass._type = TypeObject = TypeRef::refStatic(metaClass);

    /* the base object type */
    static ObjectType objectType("object", nullptr);
    metaClass._super = ObjectTypeObject = TypeRef::refStatic(objectType);
}

/*** Type Implementations ***/

void Type::typeShutdown(void)
{
    clearBuiltins();
    dict().clear();
    attrs().clear();
}

void Type::typeInitialize(void)
{
    addBuiltins();
    attrs().emplace("__name__", StringObject::fromString(_name));
    attrs().emplace("__super__", _super.isNull() ? TypeObject : _super);
}

void Type::addBuiltins(void)
{
    /* basic object protocol attributes */
    attrs().emplace("__repr__", UnboundMethodObject::fromFunction([](ObjectRef self){ return self->type()->nativeObjectRepr(self); }));
    attrs().emplace("__hash__", UnboundMethodObject::fromFunction([](ObjectRef self){ return self->type()->nativeObjectHash(self); }));

    /* built-in "__delattr__" function */
    attrs().emplace(
        "__delattr__",
        UnboundMethodObject::fromFunction([](Runtime::ObjectRef self, const std::string &name)
        {
            /* invoke the object protocol */
            self->type()->nativeObjectDelAttr(self, name);
        })
    );

    /* built-in "__getattr__" function */
    attrs().emplace(
        "__getattr__",
        UnboundMethodObject::fromFunction([](Runtime::ObjectRef self, const std::string &name)
        {
            /* invoke the object protocol */
            return self->type()->nativeObjectGetAttr(self, name);
        })
    );

    /* built-in "__setattr__" function */
    attrs().emplace(
        "__setattr__",
        UnboundMethodObject::fromFunction([](Runtime::ObjectRef self, const std::string &name, Runtime::ObjectRef value)
        {
            /* invoke the object protocol */
            self->type()->nativeObjectSetAttr(self, name, value);
        })
    );
}

ObjectRef Type::findUserMethod(ObjectRef self, const char *name, const char *alt)
{
    /* get it's dict */
    auto type = self->type();
    auto iter = type->dict().find(name);

    /* find the alternate name as needed */
    if (alt && (iter == type->dict().end()))
        iter = type->dict().find(alt);

    /* check for existence */
    if (iter == type->dict().end())
        return nullptr;
    else
        return iter->second;
}

Type::DescriptorType Type::resolveDescriptor(ObjectRef obj, ObjectRef *getter, ObjectRef *setter, ObjectRef *deleter)
{
    /* it's an unbound method object */
    if (obj->isInstanceOf(UnboundMethodTypeObject))
        return DescriptorType::Unbound;

    /* it's a proxy object */
    if (obj->isInstanceOf(ProxyTypeObject))
    {
        /* convert to proxy */
        auto desc = obj.as<ProxyObject>();

        /* get those modifiers */
        if (getter ) *getter = desc->getter();
        if (setter ) *setter = desc->setter();
        if (deleter) *deleter = desc->deleter();

        /* it's a native proxy descriptor */
        return DescriptorType::Native;
    }

    /* not a proxy, check for proxy-like descriptor */
    bool ret = false;
    auto type = obj->type();
    auto giter = type->dict().find("__get__");
    auto siter = type->dict().find("__set__");
    auto diter = type->dict().find("__delete__");

    /* read those modifiers */
    if (giter != type->dict().end()) { ret = true; if (getter ) *getter  = giter->second; }
    if (siter != type->dict().end()) { ret = true; if (setter ) *setter  = siter->second; }
    if (diter != type->dict().end()) { ret = true; if (deleter) *deleter = diter->second; }

    /* either one is not null, it's an user-defined object descriptor */
    return ret ? DescriptorType::UserDefined : DescriptorType::NotADescriptor;
}

ObjectRef Type::applyUnaryMethod(ObjectRef method, ObjectRef self)
{
    auto args = TupleObject::fromObjects(self);
    return method->type()->objectInvoke(method, args, MapObject::newOrdered());
}

ObjectRef Type::applyBinaryMethod(ObjectRef method, ObjectRef self, ObjectRef other)
{
    auto args = TupleObject::fromObjects(self, other);
    return method->type()->objectInvoke(method, args, MapObject::newOrdered());
}

ObjectRef Type::applyTernaryMethod(ObjectRef method, ObjectRef self, ObjectRef second, ObjectRef third)
{
    auto args = TupleObject::fromObjects(self, second, third);
    return method->type()->objectInvoke(method, args, MapObject::newOrdered());
}

/***** Object System Native Interface *****/

#define NOT_IMPL(self, func) {                                  \
    throw Exceptions::AttributeError(Utils::Strings::format(    \
        "\"%s\" object doesn't support \"" #func "\" action",   \
        self->type()->name()                                    \
    ));                                                         \
}

/*** Native Object Protocol ***/

uint64_t Type::nativeObjectHash(ObjectRef self)
{
    /* default: hash the object pointer */
    std::hash<uintptr_t> hash;
    return hash(reinterpret_cast<uintptr_t>(self.get()));
}

StringList Type::nativeObjectDir(ObjectRef self)
{
    /* result name list */
    StringList result;

    /* list every key in object dict */
    for (const auto &x : self->dict())
        result.emplace_back(x.first);

    /* and built-in attributes */
    for (const auto &x : self->attrs())
        result.emplace_back(x.first);

    /* type object, also add all attributes from super types */
    if (self->isInstanceOf(TypeObject))
    {
        /* enumerate all it's super types */
        for (TypeRef ref = self.as<Type>()->_super; ref; ref = ref->_super)
        {
            /* list every key in object dict */
            for (const auto &x : ref->dict())
                result.emplace_back(x.first);

            /* and built-in attributes */
            for (const auto &x : ref->attrs())
                result.emplace_back(x.first);
        }
    }

    /* sort the names, and remove duplications */
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return std::move(result);
}

std::string Type::nativeObjectRepr(ObjectRef self)
{
    /* basic object representation */
    if (self->isNotInstanceOf(TypeObject))
        return Utils::Strings::format("<%s object at %p>", _name, static_cast<void *>(self.get()));

    /* type object representation */
    auto type = self.as<Type>();
    return Utils::Strings::format("<type \"%s\" at %p>", type->name(), static_cast<void *>(type.get()));
}

bool Type::nativeObjectIsSubclassOf(ObjectRef self, TypeRef type)
{
    /* not a type at all */
    if (self->isNotInstanceOf(TypeObject))
        return false;

    /* search for all it's super types */
    for (TypeRef t = self.as<Type>(); t; t = t->super())
        if (t.isIdenticalWith(type))
            return true;

    /* no super types match */
    return false;
}

bool Type::nativeObjectHasAttr(ObjectRef self, const std::string &name)
{
    try
    {
        /* try getting the attributes from object */
        objectGetAttr(self, name);
        return true;
    }
    catch (const Exceptions::AttributeError &)
    {
        /* attribute not found */
        return false;
    }
}

void Type::nativeObjectDelAttr(ObjectRef self, const std::string &name)
{
    /* find the attribute in dict */
    TypeRef type = self->type();
    decltype(self->dict().find(name)) iter = self->dict().find(name);

    /* found in instance dict, erase directly */
    if (iter != self->dict().end())
    {
        self->dict().erase(iter);
        return;
    }

    /* built-in attributes */
    if (self->attrs().find(name) != self->attrs().end())
        throw Exceptions::AttributeError(Utils::Strings::format("Cannot delete built-in attribute \"%s\" from \"%s\"", name, _name));

    /* search this type and all of it's super types */
    while (type.isNotNull() &&
           ((iter = type->dict().find(name)) == type->dict().end()) &&
           ((iter = type->attrs().find(name)) == type->attrs().end()))
        type = type->_super;

    /* attribute not found */
    if (type.isNull())
        throw Exceptions::AttributeError(Utils::Strings::format("\"%s\" object has no attribute \"%s\"", _name, name));

    /* check for null */
    if (iter->second.isNull())
        throw Exceptions::AttributeError(Utils::Strings::format("Attribute \"%s\" of \"%s\" is not defined yet", name, _name));

    /* descriptor properties */
    ObjectRef deleter;
    Reference<TupleObject> args;

    /* check descriptor type */
    switch (resolveDescriptor(iter->second, nullptr, nullptr, &deleter))
    {
        /* native descriptor */
        case DescriptorType::Native:
        {
            args = TupleObject::fromObjects(self, this->self());
            break;
        }

        /* user-defined proxy-like object descriptor */
        case DescriptorType::UserDefined:
        {
            args = TupleObject::fromObjects(iter->second, self, this->self());
            break;
        }

        /* plain-old object or unbound method */
        case DescriptorType::Unbound:
        case DescriptorType::NotADescriptor:
            throw Exceptions::AttributeError(Utils::Strings::format("Cannot delete attribute \"%s\" from type \"%s\"", name, _name));
    }

    /* check for getdeleterter */
    if (deleter.isNull())
        throw Exceptions::AttributeError(Utils::Strings::format("Attribute \"%s\" of \"%s\" is not deletable", name, _name));

    /* invoke the deleter */
    auto kwargs = MapObject::newOrdered();
    auto result = deleter->type()->objectInvoke(deleter, args, kwargs);

    /* check for null */
    if (result.isNull())
        throw Exceptions::InternalError(Utils::Strings::format("Descriptor \"%s\" of \"%s\" gives null", name, _name));
}

ObjectRef Type::nativeObjectGetAttr(ObjectRef self, const std::string &name)
{
    /* attribute dictionary iterator */
    TypeRef type = self->type();
    decltype(self->dict().find(name)) iter;

    /* attribute exists */
    if (((iter = self->dict().find(name)) != self->dict().end()) ||
        ((iter = self->attrs().find(name)) != self->attrs().end()))
    {
        /* check for null, and read it's value directly */
        if (iter->second.isNull())
            throw Exceptions::AttributeError(Utils::Strings::format("Attribute \"%s\" of \"%s\" is not defined yet", name, _name));
        else
            return iter->second;
    }

    /* type object, also resolve from super types */
    if (self->isInstanceOf(TypeObject))
    {
        /* search for all it's super types */
        for (TypeRef ref = self.as<Type>()->_super; ref; ref = ref->_super)
        {
            /* attribute exists */
            if (((iter = ref->dict().find(name)) != ref->dict().end()) ||
                ((iter = ref->attrs().find(name)) != ref->attrs().end()))
            {
                /* check for null, and read it's value directly */
                if (iter->second.isNull())
                    throw Exceptions::AttributeError(Utils::Strings::format("Attribute \"%s\" of \"%s\" is not defined yet", name, _name));
                else
                    return iter->second;
            }
        }
    }

    /* search this type and all of it's super types */
    while (type.isNotNull() &&
           ((iter = type->dict().find(name)) == type->dict().end()) &&
           ((iter = type->attrs().find(name)) == type->attrs().end()))
        type = type->_super;

    /* attribute not found */
    if (type.isNull())
        throw Exceptions::AttributeError(Utils::Strings::format("\"%s\" object has no attribute \"%s\"", _name, name));

    /* check for null */
    if (iter->second.isNull())
        throw Exceptions::AttributeError(Utils::Strings::format("Attribute \"%s\" of \"%s\" is not defined yet", name, _name));

    /* descriptor properties */
    ObjectRef getter;
    Reference<TupleObject> args;

    /* check descriptor type */
    switch (resolveDescriptor(iter->second, &getter, nullptr, nullptr))
    {
        /* native descriptor */
        case DescriptorType::Native:
        {
            args = TupleObject::fromObjects(self, this->self());
            break;
        }

        /* unbound method */
        case DescriptorType::Unbound:
            return iter->second.as<UnboundMethodObject>()->bind(self);

        /* user-defined proxy-like object descriptor */
        case DescriptorType::UserDefined:
        {
            args = TupleObject::fromObjects(iter->second, self, this->self());
            break;
        }

        /* plain-old object */
        case DescriptorType::NotADescriptor:
            return iter->second;
    }

    /* check for getter */
    if (getter.isNull())
        throw Exceptions::AttributeError(Utils::Strings::format("Attribute \"%s\" of \"%s\" is not readable", name, _name));

    /* invoke the getter */
    auto kwargs = MapObject::newOrdered();
    auto result = getter->type()->objectInvoke(getter, args, kwargs);

    /* check for null */
    if (result.isNull())
        throw Exceptions::InternalError(Utils::Strings::format("Descriptor \"%s\" of \"%s\" gives null", name, _name));

    /* move to prevent copy */
    return std::move(result);
}

void Type::nativeObjectSetAttr(ObjectRef self, const std::string &name, ObjectRef value)
{
    /* check for value */
    if (value.isNull())
        throw Exceptions::InternalError("Setting attributes to null");

    /* find the attribute in dict */
    TypeRef type = self->type();
    decltype(self->dict().find(name)) iter = self->dict().find(name);

    /* update it's value if exists */
    if (iter != self->dict().end())
    {
        iter->second = value;
        return;
    }

    /* built-in attributes */
    if (self->attrs().find(name) != self->attrs().end())
        throw Exceptions::AttributeError(Utils::Strings::format("Attribute \"%s\" of \"%s\" is read-only", name, _name));

    /* search this type and all of it's super types */
    while (type.isNotNull() &&
           ((iter = type->dict().find(name)) == type->dict().end()) &&
           ((iter = type->attrs().find(name)) == type->attrs().end()))
        type = type->_super;

    /* attribute not found */
    if (type.isNull())
        throw Exceptions::AttributeError(Utils::Strings::format("\"%s\" object has no attribute \"%s\"", _name, name));

    /* not assigned yet, copy to instance dict */
    if (iter->second.isNull())
    {
        self->dict().emplace(name, value);
        return;
    }

    /* descriptor properties */
    ObjectRef setter;
    Reference<TupleObject> args;

    /* check for descriptor type */
    switch (resolveDescriptor(iter->second, nullptr, &setter, nullptr))
    {
        /* native descriptor */
        case DescriptorType::Native:
        {
            args = TupleObject::fromObjects(self, this->self(), value);
            break;
        }

        /* user-defined proxy-like object descriptor */
        case DescriptorType::UserDefined:
        {
            args = TupleObject::fromObjects(iter->second, self, this->self(), value);
            break;
        }

        /* plain-old object or unbound method */
        case DescriptorType::Unbound:
        case DescriptorType::NotADescriptor:
        {
            self->dict().emplace(name, value);
            return;
        }
    }

    /* check for setter */
    if (setter.isNull())
        throw Exceptions::AttributeError(Utils::Strings::format("Attribute \"%s\" of \"%s\" is read-only", name, _name));

    /* invoke the setter */
    auto kwargs = MapObject::newOrdered();
    auto result = setter->type()->objectInvoke(setter, args, kwargs);

    /* check for null */
    if (result.isNull())
        throw Exceptions::InternalError(Utils::Strings::format("Descriptor \"%s\" of \"%s\" gives null", name, _name));
}

void Type::nativeObjectDefineAttr(ObjectRef self, const std::string &name, ObjectRef value)
{
    /* check for value */
    if (value.isNull())
        throw Exceptions::InternalError("Defining attributes to null");

    /* define the attribute in dict */
    self->dict().emplace(name, value);
}

ObjectRef Type::nativeObjectNew(TypeRef type, Reference<TupleObject> args, Reference<MapObject> kwargs)
{
    /* only `type` object can create new types */
    if (type.isNotIdenticalWith(TypeObject))
        throw Exceptions::InternalError("Invalid type creation");

    /* should have no more than 3 positional arguments */
    if (args->size() > 3)
    {
        throw Exceptions::TypeError(Utils::Strings::format(
            "\"type\" takes at most 3 arguments but %zu given",
            args->size()
        ));
    }

    /* type properties */
    ObjectRef name = kwargs->pop(StringObject::fromStringInterned("name"));
    ObjectRef dict = kwargs->pop(StringObject::fromStringInterned("dict"));
    ObjectRef super = kwargs->pop(StringObject::fromStringInterned("super"));

    /* have first argument (class name) */
    if (name.isNull() && (args->size() >= 1))
        name = args->items()[0];

    /* have second argument (class dict) */
    if (dict.isNull() && (args->size() >= 2))
        dict = args->items()[1];

    /* have third argument (super class) */
    if (super.isNull() && (args->size() >= 3))
        super = args->items()[2];

    /* must have class name */
    if (name.isNull())
        throw Exceptions::TypeError("Missing required class name");

    /* and it must be a string */
    if (name->isNotInstanceOf(StringTypeObject))
    {
        throw Exceptions::TypeError(Utils::Strings::format(
            "Class name must be a string, not \"%s\"",
            name->type()->name()
        ));
    }

    /* dict must be a valid map if present */
    if (dict.isNotNull() && dict->isNotInstanceOf(MapTypeObject))
    {
        throw Exceptions::TypeError(Utils::Strings::format(
            "Class dict must be a map, not \"%s\"",
            dict->type()->name()
        ));
    }

    /* super class must be a valid type if present */
    if (super.isNotNull() && super->isNotInstanceOf(TypeObject))
    {
        throw Exceptions::TypeError(Utils::Strings::format(
            "Super class must be a type, not \"%s\"",
            super->type()->name()
        ));
    }

    /* create a new type */
    return Type::create(name.as<StringObject>()->value(), dict.as<MapObject>(), super.as<Type>());
}

ObjectRef Type::nativeObjectInit(ObjectRef self, Reference<TupleObject> args, Reference<MapObject> kwargs)
{
    /* whatevet it might be, as long as it's not null, return the instance as is */
    if (self.isNull())
        throw Exceptions::InternalError("Invalid type initialization");
    else
        return std::move(self);
}

ObjectRef Type::nativeObjectInvoke(ObjectRef self, Reference<TupleObject> args, Reference<MapObject> kwargs)
{
    /* must be a type object here */
    if (self->isNotInstanceOf(TypeObject))
        throw Exceptions::InternalError("Invalid type call");

    /* create and initialize an object */
    auto type = self.as<Type>();
    auto result = type->objectNew(type, args, kwargs);
    return result->type()->objectInit(result, args, kwargs);
}

/*** Native Boolean Protocol ***/

ObjectRef Type::nativeBoolOr(ObjectRef self, ObjectRef other)
{
    /* default: boolean or their truth value */
    return BoolObject::fromBool(self->type()->objectIsTrue(self) || other->type()->objectIsTrue(other));
}

ObjectRef Type::nativeBoolAnd(ObjectRef self, ObjectRef other)
{
    /* default: boolean and their truth value */
    return BoolObject::fromBool(self->type()->objectIsTrue(self) && other->type()->objectIsTrue(other));
}

ObjectRef Type::nativeBoolNot(ObjectRef self)
{
    /* default: boolean invert it's truth value */
    return BoolObject::fromBool(!(self->type()->objectIsTrue(self)));
}

/*** Native Numeric Protocol ***/

ObjectRef Type::nativeNumericPos(ObjectRef self)                        NOT_IMPL(self, __pos__)
ObjectRef Type::nativeNumericNeg(ObjectRef self)                        NOT_IMPL(self, __neg__)

ObjectRef Type::nativeNumericAdd  (ObjectRef self, ObjectRef other)     NOT_IMPL(self, __add__)
ObjectRef Type::nativeNumericSub  (ObjectRef self, ObjectRef other)     NOT_IMPL(self, __sub__)
ObjectRef Type::nativeNumericMul  (ObjectRef self, ObjectRef other)     NOT_IMPL(self, __mul__)
ObjectRef Type::nativeNumericDiv  (ObjectRef self, ObjectRef other)     NOT_IMPL(self, __div__)
ObjectRef Type::nativeNumericMod  (ObjectRef self, ObjectRef other)     NOT_IMPL(self, __mod__)
ObjectRef Type::nativeNumericPower(ObjectRef self, ObjectRef other)     NOT_IMPL(self, __power__)

ObjectRef Type::nativeNumericOr (ObjectRef self, ObjectRef other)       NOT_IMPL(self, __or__)
ObjectRef Type::nativeNumericAnd(ObjectRef self, ObjectRef other)       NOT_IMPL(self, __and__)
ObjectRef Type::nativeNumericXor(ObjectRef self, ObjectRef other)       NOT_IMPL(self, __xor__)
ObjectRef Type::nativeNumericNot(ObjectRef self)                        NOT_IMPL(self, __not__)

ObjectRef Type::nativeNumericLShift(ObjectRef self, ObjectRef other)    NOT_IMPL(self, __lshift__)
ObjectRef Type::nativeNumericRShift(ObjectRef self, ObjectRef other)    NOT_IMPL(self, __rshift__)

ObjectRef Type::nativeNumericIncAdd  (ObjectRef self, ObjectRef other)  NOT_IMPL(self, __inc_add__)
ObjectRef Type::nativeNumericIncSub  (ObjectRef self, ObjectRef other)  NOT_IMPL(self, __inc_sub__)
ObjectRef Type::nativeNumericIncMul  (ObjectRef self, ObjectRef other)  NOT_IMPL(self, __inc_mul__)
ObjectRef Type::nativeNumericIncDiv  (ObjectRef self, ObjectRef other)  NOT_IMPL(self, __inc_div__)
ObjectRef Type::nativeNumericIncMod  (ObjectRef self, ObjectRef other)  NOT_IMPL(self, __inc_mod__)
ObjectRef Type::nativeNumericIncPower(ObjectRef self, ObjectRef other)  NOT_IMPL(self, __inc_power__)

ObjectRef Type::nativeNumericIncOr (ObjectRef self, ObjectRef other)    NOT_IMPL(self, __inc_or__)
ObjectRef Type::nativeNumericIncAnd(ObjectRef self, ObjectRef other)    NOT_IMPL(self, __inc_and__)
ObjectRef Type::nativeNumericIncXor(ObjectRef self, ObjectRef other)    NOT_IMPL(self, __inc_xor__)

ObjectRef Type::nativeNumericIncLShift(ObjectRef self, ObjectRef other) NOT_IMPL(self, __inc_lshift__)
ObjectRef Type::nativeNumericIncRShift(ObjectRef self, ObjectRef other) NOT_IMPL(self, __inc_rshift__)

/*** Native Iterator Protocol ***/

ObjectRef Type::nativeIterableIter(ObjectRef self) NOT_IMPL(self, __iter__)
ObjectRef Type::nativeIterableNext(ObjectRef self) NOT_IMPL(self, __next__)

/*** Native Sequence Protocol ***/

ObjectRef Type::nativeSequenceLen     (ObjectRef self)                                    NOT_IMPL(self, __len__)
void      Type::nativeSequenceDelItem (ObjectRef self, ObjectRef other)                   NOT_IMPL(self, __delitem__)
ObjectRef Type::nativeSequenceGetItem (ObjectRef self, ObjectRef other)                   NOT_IMPL(self, __getitem__)
void      Type::nativeSequenceSetItem (ObjectRef self, ObjectRef second, ObjectRef third) NOT_IMPL(self, __setitem__)

void Type::nativeSequenceDelSlice(ObjectRef self, ObjectRef begin, ObjectRef end, ObjectRef step)
{
    /* avoid null objects been exported to user program */
    if (end.isNull()) end = NullObject;
    if (step.isNull()) step = NullObject;
    if (begin.isNull()) begin = NullObject;

    /* wrap as delete slicing item */
    sequenceDelItem(self, Object::newObject<SliceObject>(begin, end, step));
}

ObjectRef Type::nativeSequenceGetSlice(ObjectRef self, ObjectRef begin, ObjectRef end, ObjectRef step)
{
    /* avoid null objects been exported to user program */
    if (end.isNull()) end = NullObject;
    if (step.isNull()) step = NullObject;
    if (begin.isNull()) begin = NullObject;

    /* wrap as get slicing item */
    return sequenceGetItem(self, Object::newObject<SliceObject>(begin, end, step));
}

void Type::nativeSequenceSetSlice(ObjectRef self, ObjectRef begin, ObjectRef end, ObjectRef step, ObjectRef value)
{
    /* avoid null objects been exported to user program */
    if (end.isNull()) end = NullObject;
    if (step.isNull()) step = NullObject;
    if (begin.isNull()) begin = NullObject;

    /* wrap as set slicing item */
    sequenceSetItem(self, Object::newObject<SliceObject>(begin, end, step), value);
}

/*** Native Comparator Protocol ***/

ObjectRef Type::nativeComparableLt      (ObjectRef self, ObjectRef other) NOT_IMPL(self, __lt__)
ObjectRef Type::nativeComparableGt      (ObjectRef self, ObjectRef other) NOT_IMPL(self, __gt__)
ObjectRef Type::nativeComparableLeq     (ObjectRef self, ObjectRef other) NOT_IMPL(self, __leq__)
ObjectRef Type::nativeComparableGeq     (ObjectRef self, ObjectRef other) NOT_IMPL(self, __geq__)
ObjectRef Type::nativeComparableContains(ObjectRef self, ObjectRef other) NOT_IMPL(self, __contains__)

ObjectRef Type::nativeComparableEq(ObjectRef self, ObjectRef other)
{
    /* default: pointer comparison */
    return BoolObject::fromBool(self.isIdenticalWith(other));
}

ObjectRef Type::nativeComparableNeq(ObjectRef self, ObjectRef other)
{
    /* default: pointer comparison */
    return BoolObject::fromBool(self.isNotIdenticalWith(other));
}

ObjectRef Type::nativeComparableCompare(ObjectRef self, ObjectRef other)
{
    /* check for equality */
    if (self == other)
        return IntObject::fromInt(0);

    /* compare "greater than" */
    ObjectRef gt = self->type()->comparableGt(self, other);

    /* must not be null */
    if (gt.isNull())
        throw Exceptions::InternalError("\"__gt__\" gives null");

    /* check for truth value */
    if (gt->type()->objectIsTrue(gt))
        return IntObject::fromInt(1);

    /* compare "less than" */
    ObjectRef lt = self->type()->comparableLt(self, other);

    /* must not be null */
    if (lt.isNull())
        throw Exceptions::InternalError("\"__lt__\" gives null");

    /* check for truth value */
    if (lt->type()->objectIsTrue(lt))
        return IntObject::fromInt(-1);

    /* obejcts are unordered */
    throw Exceptions::TypeError(Utils::Strings::format(
        "\"%s\" and \"%s\" objects are unordered",
        self->type()->name(),
        other->type()->name()
    ));
}

#undef NOT_IMPL

/***** Object System Interface *****/

#define APPLY_UNARY(name, func, self) {                                     \
    ObjectRef m = findUserMethod(self, #name, nullptr);                     \
    return m.isNull() ? native ## func(std::move(self))                     \
                      : applyUnaryMethod(std::move(m), std::move(self));    \
}

#define APPLY_BINARY(name, func, self, arg1) {                                              \
    ObjectRef m = findUserMethod(self, #name, nullptr);                                     \
    return m.isNull() ? native ## func(std::move(self), std::move(arg1))                    \
                      : applyBinaryMethod(std::move(m), std::move(self), std::move(arg1));  \
}

#define APPLY_BINARY_ALT(name, alt, func, self, arg1) {                                     \
    ObjectRef m = findUserMethod(self, #name, #alt);                                        \
    return m.isNull() ? native ## func(std::move(self), std::move(arg1))                    \
                      : applyBinaryMethod(std::move(m), std::move(self), std::move(arg1));  \
}

/*** Object Protocol ***/

uint64_t Type::objectHash(ObjectRef self)
{
    /* find the "__hash__" function */
    ObjectRef ret = findUserMethod(self, "__hash__", nullptr);

    /* doesn't have user-defined "__hash__" function */
    if (ret.isNull())
        return nativeObjectHash(self);

    /* apply the user method */
    if ((ret = applyUnaryMethod(std::move(ret), std::move(self))).isNull())
        throw Exceptions::InternalError("User method \"__hash__\" gives null");

    /* must be an integer object */
    if (ret->isNotInstanceOf(IntTypeObject))
    {
        throw Exceptions::TypeError(Utils::Strings::format(
            "\"__hash__\" function must return an integer, not \"%s\"",
            ret->type()->name()
        ));
    }

    /* convert to integer */
    Reference<IntObject> value = ret.as<IntObject>();

    /* and must be a valid unsigned integer */
    if (!(value->isSafeUInt()))
        throw Exceptions::ValueError("\"__hash__\" function must return an unsigned integer");

    /* convert to unsigned integer */
    return value->toUInt();
}

StringList Type::objectDir(ObjectRef self)
{
    /* apply the "__dir__" function */
    ObjectRef ret = findUserMethod(self, "__dir__", nullptr);
    StringList result;

    /* doesn't have user-defined "__dir__" function */
    if (ret.isNull())
        return nativeObjectDir(self);

    /* apply the user method */
    if ((ret = applyUnaryMethod(std::move(ret), std::move(self))).isNull())
        throw Exceptions::InternalError("User method \"__dir__\" gives null");

    /* must be a tuple */
    if (ret->isNotInstanceOf(TupleTypeObject))
    {
        throw Exceptions::TypeError(Utils::Strings::format(
            "\"__dir__\" must return a tuple, not \"%s\"",
            ret->type()->name()
        ));
    }

    /* convert to tuple */
    auto tuple = ret.as<TupleObject>();
    size_t count = tuple->size();
    ObjectRef *items = tuple->items();

    /* fill every item */
    for (size_t i = 0; i < count; i++)
        result.emplace_back(items[i]->type()->objectStr(items[i]));

    /* move to prevent copy */
    return std::move(result);
}

std::string Type::objectStr(ObjectRef self)
{
    /* apply the "__str__" function */
    ObjectRef ret = findUserMethod(self, "__str__", nullptr);

    /* doesn't have user-defined "__str__" function */
    if (ret.isNull())
        return nativeObjectStr(self);

    /* apply the user method */
    if ((ret = applyUnaryMethod(std::move(ret), std::move(self))).isNull())
        throw Exceptions::InternalError("User method \"__str__\" gives null");

    /* must be an integer object */
    if (ret->isNotInstanceOf(StringTypeObject))
    {
        throw Exceptions::TypeError(Utils::Strings::format(
            "\"__str__\" function must return a string, not \"%s\"",
            ret->type()->name()
        ));
    }

    /* get it's value */
    return ret.as<StringObject>()->value();
}

std::string Type::objectRepr(ObjectRef self)
{
    /* apply the "__repr__" function */
    ObjectRef ret = findUserMethod(self, "__repr__", nullptr);

    /* doesn't have user-defined "__repr__" function */
    if (ret.isNull())
        return nativeObjectRepr(self);

    /* apply the user method */
    if ((ret = applyUnaryMethod(std::move(ret), std::move(self))).isNull())
        throw Exceptions::InternalError("User method \"__repr__\" gives null");

    /* must be an integer object */
    if (ret->isNotInstanceOf(StringTypeObject))
    {
        throw Exceptions::TypeError(Utils::Strings::format(
            "\"__repr__\" function must return a string, not \"%s\"",
            ret->type()->name()
        ));
    }

    /* get it's value */
    return ret.as<StringObject>()->value();
}

bool Type::objectIsTrue(ObjectRef self)
{
    /* apply the "__bool__" function */
    ObjectRef ret = findUserMethod(self, "__bool__", nullptr);

    /* doesn't have user-defined "__bool__" function */
    if (ret.isNull())
        return nativeObjectIsTrue(self);

    /* apply the user method */
    if ((ret = applyUnaryMethod(std::move(ret), std::move(self))).isNull())
        throw Exceptions::InternalError("User method \"__bool__\" gives null");

    /* must be a boolean object */
    if (ret->isNotInstanceOf(BoolTypeObject))
    {
        throw Exceptions::TypeError(Utils::Strings::format(
            "\"__bool__\" function must return a boolean, not \"%s\"",
            ret->type()->name()
        ));
    }

    /* get it's value */
    return ret.as<BoolObject>()->value();
}

void Type::objectDelAttr(ObjectRef self, const std::string &name)
{
    /* find the user method */
    ObjectRef method = findUserMethod(self, "__delattr__", nullptr);

    /* doesn't have one, call the native method */
    if (method.isNull())
    {
        nativeObjectDelAttr(std::move(self), name);
        return;
    }

    /* otherwise, apply the user method */
    applyBinaryMethod(
        std::move(method),
        std::move(self),
        StringObject::fromStringInterned(name)
    );
}

ObjectRef Type::objectGetAttr(ObjectRef self, const std::string &name)
{
    /* find the user method */
    ObjectRef method = findUserMethod(self, "__getattr__", nullptr);

    /* doesn't have one, call the native method */
    if (method.isNull())
        return nativeObjectGetAttr(std::move(self), name);

    /* otherwise, apply the user method */
    return applyBinaryMethod(
        std::move(method),
        std::move(self),
        StringObject::fromStringInterned(name)
    );
}

void Type::objectSetAttr(ObjectRef self, const std::string &name, ObjectRef value)
{
    /* find the user method */
    ObjectRef method = findUserMethod(self, "__setattr__", nullptr);

    /* doesn't have one, call the native method */
    if (method.isNull())
    {
        nativeObjectSetAttr(std::move(self), name, std::move(value));
        return;
    }

    /* otherwise, apply the user method */
    applyTernaryMethod(
        std::move(method),
        std::move(self),
        StringObject::fromStringInterned(name),
        std::move(value)
    );
}

ObjectRef Type::objectNew(TypeRef type, Reference<TupleObject> args, Reference<MapObject> kwargs)
{
    /* find the user method */
    auto iter = type->dict().find("__new__");

    /* don't have "__init__" method */
    if (iter == type->dict().end())
        return nativeObjectNew(std::move(type), std::move(args), std::move(kwargs));

    /* must be an unbound method */
    if (iter->second->isNotInstanceOf(UnboundMethodTypeObject))
    {
        throw Exceptions::TypeError(Utils::Strings::format(
            "\"__new__\" must be an unbound method, not \"%s\"",
            iter->second->type()->name()
        ));
    }

    /* invoke the method */
    auto func = iter->second.as<UnboundMethodObject>()->bind(type);
    return func->type()->objectInvoke(std::move(func), std::move(args), std::move(kwargs));
}

ObjectRef Type::objectInit(ObjectRef self, Reference<TupleObject> args, Reference<MapObject> kwargs)
{
    /* find the user method */
    ObjectRef method = findUserMethod(self, "__init__", nullptr);

    /* don't have "__init__" method */
    if (method.isNull())
        return nativeObjectInit(std::move(self), std::move(args), std::move(kwargs));

    /* must be an unbound method */
    if (method->isNotInstanceOf(UnboundMethodTypeObject))
    {
        throw Exceptions::TypeError(Utils::Strings::format(
            "\"__invoke__\" must be an unbound method, not \"%s\"",
            method->type()->name()
        ));
    }

    /* invoke the method */
    auto func = method.as<UnboundMethodObject>()->bind(self);
    auto result = func->type()->objectInvoke(std::move(func), std::move(args), std::move(kwargs));

    /* check for null reference */
    if (result.isNull())
        throw Exceptions::InternalError("Constructor gives nullptr");

    /* user-defined constructor must return null */
    if (result.isNotIdenticalWith(NullObject))
    {
        throw Exceptions::TypeError(Utils::Strings::format(
            "Constructor must return null, not \"%s\"",
            result->type()->name()
        ));
    }

    /* move to prevent copy */
    return std::move(self);
}

ObjectRef Type::objectInvoke(ObjectRef self, Reference<TupleObject> args, Reference<MapObject> kwargs)
{
    /* find the user method */
    ObjectRef method = findUserMethod(self, "__invoke__", nullptr);

    /* don't have "__invoke__" method */
    if (method.isNull())
        return nativeObjectInvoke(std::move(self), std::move(args), std::move(kwargs));

    /* must be an unbound method */
    if (method->isNotInstanceOf(UnboundMethodTypeObject))
    {
        throw Exceptions::TypeError(Utils::Strings::format(
            "\"__invoke__\" must be an unbound method, not \"%s\"",
            method->type()->name()
        ));
    }

    /* invoke the method */
    auto func = method.as<UnboundMethodObject>()->bind(self);
    return func->type()->objectInvoke(std::move(func), std::move(args), std::move(kwargs));
}

/*** Boolean Protocol ***/

ObjectRef Type::boolOr (ObjectRef self, ObjectRef other)            APPLY_BINARY(__bool_or__ , BoolOr , self, other)
ObjectRef Type::boolAnd(ObjectRef self, ObjectRef other)            APPLY_BINARY(__bool_and__, BoolAnd, self, other)
ObjectRef Type::boolNot(ObjectRef self)                             APPLY_UNARY (__bool_not__, BoolNot, self)

/*** Numeric Protocol ***/

ObjectRef Type::numericPos(ObjectRef self)                          APPLY_UNARY (__pos__   , NumericPos   , self)
ObjectRef Type::numericNeg(ObjectRef self)                          APPLY_UNARY (__neg__   , NumericNeg   , self)

ObjectRef Type::numericAdd  (ObjectRef self, ObjectRef other)       APPLY_BINARY(__add__   , NumericAdd   , self, other)
ObjectRef Type::numericSub  (ObjectRef self, ObjectRef other)       APPLY_BINARY(__sub__   , NumericSub   , self, other)
ObjectRef Type::numericMul  (ObjectRef self, ObjectRef other)       APPLY_BINARY(__mul__   , NumericMul   , self, other)
ObjectRef Type::numericDiv  (ObjectRef self, ObjectRef other)       APPLY_BINARY(__div__   , NumericDiv   , self, other)
ObjectRef Type::numericMod  (ObjectRef self, ObjectRef other)       APPLY_BINARY(__mod__   , NumericMod   , self, other)
ObjectRef Type::numericPower(ObjectRef self, ObjectRef other)       APPLY_BINARY(__power__ , NumericPower , self, other)

ObjectRef Type::numericOr (ObjectRef self, ObjectRef other)         APPLY_BINARY(__or__    , NumericOr    , self, other)
ObjectRef Type::numericAnd(ObjectRef self, ObjectRef other)         APPLY_BINARY(__and__   , NumericAnd   , self, other)
ObjectRef Type::numericXor(ObjectRef self, ObjectRef other)         APPLY_BINARY(__xor__   , NumericXor   , self, other)
ObjectRef Type::numericNot(ObjectRef self)                          APPLY_UNARY (__not__   , NumericNot   , self)

ObjectRef Type::numericLShift(ObjectRef self, ObjectRef other)      APPLY_BINARY(__lshift__, NumericLShift, self, other)
ObjectRef Type::numericRShift(ObjectRef self, ObjectRef other)      APPLY_BINARY(__rshift__, NumericRShift, self, other)

ObjectRef Type::numericIncAdd  (ObjectRef self, ObjectRef other)    APPLY_BINARY_ALT(__inc_add__   , __add__   , NumericIncAdd   , self, other)
ObjectRef Type::numericIncSub  (ObjectRef self, ObjectRef other)    APPLY_BINARY_ALT(__inc_sub__   , __sub__   , NumericIncSub   , self, other)
ObjectRef Type::numericIncMul  (ObjectRef self, ObjectRef other)    APPLY_BINARY_ALT(__inc_mul__   , __mul__   , NumericIncMul   , self, other)
ObjectRef Type::numericIncDiv  (ObjectRef self, ObjectRef other)    APPLY_BINARY_ALT(__inc_div__   , __div__   , NumericIncDiv   , self, other)
ObjectRef Type::numericIncMod  (ObjectRef self, ObjectRef other)    APPLY_BINARY_ALT(__inc_mod__   , __mod__   , NumericIncMod   , self, other)
ObjectRef Type::numericIncPower(ObjectRef self, ObjectRef other)    APPLY_BINARY_ALT(__inc_power__ , __power__ , NumericIncPower , self, other)

ObjectRef Type::numericIncOr (ObjectRef self, ObjectRef other)      APPLY_BINARY_ALT(__inc_or__    , __or__    , NumericIncOr    , self, other)
ObjectRef Type::numericIncAnd(ObjectRef self, ObjectRef other)      APPLY_BINARY_ALT(__inc_and__   , __and__   , NumericIncAnd   , self, other)
ObjectRef Type::numericIncXor(ObjectRef self, ObjectRef other)      APPLY_BINARY_ALT(__inc_xor__   , __xor__   , NumericIncXor   , self, other)

ObjectRef Type::numericIncLShift(ObjectRef self, ObjectRef other)   APPLY_BINARY_ALT(__inc_lshift__, __lshift__, NumericIncLShift, self, other)
ObjectRef Type::numericIncRShift(ObjectRef self, ObjectRef other)   APPLY_BINARY_ALT(__inc_rshift__, __rshift__, NumericIncRShift, self, other)

/*** Iterator Protocol ***/

ObjectRef Type::iterableIter(ObjectRef self)                        APPLY_UNARY (__iter__, IterableIter, self)
ObjectRef Type::iterableNext(ObjectRef self)                        APPLY_UNARY (__next__, IterableNext, self)

/*** Sequence Protocol ***/

ObjectRef Type::sequenceLen(ObjectRef self)                         APPLY_UNARY (__len__    , SequenceLen, self)
ObjectRef Type::sequenceGetItem(ObjectRef self, ObjectRef other)    APPLY_BINARY(__getitem__, SequenceGetItem, self, other)

void Type::sequenceDelItem(ObjectRef self, ObjectRef other)
{
    /* find the user method */
    ObjectRef method = findUserMethod(self, "__delitem__", nullptr);

    /* doesn't have one, call the native method */
    if (method.isNull())
    {
        nativeSequenceDelItem(std::move(self), std::move(other));
        return;
    }

    /* otherwise, apply the user method */
    applyBinaryMethod(
        std::move(method),
        std::move(self),
        std::move(other)
    );
}

void Type::sequenceSetItem(ObjectRef self, ObjectRef second, ObjectRef third)
{
    /* find the user method */
    ObjectRef method = findUserMethod(self, "__setitem__", nullptr);

    /* doesn't have one, call the native method */
    if (method.isNull())
    {
        nativeSequenceSetItem(std::move(self), std::move(second), std::move(third));
        return;
    }

    /* otherwise, apply the user method */
    applyTernaryMethod(
        std::move(method),
        std::move(self),
        std::move(second),
        std::move(third)
    );
}

/*** Comparator Protocol ***/

ObjectRef Type::comparableEq      (ObjectRef self, ObjectRef other) APPLY_BINARY(__eq__      , ComparableEq      , self, other)
ObjectRef Type::comparableLt      (ObjectRef self, ObjectRef other) APPLY_BINARY(__lt__      , ComparableLt      , self, other)
ObjectRef Type::comparableGt      (ObjectRef self, ObjectRef other) APPLY_BINARY(__gt__      , ComparableGt      , self, other)
ObjectRef Type::comparableNeq     (ObjectRef self, ObjectRef other) APPLY_BINARY(__neq__     , ComparableNeq     , self, other)
ObjectRef Type::comparableLeq     (ObjectRef self, ObjectRef other) APPLY_BINARY(__leq__     , ComparableLeq     , self, other)
ObjectRef Type::comparableGeq     (ObjectRef self, ObjectRef other) APPLY_BINARY(__geq__     , ComparableGeq     , self, other)
ObjectRef Type::comparableCompare (ObjectRef self, ObjectRef other) APPLY_BINARY(__compare__ , ComparableCompare , self, other)
ObjectRef Type::comparableContains(ObjectRef self, ObjectRef other) APPLY_BINARY(__contains__, ComparableContains, self, other)

#undef APPLY_UNARY
#undef APPLY_BINARY
#undef APPLY_BINARY_ALT

/*** Custom Class Creation Interface ***/

TypeRef Type::create(const std::string &name, Reference<MapObject> dict, TypeRef super)
{
    /* create a new type */
    TypeRef type = super.isNull()
        ? Object::newObject<ObjectType>(name)
        : Object::newObject<ObjectType>(name, super);

    /* fill the type dict if any */
    if (dict.isNotNull())
    {
        /* enumerate all it's items */
        dict->enumerate([&](ObjectRef key, ObjectRef value)
        {
            /* dict key must be a string */
            if (key->isNotInstanceOf(StringTypeObject))
            {
                throw Exceptions::TypeError(Utils::Strings::format(
                    "Class dict key must be a string, not \"%s\"",
                    key->type()->name()
                ));
            }

            /* if it's a function or native function, wrap with unbound method */
            if (value->isInstanceOf(FunctionTypeObject) ||
                value->isInstanceOf(NativeFunctionTypeObject))
                value = UnboundMethodObject::fromCallable(std::move(value));

            /* add to class dict, and continue enumerating */
            type->dict().emplace(key.as<StringObject>()->value(), std::move(value));
            return true;
        });
    }

    /* move to prevent copy */
    return std::move(type);
}

/** ObjectType implementation **/

ObjectRef ObjectType::nativeObjectNew(TypeRef type, Reference<TupleObject> args, Reference<MapObject> kwargs)
{
    /* simply create a new object of type `type` */
    return Object::newObject<Object>(type);
}

ObjectRef ObjectType::nativeObjectInit(ObjectRef self, Reference<TupleObject> args, Reference<MapObject> kwargs)
{
    /* root object doesn't take any arguments by default */
    if (args->size())
    {
        throw Exceptions::TypeError(Utils::Strings::format(
            "\"__init__\" of \"%s\" object takes no arguments, but %zu given",
            self->type()->name(),
            args->size()
        ));
    }

    /* and no keyword arguments */
    if (kwargs->size())
    {
        throw Exceptions::TypeError(Utils::Strings::format(
            "\"__init__\" of \"%s\" object doesn't accept keyword argument \"%s\"",
            self->type()->name(),
            kwargs->front()->type()->objectStr(kwargs->front())
        ));
    }

    /* move to prevent copy */
    return std::move(self);
}

ObjectRef ObjectType::nativeObjectInvoke(ObjectRef self, Reference<TupleObject> args, Reference<MapObject> kwargs)
{
    throw Exceptions::TypeError(Utils::Strings::format(
        "\"%s\" object is not callable",
        self->type()->name()
    ));
}
}
