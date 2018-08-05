#ifndef REDSCRIPT_RUNTIME_NATIVECLASSOBJECT_H
#define REDSCRIPT_RUNTIME_NATIVECLASSOBJECT_H

#include <vector>
#include <string>
#include <cstdint>
#include <functional>

#include <libtcc.h>
#include <ffi/ffi.h>

#include "runtime/Object.h"
#include "runtime/MapObject.h"
#include "runtime/ExceptionObject.h"
#include "runtime/NativeFunctionObject.h"

namespace RedScript::Runtime
{
class NativeClassType : public NativeType
{
public:
    explicit NativeClassType() : NativeType("native_class") {}

protected:
    virtual void addBuiltins(void) override {}
    virtual void clearBuiltins(void) override {}

/*** Native Object Protocol ***/

public:
    virtual std::string nativeObjectRepr(ObjectRef self) override;

};

/* type object for native class */
extern TypeRef NativeClassTypeObject;

class NativeClassObject : public Object
{
    TCCState *_tcc;
    std::string _code;
    std::string _name;
    std::vector<Exceptions::NativeSyntaxError> _errors;
    std::vector<std::pair<std::string, std::string>> _options;

public:
    virtual ~NativeClassObject();
    explicit NativeClassObject(
        const std::string &name,
        const std::string &code,
        Runtime::Reference<Runtime::MapObject> &&options
    );

public:
    const std::string &name(void) const { return _name; }
    const std::vector<Exceptions::NativeSyntaxError> &errors(void) const { return _errors; }

public:
    static void shutdown(void);
    static void initialize(void);

};

struct ForeignType : public NativeType
{
    enum class Type
    {
        Enum,
        Value,
        Struct,
        Pointer,
    };

private:
    Type _type;
    size_t _size;
    ffi_type *_ftype;

public:
    typedef std::function<void(ObjectRef, void *, size_t)> BoxerFunc;
    typedef std::function<ObjectRef(const void *, size_t)> UnboxerFunc;

private:
    BoxerFunc _boxer;
    UnboxerFunc _unboxer;

private:
    Reference<ForeignType> _ref;
    std::unordered_map<std::string, int64_t> _values;
    std::unordered_map<std::string, Reference<ForeignType>> _fields;

public:
    explicit ForeignType(const std::string &name, ffi_type *type, BoxerFunc &&boxer, UnboxerFunc &&unboxer) :
        NativeType(name),
        _type     (Type::Value),
        _size     (type->size),
        _ftype    (type),
        _boxer    (std::move(boxer)),
        _unboxer  (std::move(unboxer)) {}

public:
    explicit ForeignType(const std::string &name, Reference<ForeignType> ref) :
        NativeType(name),
        _ref      (ref),
        _type     (Type::Pointer),
        _size     (sizeof(void *)),
        _ftype    (&ffi_type_pointer) {}

public:
    Type type(void) const { return _type; }
    size_t size(void) const { return _size; }
    ffi_type *ftype(void) const { return _ftype; }

public:
    const auto &boxer(void) const { return _boxer; }
    const auto &unboxer(void) const { return _unboxer; }

public:
    const auto &ref(void)  const   { return _ref; }
    const auto &fields(void) const { return _fields; }
    const auto &values(void) const { return _values; }

public:
    static Reference<ForeignType> buildFrom(TCCType *type);

};

/* wrapped FFI types */
extern Reference<ForeignType> ForeignVoidTypeObject;
extern Reference<ForeignType> ForeignInt8TypeObject;
extern Reference<ForeignType> ForeignUInt8TypeObject;
extern Reference<ForeignType> ForeignInt16TypeObject;
extern Reference<ForeignType> ForeignUInt16TypeObject;
extern Reference<ForeignType> ForeignInt32TypeObject;
extern Reference<ForeignType> ForeignUInt32TypeObject;
extern Reference<ForeignType> ForeignInt64TypeObject;
extern Reference<ForeignType> ForeignUInt64TypeObject;
extern Reference<ForeignType> ForeignFloatTypeObject;
extern Reference<ForeignType> ForeignDoubleTypeObject;
extern Reference<ForeignType> ForeignLongDoubleTypeObject;

class ForeignInstance : public Object
{
    void *_buffer;

public:
    virtual ~ForeignInstance() { std::free(_buffer); }
    explicit ForeignInstance(TypeRef type) : Object(type), _buffer(std::malloc(type.as<ForeignType>()->size())) {}

};

class ForeignFunction : public NativeFunctionObject
{
    void *_func;
    ffi_cif _cif;

private:
    bool _isVarg;
    std::string _name;

public:
    Reference<ForeignType> _rettype;
    std::vector<std::pair<std::string, Reference<ForeignType>>> _argtypes;

public:
    virtual ~ForeignFunction() = default;
    explicit ForeignFunction(TCCState *s, const char *name, TCCFunction *func);

public:
    ObjectRef invoke(Utils::NFI::VariadicArgs args, Utils::NFI::KeywordArgs kwargs);

};
}

#endif /* REDSCRIPT_RUNTIME_NATIVECLASSOBJECT_H */
