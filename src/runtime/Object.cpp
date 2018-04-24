#include "runtime/Object.h"
#include "runtime/BoolObject.h"

namespace RedScript::Runtime
{
/* the very first class */
TypeRef TypeObject;

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

void Object::initialize(void)
{
    static Type rootClass("type", nullptr);
    TypeObject = rootClass._type = TypeRef::refStatic(rootClass);
}

/*** Type Implementations ***/

ObjectRef Type::applyUnary(const char *name, ObjectRef self)
{
    return ObjectRef();
}

ObjectRef Type::applyBinary(const char *name, ObjectRef self, ObjectRef other, const char *alternative)
{
    return ObjectRef();
}

ObjectRef Type::applyTernary(const char *name, ObjectRef self, ObjectRef second, ObjectRef third)
{
    return ObjectRef();
}

void Type::objectClear(ObjectRef self)
{

}

void Type::objectTraverse(ObjectRef self, Type::VisitFunction visit)
{

}

uint64_t Type::objectHash(ObjectRef self)
{
    std::hash<uintptr_t> hash;
    return hash(reinterpret_cast<uintptr_t>(self.get()));
}

StringList Type::objectDir(ObjectRef self)
{
    return StringList();
}

std::string Type::objectRepr(ObjectRef self)
{
    return std::string();
}

bool Type::objectIsSubclassOf(ObjectRef self, TypeRef type)
{
    /* not a type at all */
    if (self->type() != TypeObject)
        return false;

    /* convert to type reference */
    TypeRef t = self.as<Type>();

    /* search for parent classes */
    while (t != type && t != TypeObject)
        t = t->super();

    /* check for type */
    return t == type;
}

ObjectRef Type::objectDelAttr(ObjectRef self, const std::string &name)
{
    return ObjectRef();
}

ObjectRef Type::objectGetAttr(ObjectRef self, const std::string &name)
{
    return ObjectRef();
}

ObjectRef Type::objectSetAttr(ObjectRef self, const std::string &name, ObjectRef value)
{
    return ObjectRef();
}

ObjectRef Type::objectInvoke(ObjectRef self, const std::vector<ObjectRef> &args)
{
    return ObjectRef();
}

ObjectRef Type::comparableEq(ObjectRef self, ObjectRef other)
{
    // TODO: apply binary operator if any
    return BoolObject::fromBool(self.get() == other.get());
}

ObjectRef Type::comparableNeq(ObjectRef self, ObjectRef other)
{
    // TODO: apply binary operator if any
    return BoolObject::fromBool(self.get() == other.get());
}
}
