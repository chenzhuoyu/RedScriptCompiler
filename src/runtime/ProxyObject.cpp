#include "runtime/ProxyObject.h"

namespace RedScript::Runtime
{
/* type object for proxy */
TypeRef ProxyTypeObject;

void ProxyObject::initialize(void)
{
    /* proxy type object */
    static ProxyType proxyType;
    ProxyTypeObject = Reference<ProxyType>::refStatic(proxyType);
}
}
