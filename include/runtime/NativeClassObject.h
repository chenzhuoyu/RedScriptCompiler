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
    size_t _size;
    ffi_type *_ftype;

public:
    virtual ~ForeignType() { attrs().clear(); }
    explicit ForeignType(const std::string &name, ffi_type *type);

public:
    size_t size(void) const { return _size; }
    ffi_type *ftype(void) const { return _ftype; }

public:
    virtual void pack(ObjectRef value, void *buffer, size_t size) const = 0;
    virtual void unpack(ObjectRef &value, const void *buffer, size_t size) const = 0;

public:
    static Reference<ForeignType> buildFrom(TCCType *type);

};

struct ForeignVoidType : public ForeignType
{
    virtual ~ForeignVoidType() = default;
    explicit ForeignVoidType() : ForeignType("void", &ffi_type_void) {}

public:
    virtual void pack(ObjectRef value, void *buffer, size_t size) const override;
    virtual void unpack(ObjectRef &value, const void *buffer, size_t size) const override;

};

#define FFI_MAKE_INT_TYPE(bits)                                                                         \
struct ForeignInt ## bits ## Type : public ForeignType                                                  \
{                                                                                                       \
    virtual ~ForeignInt ## bits ## Type() = default;                                                    \
    explicit ForeignInt ## bits ## Type() : ForeignType("int" #bits "_t", &ffi_type_sint ## bits) {}    \
                                                                                                        \
public:                                                                                                 \
    virtual void pack(ObjectRef value, void *buffer, size_t size) const override;                       \
    virtual void unpack(ObjectRef &value, const void *buffer, size_t size) const override;              \
                                                                                                        \
}

#define FFI_MAKE_UINT_TYPE(bits)                                                                        \
struct ForeignUInt ## bits ## Type : public ForeignType                                                 \
{                                                                                                       \
    virtual ~ForeignUInt ## bits ## Type() = default;                                                   \
    explicit ForeignUInt ## bits ## Type() : ForeignType("uint" #bits "_t", &ffi_type_uint ## bits) {}  \
                                                                                                        \
public:                                                                                                 \
    virtual void pack(ObjectRef value, void *buffer, size_t size) const override;                       \
    virtual void unpack(ObjectRef &value, const void *buffer, size_t size) const override;              \
                                                                                                        \
}

#define FFI_MAKE_FLOAT_TYPE(name, type, ftype)                                                          \
struct Foreign ## name ## Type : public ForeignType                                                     \
{                                                                                                       \
    virtual ~Foreign ## name ## Type() = default;                                                       \
    explicit Foreign ## name ## Type() : ForeignType(#ftype, &ffi_type_ ## ftype) {}                    \
                                                                                                        \
public:                                                                                                 \
    virtual void pack(ObjectRef value, void *buffer, size_t size) const override;                       \
    virtual void unpack(ObjectRef &value, const void *buffer, size_t size) const override;              \
                                                                                                        \
}

FFI_MAKE_INT_TYPE(8);
FFI_MAKE_INT_TYPE(16);
FFI_MAKE_INT_TYPE(32);
FFI_MAKE_INT_TYPE(64);

FFI_MAKE_UINT_TYPE(8);
FFI_MAKE_UINT_TYPE(16);
FFI_MAKE_UINT_TYPE(32);
FFI_MAKE_UINT_TYPE(64);

FFI_MAKE_FLOAT_TYPE(Float, float, float);
FFI_MAKE_FLOAT_TYPE(Double, double, double);
FFI_MAKE_FLOAT_TYPE(LongDouble, long double, longdouble);

#undef FFI_MAKE_INT_TYPE
#undef FFI_MAKE_UINT_TYPE
#undef FFI_MAKE_FLOAT_TYPE

/* wrapped primitive FFI types */
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

class ForeignPointerType : public ForeignType
{
    bool _isConst;
    Reference<ForeignType> _base;

public:
    virtual ~ForeignPointerType() = default;
    explicit ForeignPointerType(Reference<ForeignType> base, bool isConst) :
        ForeignType(Utils::Strings::format("%s *", base->name()), &ffi_type_pointer),
        _base(base),
        _isConst(isConst){}

public:
    bool isConst(void) const { return _isConst; }
    size_t baseSize(void) const { return _base->size(); }
    Reference<ForeignType> &base(void) { return _base; }

public:
    virtual void pack(ObjectRef value, void *buffer, size_t size) const override;
    virtual void unpack(ObjectRef &value, const void *buffer, size_t size) const override;

};

struct ForeignCStringType : public ForeignPointerType
{
    virtual ~ForeignCStringType() = default;
    explicit ForeignCStringType(bool isConst) : ForeignPointerType(ForeignInt8TypeObject, isConst) {}

public:
    virtual void pack(ObjectRef value, void *buffer, size_t size) const override;
    virtual void unpack(ObjectRef &value, const void *buffer, size_t size) const override;

};

class ForeignInstance : public Object
{
    void *_data;
    size_t _size;

public:
    virtual ~ForeignInstance() { std::free(_data); }
    explicit ForeignInstance(TypeRef type) : Object(type), _size(type.as<ForeignType>()->size()) { _data = std::malloc(_size); }

public:
    void set(ObjectRef value) { type().as<ForeignType>()->pack(value, _data, _size); }
    void get(ObjectRef &value) { type().as<ForeignType>()->unpack(value, _data, _size); }

};

class ForeignFunction : public NativeFunctionObject
{
    void *_func;
    ffi_cif _cif;

private:
    bool _isVarg;
    std::string _name;

private:
    std::vector<char> _ret;
    std::vector<void *> _argsp;
    std::vector<ffi_type *> _argsf;
    std::vector<std::vector<char>> _argsv;

private:
    Reference<ForeignType> _rettype;
    std::vector<std::string> _argnames;
    std::vector<Reference<ForeignType>> _argtypes;

public:
    virtual ~ForeignFunction() = default;
    explicit ForeignFunction(TCCState *s, const char *name, TCCFunction *func);

public:
    ObjectRef invoke(Utils::NFI::VariadicArgs args, Utils::NFI::KeywordArgs kwargs);

};
}

#endif /* REDSCRIPT_RUNTIME_NATIVECLASSOBJECT_H */
