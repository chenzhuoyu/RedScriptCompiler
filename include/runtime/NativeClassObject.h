#ifndef REDSCRIPT_RUNTIME_NATIVECLASSOBJECT_H
#define REDSCRIPT_RUNTIME_NATIVECLASSOBJECT_H

#include <vector>
#include <string>
#include <cstdint>
#include <libtcc.h>

#include "runtime/Object.h"
#include "runtime/MapObject.h"
#include "exceptions/NativeSyntaxError.h"

namespace RedScript::Runtime
{
class NativeClassType : public NativeType
{
public:
    explicit NativeClassType() : NativeType("native_class") {}

protected:
    virtual void addBuiltins(void) override {}
    virtual void clearBuiltins(void) override {}

};

/* type object for native class */
extern TypeRef NativeClassTypeObject;

class NativeClassObject : public Object
{
    TCCState *_tcc;
    std::string _code;
    std::vector<Exceptions::NativeSyntaxError> _errors;
    std::vector<std::pair<std::string, std::string>> _options;

public:
    virtual ~NativeClassObject() { tcc_delete(_tcc); }
    explicit NativeClassObject(const std::string &code, Runtime::Reference<Runtime::MapObject> &&options);

public:
    const std::vector<Exceptions::NativeSyntaxError> &errors(void) const { return _errors; }

public:
    static void shutdown(void) {}
    static void initialize(void);

};
}

#endif /* REDSCRIPT_RUNTIME_NATIVECLASSOBJECT_H */
