#include "runtime/Object.h"

namespace RedScript::Runtime
{
struct MetaClassInit
{
    MetaClassInit()
    {
        static Type rootClass(nullptr);
        MetaType = rootClass._type = TypeRef::refStatic(rootClass);
    }
};

/* the very first class */
TypeRef MetaType;
static MetaClassInit __META_CLASS_INIT__;

/*** Object Implementations ***/

/* flag to indicate that is instaniated by `new`, should be `long` in order to perform CAS operations */
static thread_local std::atomic_bool _isStaticObject = true;
static_assert(std::atomic_bool::is_always_lock_free, "Non lock-free static object flag");

static inline bool isStaticObject(void)
{
    /* a simple CAS is good enough */
    return _isStaticObject.exchange(true);
}

Object::~Object()
{
    if (_type != MetaType)
        _type->objectDestroy(self());
}

Object::Object(TypeRef type) : _type(type), _isStatic(isStaticObject())
{
    if (_type != MetaType)
        _type->objectInit(self());
}

void *Object::operator new(size_t size)
{
    /* check for minimun required size */
    if (size < sizeof(Object))
        throw std::bad_alloc();

    /* mark as dynamic created object, and allocate new object from GC */
    _isStaticObject.store(false);
    return Engine::GarbageCollector::allocObject(size);
}

void Object::operator delete(void *self)
{
    /* just free the object into GC */
    Engine::GarbageCollector::freeObject(self);
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
