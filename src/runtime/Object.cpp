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
#include "runtime/UnboundMethodObject.h"

#include "utils/Strings.h"
#include "exceptions/TypeError.h"
#include "exceptions/InternalError.h"
#include "exceptions/AttributeError.h"

namespace RedScript::Runtime
{
/* the very first class */
TypeRef TypeObject;

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
    static Type rootClass("type", nullptr);
    TypeObject = rootClass._type = TypeRef::refStatic(rootClass);
}

/*** Type Implementations ***/

void Type::addBuiltins(void)
{
    /* basic object protocol attributes */
    attrs().emplace("__name__", StringObject::fromString(_name));
    attrs().emplace("__repr__", UnboundMethodObject::fromFunction([](ObjectRef self){ return self->type()->defaultObjectRepr(self); }));
    attrs().emplace("__hash__", UnboundMethodObject::fromFunction([](ObjectRef self){ return self->type()->defaultObjectHash(self); }));

    /* built-in "__delattr__" function */
    attrs().emplace(
        "__delattr__",
        UnboundMethodObject::fromFunction([](Runtime::ObjectRef self, const std::string &name)
        {
            /* invoke the object protocol */
            self->type()->defaultObjectDelAttr(self, name);
        })
    );

    /* built-in "__getattr__" function */
    attrs().emplace(
        "__getattr__",
        UnboundMethodObject::fromFunction([](Runtime::ObjectRef self, const std::string &name)
        {
            /* invoke the object protocol */
            return self->type()->defaultObjectGetAttr(self, name);
        })
    );

    /* built-in "__setattr__" function */
    attrs().emplace(
        "__setattr__",
        UnboundMethodObject::fromFunction([](Runtime::ObjectRef self, const std::string &name, Runtime::ObjectRef value)
        {
            /* invoke the object protocol */
            self->type()->defaultObjectSetAttr(self, name, value);
        })
    );
}

void Type::clearBuiltins(void)
{
    dict().clear();
    attrs().clear();
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

ObjectRef Type::applyUnary(const char *name, ObjectRef self, bool isSilent)
{
    /* find the user method */
    ObjectRef method = findUserMethod(self, name, nullptr);

    /* found, apply the method */
    if (!(method.isNull()))
        return applyUnaryMethod(std::move(method), std::move(self));

    /* don't throw exceptions if required */
    if (isSilent)
        return nullptr;

    /* otherwise throw the exception */
    throw Exceptions::AttributeError(Utils::Strings::format(
        "\"%s\" object doesn't support \"%s\" action",
        self->type()->name(),
        name
    ));
}

ObjectRef Type::applyBinary(const char *name, ObjectRef self, ObjectRef other, const char *alt, bool isSilent)
{
    /* find the user method */
    ObjectRef method = findUserMethod(self, name, alt);

    /* found, apply the method */
    if (!(method.isNull()))
        return applyBinaryMethod(std::move(method), std::move(self), std::move(other));

    /* don't throw exceptions if required */
    if (isSilent)
        return nullptr;

    /* otherwise throw the exception */
    throw Exceptions::AttributeError(Utils::Strings::format(
        "\"%s\" object doesn't support \"%s\" action",
        self->type()->name(),
        name
    ));
}

ObjectRef Type::applyTernary(const char *name, ObjectRef self, ObjectRef second, ObjectRef third, bool isSilent)
{
    /* find the user method */
    ObjectRef method = findUserMethod(self, name, nullptr);

    /* found, apply the method */
    if (!(method.isNull()))
        return applyTernaryMethod(std::move(method), std::move(self), std::move(second), std::move(third));

    /* don't throw exceptions if required */
    if (isSilent)
        return nullptr;

    /* otherwise throw the exception */
    throw Exceptions::AttributeError(Utils::Strings::format(
        "\"%s\" object doesn't support \"%s\" action",
        self->type()->name(),
        name
    ));
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

/*** Default Object Protocol ***/

uint64_t Type::defaultObjectHash(ObjectRef self)
{
    /* default: hash the object pointer */
    std::hash<uintptr_t> hash;
    return hash(reinterpret_cast<uintptr_t>(self.get()));
}

StringList Type::defaultObjectDir(ObjectRef self)
{
    /* result name list */
    StringList result;

    /* list every key in object dict */
    for (const auto &x : self->dict())
        result.emplace_back(x.first);

    /* and built-in attributes */
    for (const auto &x : self->attrs())
        result.emplace_back(x.first);

    /* sort the names, and remove duplications */
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return std::move(result);
}

std::string Type::defaultObjectRepr(ObjectRef self)
{
    /* basic object representation */
    if (self->isNotInstanceOf(TypeObject))
        return Utils::Strings::format("<%s object at %p>", _name, static_cast<void *>(self.get()));

    /* type object representation */
    auto type = self.as<Type>();
    return Utils::Strings::format("<type \"%s\" at %p>", type->name(), static_cast<void *>(type.get()));
}

bool Type::defaultObjectIsSubclassOf(ObjectRef self, TypeRef type)
{
    /* not a type at all */
    if (self->type() != TypeObject)
        return false;

    /* convert to type reference */
    TypeRef t = self.as<Type>();

    /* search for parent classes */
    while (t.isIdenticalWith(type) && t.isIdenticalWith(TypeObject))
        t = t->super();

    /* check for type */
    return t.isIdenticalWith(type);
}

void Type::defaultObjectDelAttr(ObjectRef self, const std::string &name)
{
    /* find the attribute in dict */
    auto iter = self->dict().find(name);

    /* found in instance dict, erase directly */
    if (iter != self->dict().end())
    {
        self->dict().erase(iter);
        return;
    }

    /* built-in attributes */
    if (self->attrs().find(name) != self->attrs().end())
        throw Exceptions::AttributeError(Utils::Strings::format("Cannot delete built-in attribute \"%s\" of \"%s\"", name, _name));

    /* find in type dict */
    if ((iter = this->dict().find(name)) == this->dict().end())
        if ((iter = this->attrs().find(name)) == this->attrs().end())
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

ObjectRef Type::defaultObjectGetAttr(ObjectRef self, const std::string &name)
{
    /* attribute dictionary iterator */
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

    /* find in type dict and built-in type attribute dict */
    if ((iter = this->dict().find(name)) == this->dict().end())
        if ((iter = this->attrs().find(name)) == this->attrs().end())
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

void Type::defaultObjectSetAttr(ObjectRef self, const std::string &name, ObjectRef value)
{
    /* check for value */
    if (value.isNull())
        throw Exceptions::InternalError("Setting attributes to null");

    /* find the attribute in dict */
    auto iter = self->dict().find(name);

    /* update it's value if exists */
    if (iter != self->dict().end())
    {
        iter->second = value;
        return;
    }

    /* built-in attributes */
    if (self->attrs().find(name) != self->attrs().end())
        throw Exceptions::AttributeError(Utils::Strings::format("Attribute \"%s\" of \"%s\" is read-only", name, _name));

    /* find in type dict */
    if ((iter = this->dict().find(name)) == this->dict().end())
        if ((iter = this->attrs().find(name)) == this->attrs().end())
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

/*** Default Boolean Protocol ***/

ObjectRef Type::defaultBoolOr(ObjectRef self, ObjectRef other)
{
    /* default: boolean or their truth value */
    return BoolObject::fromBool(self->type()->objectIsTrue(self) || other->type()->objectIsTrue(other));
}

ObjectRef Type::defaultBoolAnd(ObjectRef self, ObjectRef other)
{
    /* default: boolean and their truth value */
    return BoolObject::fromBool(self->type()->objectIsTrue(self) && other->type()->objectIsTrue(other));
}

ObjectRef Type::defaultBoolNot(ObjectRef self)
{
    /* default: boolean invert it's truth value */
    return BoolObject::fromBool(!(self->type()->objectIsTrue(self)));
}

/*** Default Comparator Protocol ***/

ObjectRef Type::defaultComparableEq(ObjectRef self, ObjectRef other)
{
    /* default: pointer comparison */
    return BoolObject::fromBool(self.isIdenticalWith(other));
}

ObjectRef Type::defaultComparableNeq(ObjectRef self, ObjectRef other)
{
    /* default: pointer comparison */
    return BoolObject::fromBool(self.isNotIdenticalWith(other));
}

ObjectRef Type::defaultComparableCompare(ObjectRef self, ObjectRef other)
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

/*** Object Protocol ***/

uint64_t Type::objectHash(ObjectRef self)
{
    /* apply the "__hash__" function */
    ObjectRef ret = applyUnary("__hash__", self, true);

    /* doesn't have user-defined "__hash__" function */
    if (ret.isNull())
        return defaultObjectHash(self);

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
    ObjectRef ret = applyUnary("__dir__", self, true);
    StringList result;

    /* doesn't have user-defined "__dir__" function */
    if (ret.isNull())
        return defaultObjectDir(self);

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
    ObjectRef ret = applyUnary("__str__", self, true);

    /* doesn't have user-defined "__str__" function */
    if (ret.isNull())
        return defaultObjectStr(self);

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
    ObjectRef ret = applyUnary("__repr__", self, true);

    /* doesn't have user-defined "__repr__" function */
    if (ret.isNull())
        return defaultObjectRepr(self);

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

bool Type::objectIsSubclassOf(ObjectRef self, TypeRef type)
{
    ObjectRef ret = applyBinary("__is_subclass_of__", self, type, nullptr, true);
    return ret.isNull() ? defaultObjectIsSubclassOf(std::move(self), std::move(type)) : ret->type()->objectIsTrue(ret);
}

bool Type::objectIsInstanceOf(ObjectRef self, TypeRef type)
{
    ObjectRef ret = applyBinary("__is_instance_of__", self, type, nullptr, true);
    return ret.isNull() ? defaultObjectIsInstanceOf(std::move(self), std::move(type)) : ret->type()->objectIsTrue(ret);
}

bool Type::objectHasAttr(ObjectRef self, const std::string &name)
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

void Type::objectDelAttr(ObjectRef self, const std::string &name)
{
    /* find the user method */
    ObjectRef method = findUserMethod(self, "__delattr__", nullptr);

    /* doesn't have one, call the default */
    if (method.isNull())
    {
        defaultObjectDelAttr(std::move(self), name);
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

    /* doesn't have one, call the default */
    if (method.isNull())
        return defaultObjectGetAttr(std::move(self), name);

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

    /* doesn't have one, call the default */
    if (method.isNull())
    {
        defaultObjectSetAttr(std::move(self), name, std::move(value));
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

void Type::objectDefineAttr(ObjectRef self, const std::string &name, ObjectRef value)
{
    /* check for value */
    if (value.isNull())
        throw Exceptions::InternalError("Defining attributes to null");

    /* define the attribute in dict */
    self->dict().emplace(name, value);
}

ObjectRef Type::objectInvoke(ObjectRef self, ObjectRef args, ObjectRef kwargs)
{
    /* apply the user method if any */
    ObjectRef ret = applyTernary(
        "__invoke__",
        std::move(self),
        std::move(args),
        std::move(kwargs),
        true
    );

    /* apply successful */
    if (!(ret.isNull()))
        return std::move(ret);

    /* otherwise, not callable */
    throw Exceptions::TypeError(Utils::Strings::format(
        "\"%s\" object is not callable",
        self->type()->name()
    ));
}

/*** Boolean Protocol ***/

ObjectRef Type::boolOr(ObjectRef self, ObjectRef other)
{
    ObjectRef ret = applyBinary("__bool_or__", self, other, nullptr, true);
    return ret.isNull() ? defaultBoolOr(std::move(self), std::move(other)) : std::move(ret);
}

ObjectRef Type::boolAnd(ObjectRef self, ObjectRef other)
{
    ObjectRef ret = applyBinary("__bool_and__", self, other, nullptr, true);
    return ret.isNull() ? defaultBoolAnd(std::move(self), std::move(other)) : std::move(ret);
}

ObjectRef Type::boolNot(ObjectRef self)
{
    ObjectRef ret = applyUnary("__bool_not__", self, true);
    return ret.isNull() ? defaultBoolNot(std::move(self)) : std::move(ret);
}

/*** Sequence Protocol ***/

void Type::sequenceDelSlice(ObjectRef self, ObjectRef begin, ObjectRef end, ObjectRef step)
{
    /* avoid null objects been exported to user program */
    if (end.isNull()) end = NullObject;
    if (step.isNull()) step = NullObject;
    if (begin.isNull()) begin = NullObject;

    /* wrap as delete slicing item */
    self->type()->sequenceDelItem(self, Object::newObject<SliceObject>(begin, end, step));
}

ObjectRef Type::sequenceGetSlice(ObjectRef self, ObjectRef begin, ObjectRef end, ObjectRef step)
{
    /* avoid null objects been exported to user program */
    if (end.isNull()) end = NullObject;
    if (step.isNull()) step = NullObject;
    if (begin.isNull()) begin = NullObject;

    /* wrap as get slicing item */
    return self->type()->sequenceGetItem(self, Object::newObject<SliceObject>(begin, end, step));
}

void Type::sequenceSetSlice(ObjectRef self, ObjectRef begin, ObjectRef end, ObjectRef step, ObjectRef value)
{
    /* avoid null objects been exported to user program */
    if (end.isNull()) end = NullObject;
    if (step.isNull()) step = NullObject;
    if (begin.isNull()) begin = NullObject;

    /* wrap as set slicing item */
    self->type()->sequenceSetItem(self, Object::newObject<SliceObject>(begin, end, step), value);
}

/*** Comparator Protocol ***/

ObjectRef Type::comparableEq(ObjectRef self, ObjectRef other)
{
    ObjectRef ret = applyBinary("__eq__", self, other, nullptr, true);
    return ret.isNull() ? defaultComparableEq(std::move(self), std::move(other)) : std::move(ret);
}

ObjectRef Type::comparableNeq(ObjectRef self, ObjectRef other)
{
    ObjectRef ret = applyBinary("__neq__", self, other, nullptr, true);
    return ret.isNull() ? defaultComparableNeq(std::move(self), std::move(other)) : std::move(ret);
}

ObjectRef Type::comparableCompare(ObjectRef self, ObjectRef other)
{
    ObjectRef ret = applyBinary("__compare__", self, other, nullptr, true);
    return ret.isNull() ? defaultComparableCompare(std::move(self), std::move(other)) : std::move(ret);
}
}
