#ifndef REDSCRIPT_COMPILER_NATIVECLASSOBJECT_H
#define REDSCRIPT_COMPILER_NATIVECLASSOBJECT_H

#include <vector>
#include <string>
#include <cstdint>

#include "runtime/Object.h"

namespace RedScript::Runtime
{
class NativeClassType : public Type
{
public:
    explicit NativeClassType() : Type("_NativeClass") {}

};

/* type object for native class */
extern TypeRef NativeClassTypeObject;

class NativeClassObject : public Object
{
public:
    virtual ~NativeClassObject() = default;
    explicit NativeClassObject() : Object(NativeClassTypeObject) {}

public:
    static void shutdown(void) {}
    static void initialize(void);

};
}

#endif /* REDSCRIPT_COMPILER_NATIVECLASSOBJECT_H */
