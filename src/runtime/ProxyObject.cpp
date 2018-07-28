#include "runtime/ProxyObject.h"

namespace RedScript::Runtime
{
/* type object for proxy */
TypeRef ProxyTypeObject;

void ProxyObject::shutdown(void)
{
    /* clear type instance */
    ProxyTypeObject = nullptr;
}

void ProxyObject::initialize(void)
{
    /* proxy type object */
    static ProxyType proxyType;
    ProxyTypeObject = Reference<ProxyType>::refStatic(proxyType);
}
}
