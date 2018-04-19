#include "runtime/Object.h"

namespace RedScript::Runtime
{
/* the very first class */
TypeRef TypeObject;

/*** Object Implementations ***/

Object::~Object()
{
    if (_type != TypeObject)
        _type->objectDestroy(self());
}

Object::Object(TypeRef type) : _type(type)
{
    if (_type != TypeObject)
        _type->objectInit(self());
}

void Object::initialize(void)
{
    static Type rootClass(nullptr);
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

void Type::objectInit(ObjectRef self)
{

}

void Type::objectClear(ObjectRef self)
{

}

void Type::objectDestroy(ObjectRef self)
{

}

void Type::objectTraverse(ObjectRef self, Type::VisitFunction visit)
{

}

uint64_t Type::objectHash(ObjectRef self)
{
    return 0;
}

StringList Type::objectDir(ObjectRef self)
{
    return StringList();
}

std::string Type::objectStr(ObjectRef self)
{
    return std::string();
}

std::string Type::objectRepr(ObjectRef self)
{
    return std::string();
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
    return ObjectRef();
}

ObjectRef Type::comparableNeq(ObjectRef self, ObjectRef other)
{
    return ObjectRef();
}
}
