#ifndef REDSCRIPT_RUNTIME_NATIVECLASSOBJECT_H
#define REDSCRIPT_RUNTIME_NATIVECLASSOBJECT_H

#include <vector>
#include <string>
#include <cstdint>
#include <functional>
#include <unordered_map>

#include <libtcc.h>
#include <ffi/ffi.h>

#include "utils/NFI.h"
#include "engine/Memory.h"

#include "runtime/Object.h"
#include "runtime/MapObject.h"
#include "runtime/NullObject.h"
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

class ForeignType;
class NativeClassObject : public Object
{
    TCCState *_tcc;
    std::string _code;
    std::string _name;
    std::vector<Exceptions::NativeSyntaxError> _errors;

private:
    std::unordered_map<TCCType *, Reference<ForeignType>> _types;
    std::unordered_map<std::string, Reference<NativeType>> _enums;

public:
    virtual ~NativeClassObject() { tcc_delete(_tcc); }
    explicit NativeClassObject(
        const std::string &name,
        const std::string &code,
        Runtime::Reference<Runtime::MapObject> &&options
    );

private:
    void addType(const char *name, TCCType *type);
    void addFunction(const char *name, TCCFunction *func);

private:
    friend class ForeignFunction;
    Reference<ForeignType> makeForeignType(TCCType *type);

public:
    const std::string &name(void) const { return _name; }
    const std::vector<Exceptions::NativeSyntaxError> &errors(void) const { return _errors; }

public:
    static void shutdown(void);
    static void initialize(void);

};

class ForeignType : public NativeType
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
    virtual void pack(void *buffer, size_t size, ObjectRef value) const = 0;
    virtual ObjectRef unpack(const void *buffer, size_t size) const = 0;

/** Native Object Protocol **/

public:
    virtual std::string nativeObjectRepr(ObjectRef self) override;
    virtual void nativeObjectDefineSubclass(TypeRef self, TypeRef type) override;

};

struct ForeignVoidType : public ForeignType
{
    virtual ~ForeignVoidType() = default;
    explicit ForeignVoidType() : ForeignType("void", &ffi_type_void) {}

public:
    virtual void pack(void *buffer, size_t size, ObjectRef value) const override;
    virtual ObjectRef unpack(const void *buffer, size_t size) const override { return NullObject; }

/** Native Object Protocol **/

public:
    virtual ObjectRef nativeObjectNew(TypeRef type, Reference<TupleObject> args, Reference<MapObject> kwargs) override
    {
        /* "void" type cannot be instaniated */
        throw Exceptions::TypeError("\"void\" type cannot be instaniated");
    }
};

#define FFI_MAKE_INT_TYPE(bits)                                                                         \
struct ForeignInt ## bits ## Type : public ForeignType                                                  \
{                                                                                                       \
    virtual ~ForeignInt ## bits ## Type() = default;                                                    \
    explicit ForeignInt ## bits ## Type() : ForeignType("int" #bits "_t", &ffi_type_sint ## bits) {}    \
                                                                                                        \
public:                                                                                                 \
    virtual void pack(void *buffer, size_t size, ObjectRef value) const override;                       \
    virtual ObjectRef unpack(const void *buffer, size_t size) const override;                           \
                                                                                                        \
}

#define FFI_MAKE_UINT_TYPE(bits)                                                                        \
struct ForeignUInt ## bits ## Type : public ForeignType                                                 \
{                                                                                                       \
    virtual ~ForeignUInt ## bits ## Type() = default;                                                   \
    explicit ForeignUInt ## bits ## Type() : ForeignType("uint" #bits "_t", &ffi_type_uint ## bits) {}  \
                                                                                                        \
public:                                                                                                 \
    virtual void pack(void *buffer, size_t size, ObjectRef value) const override;                       \
    virtual ObjectRef unpack(const void *buffer, size_t size) const override;                           \
                                                                                                        \
}

#define FFI_MAKE_FLOAT_TYPE(name, type, ftype)                                                          \
struct Foreign ## name ## Type : public ForeignType                                                     \
{                                                                                                       \
    virtual ~Foreign ## name ## Type() = default;                                                       \
    explicit Foreign ## name ## Type() : ForeignType(#type, &ffi_type_ ## ftype) {}                     \
                                                                                                        \
public:                                                                                                 \
    virtual void pack(void *buffer, size_t size, ObjectRef value) const override;                       \
    virtual ObjectRef unpack(const void *buffer, size_t size) const override;                           \
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
FFI_MAKE_FLOAT_TYPE(LongDouble, long_double, longdouble);

#undef FFI_MAKE_INT_TYPE
#undef FFI_MAKE_UINT_TYPE
#undef FFI_MAKE_FLOAT_TYPE

struct ForeignEnumType : public NativeType
{
    virtual ~ForeignEnumType() = default;
    explicit ForeignEnumType(const std::string &name) : NativeType(name) {}

public:
    virtual ObjectRef nativeObjectNew(TypeRef type, Reference<TupleObject> args, Reference<MapObject> kwargs) override
    {
        throw Exceptions::TypeError(Utils::Strings::format(
            "Cannot create instances of enum type \"%s\"",
            name()
        ));
    }
};

struct ForeignStructType : public ForeignType
{

};

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

/* raw pointer types */
extern Reference<ForeignType> ForeignRawPointerTypeObject;
extern Reference<ForeignType> ForeignConstRawPointerTypeObject;

/* string types */
extern Reference<ForeignType> ForeignCStringTypeObject;
extern Reference<ForeignType> ForeignConstCStringTypeObject;

class ForeignPointerType : public ForeignType
{
    bool _isConst;
    Reference<ForeignType> _base;

public:
    virtual ~ForeignPointerType() = default;
    explicit ForeignPointerType(const std::string &name, Reference<ForeignType> base, bool isConst);

public:
    explicit ForeignPointerType(bool isConst) : ForeignPointerType(ForeignVoidTypeObject, isConst) {}
    explicit ForeignPointerType(Reference<ForeignType> base, bool isConst) : ForeignPointerType(
        Utils::Strings::format((isConst ? "const_%s_p" : "%s_p"), base->name()),
        base,
        isConst
    ) {}

public:
    bool isConst(void) const { return _isConst; }
    size_t baseSize(void) const { return _base->size(); }
    Reference<ForeignType> &baseType(void) { return _base; }

public:
    virtual void pack(void *buffer, size_t size, ObjectRef value) const override;
    virtual ObjectRef unpack(const void *buffer, size_t size) const override;

/** Native Object Protocol **/

public:
    virtual std::string nativeObjectRepr(ObjectRef self) override;

public:
    static Reference<ForeignPointerType> ref(Reference<ForeignType> base)
    {
        if (base.isIdenticalWith(ForeignInt8TypeObject))
            return ForeignCStringTypeObject.as<ForeignPointerType>();
        else
            return Object::newObject<ForeignPointerType>(base, false);
    }

public:
    static Reference<ForeignPointerType> refConst(Reference<ForeignType> base)
    {
        if (base.isIdenticalWith(ForeignInt8TypeObject))
            return ForeignConstCStringTypeObject.as<ForeignPointerType>();
        else
            return Object::newObject<ForeignPointerType>(base, true);
    }
};

struct ForeignCStringType : public ForeignPointerType
{
    virtual ~ForeignCStringType() = default;
    explicit ForeignCStringType(bool isConst);

public:
    virtual void pack(void *buffer, size_t size, ObjectRef value) const override;
    virtual ObjectRef unpack(const void *buffer, size_t size) const override;

/** Native Object Protocol **/

public:
    virtual std::string nativeObjectRepr(ObjectRef self) override;

};

class ForeignInstance : public Object
{
    void *_data;
    size_t _size;
    ForeignType *_ftype;

public:
    virtual ~ForeignInstance() { Engine::Memory::free(_data); }
    explicit ForeignInstance(TypeRef type) : Object(type), _ftype(type.as<ForeignType>().get())
    {
        _size = _ftype->size();
        _data = Engine::Memory::alloc(_size);
    }

public:
    void *data(void) const { return _data; }
    size_t size(void) const { return _size; }
    ffi_type *ftype(void) const { return _ftype->ftype(); }

public:
    virtual void set(ObjectRef value) { _ftype->pack(_data, _size, value); }
    virtual ObjectRef get(void) const { return _ftype->unpack(_data, _size); }

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
    explicit ForeignFunction(NativeClassObject *self, TCCState *s, const char *name, TCCFunction *func);

public:
    ObjectRef invoke(Utils::NFI::VariadicArgs args, Utils::NFI::KeywordArgs kwargs);

};

#define FFI_MAKE_INSTANCE(name, type)                                                                                               \
struct Foreign ## name : public ForeignInstance                                                                                     \
{                                                                                                                                   \
    virtual ~Foreign ## name() = default;                                                                                           \
    explicit Foreign ## name(type value) : ForeignInstance(Foreign ## name ## TypeObject) { memcpy(data(), &value, sizeof(type)); } \
}

FFI_MAKE_INSTANCE(Int8 , int8_t );
FFI_MAKE_INSTANCE(Int16, int16_t);
FFI_MAKE_INSTANCE(Int32, int32_t);
FFI_MAKE_INSTANCE(Int64, int64_t);

FFI_MAKE_INSTANCE(UInt8 , uint8_t );
FFI_MAKE_INSTANCE(UInt16, uint16_t);
FFI_MAKE_INSTANCE(UInt32, uint32_t);
FFI_MAKE_INSTANCE(UInt64, uint64_t);

FFI_MAKE_INSTANCE(Float     , float      );
FFI_MAKE_INSTANCE(Double    , double     );
FFI_MAKE_INSTANCE(LongDouble, long double);

#undef FFI_MAKE_INSTANCE

class ForeignRawBuffer : public ForeignInstance
{
    bool _free;
    size_t _size;
    size_t _alloc;

public:
    virtual ~ForeignRawBuffer();
    explicit ForeignRawBuffer(size_t size) : ForeignRawBuffer(ForeignRawPointerTypeObject, size) {}
    explicit ForeignRawBuffer(TypeRef type, size_t size) : ForeignRawBuffer(type, nullptr, size, true) {}

public:
    explicit ForeignRawBuffer(TypeRef type, void *data, size_t size, bool autoRelease);
    explicit ForeignRawBuffer(TypeRef type, const void *data, size_t size) : ForeignRawBuffer(type, const_cast<void *>(data), size, true) {}

public:
    void *&buffer(void) const { return *(reinterpret_cast<void **>(data())); }
    size_t bufferSize(void) const { return _size; }
    size_t allocationSize(void) const { return _alloc; }

public:
    bool isAutoRelease(void) const { return _free; }
    void setAutoRelease(bool value) { _free = value; }
    void setRawBufferSize(size_t value) { _size = value; }

public:
    virtual void set(ObjectRef value) override;
    virtual ObjectRef get(void) const override;

};

class ForeignStringBuffer : public ForeignRawBuffer
{
    static inline size_t sizeChecked(size_t size)
    {
        /* buffer length check (though very unlikely) */
        if (size == SIZE_T_MAX)
            throw Exceptions::ValueError("Buffer too large");
        else
            return size;
    }

public:
    explicit ForeignStringBuffer(char *value) : ForeignRawBuffer(ForeignCStringTypeObject, value, 0, false) {}
    explicit ForeignStringBuffer(size_t size) : ForeignRawBuffer(ForeignCStringTypeObject, sizeChecked(size) + 1) { str()[size] = 0; }

public:
    explicit ForeignStringBuffer(char *value, size_t size);
    explicit ForeignStringBuffer(const std::string &value = "");

public:
    char *str(void) const { return reinterpret_cast<char *>(buffer()); }
    size_t length(void) const { return bufferSize() - 1; }

public:
    virtual void set(ObjectRef value) override;
    virtual ObjectRef get(void) const override;

};
}

#endif /* REDSCRIPT_RUNTIME_NATIVECLASSOBJECT_H */
