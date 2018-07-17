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
    /* class name */
    dict().emplace("__name__", StringObject::fromString(_name));

    /* basic object protocol functions */
    dict().emplace("__str__" , UnboundMethodObject::fromFunction([](ObjectRef self){ return self->type()->objectStr (self); }));
    dict().emplace("__repr__", UnboundMethodObject::fromFunction([](ObjectRef self){ return self->type()->objectRepr(self); }));
    dict().emplace("__hash__", UnboundMethodObject::fromFunction([](ObjectRef self){ return self->type()->objectHash(self); }));

    /* built-in "__delattr__" function */
    dict().emplace(
        "__delattr__",
        UnboundMethodObject::fromFunction([](Runtime::ObjectRef self, const std::string &name)
        {
            /* invoke the object protocol */
            self->type()->objectDelAttr(self, name);
        })
    );

    /* built-in "__getattr__" function */
    dict().emplace(
        "__getattr__",
        UnboundMethodObject::fromFunction([](Runtime::ObjectRef self, const std::string &name)
        {
            /* invoke the object protocol */
            return self->type()->objectGetAttr(self, name);
        })
    );

    /* built-in "__setattr__" function */
    dict().emplace(
        "__setattr__",
        UnboundMethodObject::fromFunction([](Runtime::ObjectRef self, const std::string &name, Runtime::ObjectRef value)
        {
            /* invoke the object protocol */
            self->type()->objectSetAttr(self, name, value);
        })
    );
}

ObjectRef Type::applyUnary(const char *name, ObjectRef self)
{
    // TODO: implement this
    throw Exceptions::InternalError("not implemented yet");
}

ObjectRef Type::applyBinary(const char *name, ObjectRef self, ObjectRef other, const char *alternative)
{
    // TODO: implement this
    throw Exceptions::InternalError("not implemented yet");
}

ObjectRef Type::applyTernary(const char *name, ObjectRef self, ObjectRef second, ObjectRef third)
{
    // TODO: implement this
    throw Exceptions::InternalError("not implemented yet");
}

/*** Object Protocol ***/

uint64_t Type::objectHash(ObjectRef self)
{
    // TODO: apply "__hash__" if any
    std::hash<uintptr_t> hash;
    return hash(reinterpret_cast<uintptr_t>(self.get()));
}

StringList Type::objectDir(ObjectRef self)
{
    // TODO: apply "__dir__" if any
    StringList result;
    for (const auto &x : self->dict()) result.emplace_back(x.first);
    return std::move(result);
}

std::string Type::objectRepr(ObjectRef self)
{
    // TODO: apply "__repr__" if any

    /* basic object representation */
    if (self->isNotInstanceOf(TypeObject))
        return Utils::Strings::format("<%s object at %p>", _name, static_cast<void *>(self.get()));

    /* type object representation */
    auto type = self.as<Type>();
    return Utils::Strings::format("<type \"%s\" at %p>", type->name(), static_cast<void *>(type.get()));
}

bool Type::objectIsSubclassOf(ObjectRef self, TypeRef type)
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
    /* find the attribute in dict */
    auto iter = self->dict().find(name);

    /* found in instance dict, erase directly */
    if (iter != self->dict().end())
    {
        self->dict().erase(iter);
        return;
    }

    /* find in type dict */
    if ((iter = this->dict().find(name)) == this->dict().end())
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

ObjectRef Type::objectGetAttr(ObjectRef self, const std::string &name)
{
    /* find the attribute in dict */
    auto iter = self->dict().find(name);

    /* if not exists, search from type dict, throw an exception if not found either */
    if (iter != self->dict().end())
    {
        /* check for null, and read it's value directly */
        if (iter->second.isNull())
            throw Exceptions::AttributeError(Utils::Strings::format("Attribute \"%s\" of \"%s\" is not defined yet", name, _name));
        else
            return iter->second;
    }

    /* find in type dict */
    if ((iter = this->dict().find(name)) == this->dict().end())
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

void Type::objectSetAttr(ObjectRef self, const std::string &name, ObjectRef value)
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

    /* find in type dict */
    if ((iter = this->dict().find(name)) == this->dict().end())
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
    throw Exceptions::TypeError(Utils::Strings::format(
        "\"%s\" object is not callable",
        self->type()->name()
    ));
}

/*** Boolean Protocol ***/

ObjectRef Type::boolOr(ObjectRef self, ObjectRef other)
{
    // TODO: apply binary operator if any
    // return applyBinary("__bool_or__", self, other);
    return BoolObject::fromBool(
        self->type()->objectIsTrue(self) ||
        other->type()->objectIsTrue(other)
    );
}

ObjectRef Type::boolAnd(ObjectRef self, ObjectRef other)
{
    // TODO: apply binary operator if any
    // return applyBinary("__bool_and__", self, other);
    return BoolObject::fromBool(
        self->type()->objectIsTrue(self) &&
        other->type()->objectIsTrue(other)
    );
}

ObjectRef Type::boolNot(ObjectRef self)
{
    // TODO: apply binary operator if any
    // return applyUnary("__bool_not__", self);
    return BoolObject::fromBool(!(self->type()->objectIsTrue(self)));
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
    // TODO: apply binary operator if any
    // return applyBinary("__eq__", self, other);
    return BoolObject::fromBool(self.get() == other.get());
}

ObjectRef Type::comparableNeq(ObjectRef self, ObjectRef other)
{
    // TODO: apply binary operator if any
    // return applyBinary("__neq__", self, other);
    return BoolObject::fromBool(self.get() == other.get());
}

ObjectRef Type::comparableCompare(ObjectRef self, ObjectRef other)
{
    // TODO: apply binary operator if any
    // return applyBinary("__compare__", self, other);

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
}
