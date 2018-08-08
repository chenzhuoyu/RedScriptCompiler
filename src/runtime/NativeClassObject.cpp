#include "utils/Decimal.h"
#include "utils/Integer.h"
#include "utils/Strings.h"

#include "runtime/IntObject.h"
#include "runtime/BoolObject.h"
#include "runtime/ProxyObject.h"
#include "runtime/StringObject.h"
#include "runtime/DecimalObject.h"
#include "runtime/NativeClassObject.h"

namespace RedScript::Runtime
{
/* type object for native class */
TypeRef NativeClassTypeObject;

/* wrapped primitive FFI types */
Reference<ForeignType> ForeignVoidTypeObject;
Reference<ForeignType> ForeignInt8TypeObject;
Reference<ForeignType> ForeignUInt8TypeObject;
Reference<ForeignType> ForeignInt16TypeObject;
Reference<ForeignType> ForeignUInt16TypeObject;
Reference<ForeignType> ForeignInt32TypeObject;
Reference<ForeignType> ForeignUInt32TypeObject;
Reference<ForeignType> ForeignInt64TypeObject;
Reference<ForeignType> ForeignUInt64TypeObject;
Reference<ForeignType> ForeignFloatTypeObject;
Reference<ForeignType> ForeignDoubleTypeObject;
Reference<ForeignType> ForeignLongDoubleTypeObject;

/* raw pointer types */
Reference<ForeignType> ForeignRawPointerTypeObject;
Reference<ForeignType> ForeignConstRawPointerTypeObject;

/* string types */
Reference<ForeignType> ForeignCStringTypeObject;
Reference<ForeignType> ForeignConstCStringTypeObject;

static inline size_t align(size_t val, size_t to)
{
    return val ? ((val - 1) / to + 1) * to : 0;
}

static std::string type2str(TCCState *s, TCCType *t, const char *n)
{
    int id = tcc_type_get_id(t);
    std::string name = n ?: tcc_type_get_name(t);
    static std::unordered_set<TCCType *> ts;

    if (ts.find(t) != ts.end())
        return Utils::Strings::format("(%p...)", static_cast<void *>(t));
    ts.insert(t);

    if (!n || !strcmp(n, tcc_type_get_name(t)))
    {
        if (IS_ENUM(id))
            name = "enum " + name;
        else if (IS_UNION(id))
            name = "union " + name;
        else if (IS_STRUCT(id))
            name = "struct " + name;
    }
    else
    {
        name += " (aka. \"";

        if (IS_ENUM(id))
            name += "enum ";
        else if (IS_UNION(id))
            name += "union ";
        else if (IS_STRUCT(id))
            name += "struct ";

        name += tcc_type_get_name(t);
        name += "\")";
    }

    if (IS_PTR(id))
    {
        name += "/";
        name += type2str(s, tcc_type_get_ref(t), nullptr);
    }

    else if (IS_ENUM(id))
    {
        name += " { ";
        tcc_type_list_items(s, t, [](TCCState *s, TCCType *t, const char *name, long long val, void *pn) -> char
        {
            *(std::string *)pn += name;
            *(std::string *)pn += " = ";
            *(std::string *)pn += std::to_string(val);
            *(std::string *)pn += ", ";
            return 1;
        }, &name);
        name += "}";
    }

    else if (IS_UNION(id) || IS_STRUCT(id))
    {
        name += " { ";
        tcc_type_list_fields(s, t, [](TCCState *s, TCCType *t, const char *name, TCCType *type, void *pn) -> char
        {
            *(std::string *)pn += type2str(s, type, nullptr);
            *(std::string *)pn += " ";
            *(std::string *)pn += name;
            *(std::string *)pn += "; ";
            return 1;
        }, &name);
        name += "}";
    }

    ts.erase(t);
    return Utils::Strings::format("(%p)%s", static_cast<void *>(t), name);
}

std::string NativeClassType::nativeObjectRepr(ObjectRef self)
{
    return Utils::Strings::format(
        "<native class \"%s\" at %p>",
        self.as<NativeClassObject>()->name(),
        static_cast<void *>(self.get())
    );
}

NativeClassObject::~NativeClassObject()
{
    attrs().clear();
    tcc_delete(_tcc);
}

NativeClassObject::NativeClassObject(
    const std::string &name,
    const std::string &code,
    Runtime::Reference<Runtime::MapObject> &&options
) : Object(NativeClassTypeObject), _tcc(tcc_new()), _name(name)
{
    /* convert all items to pair vector */
    options->enumerate([&](ObjectRef key, ObjectRef value)
    {
        /* key must be string */
        if (key->isNotInstanceOf(StringTypeObject))
            throw Exceptions::InternalError("Non-string keys");

        /* value must be string either */
        if (value->isNotInstanceOf(StringTypeObject))
            throw Exceptions::InternalError("Non-string values");

        /* add to options list */
        _options.emplace_back(key.as<StringObject>()->value(), value.as<StringObject>()->value());
        return true;
    });

    /* initialize tcc */
    tcc_set_options(_tcc, "-nostdlib");
    tcc_set_output_type(_tcc, TCC_OUTPUT_MEMORY);

    /* error output function */
    tcc_set_error_func(_tcc, this, [](TCCState *s, int isWarning, const char *file, int line, const char *message, void *ctx)
    {
        NativeClassObject *self = static_cast<NativeClassObject *>(ctx);
        self->_errors.emplace_back(file ?: "(\?\?)", line, (isWarning != 0), message);
    });

    /* compile the code */
    if (tcc_compile_string_ex(_tcc, "<native>", code.data(), code.size()) < 0)
        for (const auto &ctx : _errors)
            if (!(ctx->isWarning()))
                throw ctx;

    /* link the code in memory */
    if (tcc_relocate(_tcc) < 0)
        throw Exceptions::RuntimeError("Unable to link native object in memory");

    /* add all the types */
    tcc_list_types(_tcc, [](TCCState *s, const char *name, TCCType *type, void *self) -> char
    {
        static_cast<NativeClassObject *>(self)->addType(name, type);
        return 1;
    }, this);

    /* add all the functions */
    tcc_list_functions(_tcc, [](TCCState *s, const char *name, TCCFunction *func, void *self) -> char
    {
        static_cast<NativeClassObject *>(self)->addFunction(name, func);
        return 1;
    }, this);
}

void NativeClassObject::addType(const char *name, TCCType *type)
{
    printf("* TYPE :: %s\n", type2str(_tcc, type, name).c_str());
}

void NativeClassObject::addFunction(const char *name, TCCFunction *func)
{
    auto fname = name ?: tcc_function_get_name(func);
    addObject(name, Object::newObject<ForeignFunction>(this, _tcc, fname, func));
}

Reference<ForeignType> NativeClassObject::makeForeignType(TCCType *type)
{
    /* type ID */
    int id = tcc_type_get_id(type);
    decltype(_types.find(type)) iter;

    /* convert types */
    switch (id & VT_BTYPE)
    {
        /* primitive types */
        case VT_VOID    : return ForeignVoidTypeObject;
        case VT_BOOL    : return ForeignInt8TypeObject;
        case VT_FLOAT   : return ForeignFloatTypeObject;
        case VT_DOUBLE  : return ForeignDoubleTypeObject;
        case VT_LDOUBLE : return ForeignLongDoubleTypeObject;
        case VT_BYTE    : return (id & VT_UNSIGNED) ? ForeignUInt8TypeObject  : ForeignInt8TypeObject;
        case VT_SHORT   : return (id & VT_UNSIGNED) ? ForeignUInt16TypeObject : ForeignInt16TypeObject;
        case VT_INT     : return (id & VT_UNSIGNED) ? ForeignUInt32TypeObject : ForeignInt32TypeObject;
        case VT_LLONG   : return (id & VT_UNSIGNED) ? ForeignUInt64TypeObject : ForeignInt64TypeObject;

        /* pointers */
        case VT_PTR:
        {
            /* pointer base type */
            auto base = tcc_type_get_ref(type);
            auto typeId = tcc_type_get_id(base);
            bool isConst = (typeId & VT_CONSTANT) != 0;

            /* special case for "char *" and "const char *" */
            if ((typeId & VT_BTYPE) == VT_BYTE)
                return isConst ? ForeignConstCStringTypeObject : ForeignCStringTypeObject;

            /* special case for "void *" and "const void *" */
            if ((typeId & VT_BTYPE) == VT_VOID)
                return isConst ? ForeignConstRawPointerTypeObject : ForeignRawPointerTypeObject;

            /* read from cache if possible */
            if ((iter = _types.find(type)) != _types.end())
                return iter->second;

            /* wrap with pointer type */
            auto btype = makeForeignType(base);
            auto result = Object::newObject<ForeignPointerType>(std::move(btype), isConst);

            /* add to type cache */
            _types.emplace(type, result);
            return std::move(result);
        }

        /* 128-bit integers */
        case VT_QLONG:
            throw Exceptions::TypeError("FFI currently doesn't support 128-bit integers");

        /* 128-bit floating point numbers */
        case VT_QFLOAT:
            throw Exceptions::TypeError("FFI currently doesn't support 128-bit floating point numbers");

        /* function pointers */
        case VT_FUNC:
        {
            // TODO: implement VT_FUNC
            throw Exceptions::InternalError("not implemented: VT_FUNC");
        }

        /* structs */
        case VT_STRUCT:
        {
            // TODO: implement VT_STRUCT
            throw Exceptions::InternalError("not implemented: VT_STRUCT");
        }

        /* other unknown types */
        default:
        {
            throw Exceptions::InternalError(Utils::Strings::format(
                "Unknown type ID: %#08x",
                static_cast<uint32_t>(id)
            ));
        }
    }
}

void NativeClassObject::shutdown(void)
{
    /* clear FFI types */
    ForeignVoidTypeObject            = nullptr;
    ForeignInt8TypeObject            = nullptr;
    ForeignUInt8TypeObject           = nullptr;
    ForeignInt16TypeObject           = nullptr;
    ForeignUInt16TypeObject          = nullptr;
    ForeignInt32TypeObject           = nullptr;
    ForeignUInt32TypeObject          = nullptr;
    ForeignInt64TypeObject           = nullptr;
    ForeignUInt64TypeObject          = nullptr;
    ForeignFloatTypeObject           = nullptr;
    ForeignDoubleTypeObject          = nullptr;
    ForeignLongDoubleTypeObject      = nullptr;

    /* clear raw pointer types */
    ForeignRawPointerTypeObject      = nullptr;
    ForeignConstRawPointerTypeObject = nullptr;

    /* clear string types */
    ForeignCStringTypeObject         = nullptr;
    ForeignConstCStringTypeObject    = nullptr;

    /* clear type instance */
    NativeClassTypeObject            = nullptr;
}

void NativeClassObject::initialize(void)
{
    /* native class type object */
    static NativeClassType nullType;
    NativeClassTypeObject            = Reference<NativeClassType>::refStatic(nullType);

    /* wrapped primitive FFI types */
    ForeignVoidTypeObject            = Object::newObject<ForeignVoidType>();
    ForeignInt8TypeObject            = Object::newObject<ForeignTypeFactory<ForeignInt8Type      , ForeignInt8      , int8_t     >>();
    ForeignUInt8TypeObject           = Object::newObject<ForeignTypeFactory<ForeignUInt8Type     , ForeignUInt8     , uint8_t    >>();
    ForeignInt16TypeObject           = Object::newObject<ForeignTypeFactory<ForeignInt16Type     , ForeignInt16     , int16_t    >>();
    ForeignUInt16TypeObject          = Object::newObject<ForeignTypeFactory<ForeignUInt16Type    , ForeignUInt16    , uint16_t   >>();
    ForeignInt32TypeObject           = Object::newObject<ForeignTypeFactory<ForeignInt32Type     , ForeignInt32     , int32_t    >>();
    ForeignUInt32TypeObject          = Object::newObject<ForeignTypeFactory<ForeignUInt32Type    , ForeignUInt32    , uint32_t   >>();
    ForeignInt64TypeObject           = Object::newObject<ForeignTypeFactory<ForeignInt64Type     , ForeignInt64     , int64_t    >>();
    ForeignUInt64TypeObject          = Object::newObject<ForeignTypeFactory<ForeignUInt64Type    , ForeignUInt64    , uint64_t   >>();
    ForeignFloatTypeObject           = Object::newObject<ForeignTypeFactory<ForeignFloatType     , ForeignFloat     , float      >>();
    ForeignDoubleTypeObject          = Object::newObject<ForeignTypeFactory<ForeignDoubleType    , ForeignDouble    , double     >>();
    ForeignLongDoubleTypeObject      = Object::newObject<ForeignTypeFactory<ForeignLongDoubleType, ForeignLongDouble, long double>>();

    /* raw pointer types */
    ForeignRawPointerTypeObject      = Object::newObject<ForeignPointerType>(false);
    ForeignConstRawPointerTypeObject = Object::newObject<ForeignPointerType>(true);

    /* string types */
    ForeignCStringTypeObject         = Object::newObject<ForeignTypeFactory<ForeignCStringType, ForeignStringBuffer, const std::string &>>(false);
    ForeignConstCStringTypeObject    = Object::newObject<ForeignCStringType>(true);
}

/** Abstract Foreign Type **/

static ObjectRef foreignTypeValueGetter(ObjectRef self, ObjectRef type)
{
    /* convert to root foreign instance */
    Object *obj = self.get();
    ForeignInstance *fvalue = dynamic_cast<ForeignInstance *>(obj);

    /* foreign type check */
    if (fvalue == nullptr)
    {
        throw Exceptions::TypeError(Utils::Strings::format(
            "Argument must be a foreign instance, not \"%s\"",
            self->type()->name()
        ));
    }

    /* read it's value */
    return fvalue->get();
}

static ObjectRef foreignTypeValueSetter(ObjectRef self, ObjectRef type, ObjectRef value)
{
    /* convert to root foreign instance */
    Object *obj = self.get();
    ForeignInstance *fvalue = dynamic_cast<ForeignInstance *>(obj);

    /* foreign type check */
    if (fvalue == nullptr)
    {
        throw Exceptions::TypeError(Utils::Strings::format(
            "Argument must be a foreign instance, not \"%s\"",
            self->type()->name()
        ));
    }

    /* set it's value */
    fvalue->set(value);
    return std::move(value);
}

ForeignType::ForeignType(const std::string &name, ffi_type *type) : NativeType(name), _size(type->size), _ftype(type)
{
    addObject("value", ProxyObject::newReadWrite(
        NativeFunctionObject::newBinary("__get__", &foreignTypeValueGetter),
        NativeFunctionObject::newTernary("__set__", &foreignTypeValueSetter)
    ));
}

std::string ForeignType::nativeObjectRepr(ObjectRef self)
{
    auto obj = self.as<ForeignInstance>()->get();
    return Utils::Strings::format("ffi.%s(%s)", name(), obj->type()->objectRepr(obj));
}

void ForeignType::nativeObjectDefineSubclass(TypeRef self, TypeRef type)
{
    /* FFI types contains pointers, no sub-classes allowed */
    throw Exceptions::TypeError("Cannot create subclass of FFI types");
}

/** Primitive Type Packers **/

void ForeignVoidType::pack(void *buffer, size_t size, ObjectRef value) const
{
    /* value check */
    if (value.isNotIdenticalWith(NullObject))
    {
        throw Exceptions::ValueError(Utils::Strings::format(
            "Value must be \"null\", not \"%s\" object",
            value->type()->name()
        ));
    };
}

#define FFI_PACK_INT(bits)                                                              \
{                                                                                       \
    /* type size check */                                                               \
    if (size < sizeof(int ## bits ## _t))                                               \
        throw Exceptions::InternalError("Insufficient space for \"int" #bits "_t\"");   \
                                                                                        \
    /* value check */                                                                   \
    if (value->isNotInstanceOf(IntTypeObject))                                          \
    {                                                                                   \
        throw Exceptions::TypeError(Utils::Strings::format(                             \
            "Value must be an integer, not \"%s\"",                                     \
            value->type()->name()                                                       \
        ));                                                                             \
    };                                                                                  \
                                                                                        \
    /* convert to integer */                                                            \
    int ## bits ## _t data;                                                             \
    const Utils::Integer &val = value.as<IntObject>()->value();                         \
                                                                                        \
    /* value range check */                                                             \
    if ((val < INT ## bits ## _MIN) || (val > INT ## bits ## _MAX))                     \
        throw Exceptions::ValueError("Integer value out of range: int" #bits "_t");     \
                                                                                        \
    /* copy to buffer */                                                                \
    data = static_cast<int ## bits ## _t>(val.toInt());                                 \
    memcpy(buffer, &data, sizeof(int ## bits ## _t));                                   \
}

#define FFI_PACK_UINT(bits)                                                             \
{                                                                                       \
    /* type size check */                                                               \
    if (size < sizeof(uint ## bits ## _t))                                              \
        throw Exceptions::InternalError("Insufficient space for \"uint" #bits "_t\"");  \
                                                                                        \
    /* value check */                                                                   \
    if (value->isNotInstanceOf(IntTypeObject))                                          \
    {                                                                                   \
        throw Exceptions::TypeError(Utils::Strings::format(                             \
            "Value must be an integer, not \"%s\"",                                     \
            value->type()->name()                                                       \
        ));                                                                             \
    };                                                                                  \
                                                                                        \
    /* convert to integer */                                                            \
    uint ## bits ## _t data;                                                            \
    const Utils::Integer &val = value.as<IntObject>()->value();                         \
                                                                                        \
    /* value range check */                                                             \
    if (val > UINT ## bits ## _MAX)                                                     \
        throw Exceptions::ValueError("Integer value out of range: uint" #bits "_t");    \
                                                                                        \
    /* copy to buffer */                                                                \
    data = static_cast<uint ## bits ## _t>(val.toUInt());                               \
    memcpy(buffer, &data, sizeof(uint ## bits ## _t));                                  \
}

#define FFI_PACK_FLOAT(vtype, dtype)                                                    \
{                                                                                       \
    /* type size check */                                                               \
    if (size < sizeof(vtype))                                                           \
        throw Exceptions::RuntimeError("Insufficient space for \"" #vtype "\"");        \
                                                                                        \
    /* value check */                                                                   \
    if (value->isNotInstanceOf(DecimalTypeObject))                                      \
    {                                                                                   \
        throw Exceptions::TypeError(Utils::Strings::format(                             \
            "Value must be a decimal, not \"%s\"",                                      \
            value->type()->name()                                                       \
        ));                                                                             \
    };                                                                                  \
                                                                                        \
    /* convert to integer */                                                            \
    vtype data;                                                                         \
    const Utils::Decimal &val = value.as<DecimalObject>()->value();                     \
                                                                                        \
    /* value range check */                                                             \
    if (!(val.isSafe ## dtype ()))                                                      \
        throw Exceptions::ValueError("Decimal value out of range: " #vtype);            \
                                                                                        \
    /* copy to buffer */                                                                \
    data = val.to ## dtype();                                                           \
    memcpy(buffer, &data, sizeof(vtype));                                               \
}

void ForeignInt8Type ::pack(void *buffer, size_t size, ObjectRef value) const FFI_PACK_INT(8)
void ForeignInt16Type::pack(void *buffer, size_t size, ObjectRef value) const FFI_PACK_INT(16)
void ForeignInt32Type::pack(void *buffer, size_t size, ObjectRef value) const FFI_PACK_INT(32)
void ForeignInt64Type::pack(void *buffer, size_t size, ObjectRef value) const FFI_PACK_INT(64)

void ForeignUInt8Type ::pack(void *buffer, size_t size, ObjectRef value) const FFI_PACK_UINT(8)
void ForeignUInt16Type::pack(void *buffer, size_t size, ObjectRef value) const FFI_PACK_UINT(16)
void ForeignUInt32Type::pack(void *buffer, size_t size, ObjectRef value) const FFI_PACK_UINT(32)
void ForeignUInt64Type::pack(void *buffer, size_t size, ObjectRef value) const FFI_PACK_UINT(64)

void ForeignFloatType     ::pack(void *buffer, size_t size, ObjectRef value) const FFI_PACK_FLOAT(float      , Float     )
void ForeignDoubleType    ::pack(void *buffer, size_t size, ObjectRef value) const FFI_PACK_FLOAT(double     , Double    )
void ForeignLongDoubleType::pack(void *buffer, size_t size, ObjectRef value) const FFI_PACK_FLOAT(long double, LongDouble)

#undef FFI_PACK_INT
#undef FFI_PACK_UINT
#undef FFI_PACK_FLOAT

/** Primitive Type Unpackers **/

#define FFI_UNPACK_INT(bits)                                                            \
{                                                                                       \
    /* unpack as integer */                                                             \
    if (size < sizeof(int ## bits ## _t))                                               \
        throw Exceptions::RuntimeError("EOF when unpacking \"int" #bits "_t\"");        \
    else                                                                                \
        return IntObject::fromInt(*(static_cast<const int ## bits ## _t *>(buffer)));   \
}

#define FFI_UNPACK_UINT(bits)                                                           \
{                                                                                       \
    /* unpack as integer */                                                             \
    if (size < sizeof(uint ## bits ## _t))                                              \
        throw Exceptions::RuntimeError("EOF when unpacking \"uint" #bits "_t\"");       \
    else                                                                                \
        return IntObject::fromUInt(*(static_cast<const uint ## bits ## _t *>(buffer))); \
}

#define FFI_UNPACK_FLOAT(vtype, dtype)                                                  \
{                                                                                       \
    /* unpack as decimal */                                                             \
    if (size < sizeof(vtype))                                                           \
        throw Exceptions::RuntimeError("EOF when unpacking \"" #vtype "\"");            \
    else                                                                                \
        return DecimalObject::from ## dtype(*(static_cast<const vtype *>(buffer)));     \
}

ObjectRef ForeignInt8Type ::unpack(const void *buffer, size_t size) const FFI_UNPACK_INT(8)
ObjectRef ForeignInt16Type::unpack(const void *buffer, size_t size) const FFI_UNPACK_INT(16)
ObjectRef ForeignInt32Type::unpack(const void *buffer, size_t size) const FFI_UNPACK_INT(32)
ObjectRef ForeignInt64Type::unpack(const void *buffer, size_t size) const FFI_UNPACK_INT(64)

ObjectRef ForeignUInt8Type ::unpack(const void *buffer, size_t size) const FFI_UNPACK_UINT(8)
ObjectRef ForeignUInt16Type::unpack(const void *buffer, size_t size) const FFI_UNPACK_UINT(16)
ObjectRef ForeignUInt32Type::unpack(const void *buffer, size_t size) const FFI_UNPACK_UINT(32)
ObjectRef ForeignUInt64Type::unpack(const void *buffer, size_t size) const FFI_UNPACK_UINT(64)

ObjectRef ForeignFloatType     ::unpack(const void *buffer, size_t size) const FFI_UNPACK_FLOAT(float, Float)
ObjectRef ForeignDoubleType    ::unpack(const void *buffer, size_t size) const FFI_UNPACK_FLOAT(double, Double)
ObjectRef ForeignLongDoubleType::unpack(const void *buffer, size_t size) const FFI_UNPACK_FLOAT(long double, LongDouble)

#undef FFI_UNPACK_INT
#undef FFI_UNPACK_UINT
#undef FFI_UNPACK_FLOAT

/** Foreign Pointer Type Implementations **/

void ForeignPointerType::pack(void *buffer, size_t size, ObjectRef value) const
{
    /* type size check */
    if (size < sizeof(void *))
        throw Exceptions::InternalError("Insufficient space for pointers");

    /* convert to raw buffer */
    Object *obj = value.get();
    ForeignRawBuffer *frb = dynamic_cast<ForeignRawBuffer *>(obj);

    /* buffer type check */
    if (frb == nullptr)
    {
        throw Exceptions::TypeError(Utils::Strings::format(
            "Argument must be a buffer, not \"%s\"",
            obj->type()->name()
        ));
    }

    /* pack the pointer address */
    *(reinterpret_cast<void **>(buffer)) = frb->buffer();
}

ObjectRef ForeignPointerType::unpack(const void *buffer, size_t size) const
{
    /* type size check */
    if (size < sizeof(void *))
        throw Exceptions::RuntimeError("EOF when unpacking pointers");

    // TODO: implement unpack pointer
    throw Exceptions::InternalError("not implemented: unpack pointer");
}

std::string ForeignPointerType::nativeObjectRepr(ObjectRef self)
{
    return Utils::Strings::format(
        "ffi.%s(ptr=%p, size=%zu, auto_release=%s)",
        name(),
        self.as<ForeignRawBuffer>()->buffer(),
        self.as<ForeignRawBuffer>()->bufferSize(),
        self.as<ForeignRawBuffer>()->isAutoRelease() ? "true" : "false"
    );
}

/** Foreign Pointer Type Implementations **/

static ObjectRef foreignPointerAddrGetter(ObjectRef self, ObjectRef type)
{
    /* convert to raw buffer */
    Object *obj = self.get();
    ForeignRawBuffer *frb = dynamic_cast<ForeignRawBuffer *>(obj);

    /* object type check */
    if (frb == nullptr)
    {
        throw Exceptions::TypeError(Utils::Strings::format(
            "Argument must be a buffer, not \"%s\"",
            self->type()->name()
        ));
    }

    /* get it's address */
    return IntObject::fromUInt(reinterpret_cast<uintptr_t>(frb->buffer()));
}

static ObjectRef foreignPointerSizeGetter(ObjectRef self, ObjectRef type)
{
    /* convert to raw buffer */
    Object *obj = self.get();
    ForeignRawBuffer *frb = dynamic_cast<ForeignRawBuffer *>(obj);

    /* object type check */
    if (frb == nullptr)
    {
        throw Exceptions::TypeError(Utils::Strings::format(
            "Argument must be a buffer, not \"%s\"",
            self->type()->name()
        ));
    }

    /* get it's size */
    return IntObject::fromUInt(frb->bufferSize());
}

static ObjectRef foreignPointerAutoReleaseGetter(ObjectRef self, ObjectRef type)
{
    /* convert to raw buffer */
    Object *obj = self.get();
    ForeignRawBuffer *frb = dynamic_cast<ForeignRawBuffer *>(obj);

    /* object type check */
    if (frb == nullptr)
    {
        throw Exceptions::TypeError(Utils::Strings::format(
            "Argument must be a buffer, not \"%s\"",
            self->type()->name()
        ));
    }

    /* read it's value */
    return BoolObject::fromBool(frb->isAutoRelease());
}

static ObjectRef foreignPointerAutoReleaseSetter(ObjectRef self, ObjectRef type, ObjectRef value)
{
    /* convert to raw buffer */
    Object *obj = self.get();
    ForeignRawBuffer *frb = dynamic_cast<ForeignRawBuffer *>(obj);

    /* object type check */
    if (frb == nullptr)
    {
        throw Exceptions::TypeError(Utils::Strings::format(
            "Argument must be a buffer, not \"%s\"",
            self->type()->name()
        ));
    }

    /* set it's value */
    frb->setAutoRelease(value->type()->objectIsTrue(value));
    return std::move(value);
}

ForeignPointerType::ForeignPointerType(const std::string &name, Reference<ForeignType> base, bool isConst) :
    ForeignType(name, &ffi_type_pointer), _base(base), _isConst(isConst)
{
    /* addr and size properties */
    addObject("addr", ProxyObject::newReadOnly(NativeFunctionObject::newBinary("__get__", &foreignPointerAddrGetter)));
    addObject("size", ProxyObject::newReadOnly(NativeFunctionObject::newBinary("__get__", &foreignPointerSizeGetter)));

    /* auto release property */
    addObject("auto_release", ProxyObject::newReadWrite(
        NativeFunctionObject::newBinary("__get__", &foreignPointerAutoReleaseGetter),
        NativeFunctionObject::newTernary("__set__", &foreignPointerAutoReleaseSetter)
    ));
}

/** Foreign CString Type Implementations **/

ForeignCStringType::ForeignCStringType(bool isConst) :
    ForeignPointerType(isConst ? "const_char_p" : "char_p", ForeignInt8TypeObject, isConst)
{
    /* length property */
    addObject("length", ProxyObject::newReadOnly(NativeFunctionObject::newBinary("__get__", [](ObjectRef self, ObjectRef type)
    {
        /* convert to raw buffer */
        Object *obj = self.get();
        ForeignStringBuffer *fsb = dynamic_cast<ForeignStringBuffer *>(obj);

        /* object type check */
        if (fsb == nullptr)
        {
            throw Exceptions::TypeError(Utils::Strings::format(
                "Argument must be a string buffer, not \"%s\"",
                self->type()->name()
            ));
        }

        /* get it's length */
        return IntObject::fromUInt(fsb->length());
    })));
}

void ForeignCStringType::pack(void *buffer, size_t size, ObjectRef value) const
{
    /* type size check */
    if (size < sizeof(char *))
        throw Exceptions::InternalError("Insufficient space for pointers");

    /* const string, use the content directly */
    if (isConst() && value->isInstanceOf(StringTypeObject))
        *(reinterpret_cast<const char **>(buffer)) = value.as<StringObject>()->value().c_str();

    /* string buffer, use it's address directly */
    else if (value->isInstanceOf(ForeignCStringTypeObject) ||
             value->isInstanceOf(ForeignConstCStringTypeObject))
        *(reinterpret_cast<const char **>(buffer)) = value.as<ForeignStringBuffer>()->str();

    /* otherwise, pack as plain pointer */
    else
        ForeignPointerType::pack(buffer, size, std::move(value));
}

ObjectRef ForeignCStringType::unpack(const void *buffer, size_t size) const
{
    /* type size check */
    if (size < sizeof(const char *))
        throw Exceptions::RuntimeError("EOF when unpacking pointers");

    /* get the string pointer */
    auto sp = reinterpret_cast<char * const *>(buffer);
    auto *str = *sp;

    /* null pointer, unpack as null */
    if (str == nullptr)
        return NullObject;

    /* unpack as string, if constant */
    else if (isConst())
        return StringObject::fromString(str);

    /* non-const string pointer, wrap as string buffer */
    else
        return Object::newObject<ForeignStringBuffer>(str);
}

std::string ForeignCStringType::nativeObjectRepr(ObjectRef self)
{
    auto obj = self.as<ForeignStringBuffer>()->get();
    return Utils::Strings::format("ffi.%s(%s)", (isConst() ? "const_char_p" : "char_p"), obj->type()->objectRepr(obj));
}

/** Foreign Function Implementations **/

ForeignFunction::ForeignFunction(NativeClassObject *self, TCCState *s, const char *name, TCCFunction *func) :
    NativeFunctionObject(name, std::bind(
        &ForeignFunction::invoke,
        this,
        std::placeholders::_1,
        std::placeholders::_2
    )),
    _name(name),
    _func(tcc_function_get_addr(s, func)),
    _isVarg(tcc_function_is_variadic(func)),
    _rettype(self->makeForeignType(tcc_function_get_return_type(func)))
{
    /* reserve spaces */
    _argsf.resize(tcc_function_get_nargs(func));
    _argsp.resize(tcc_function_get_nargs(func));
    _argsv.resize(tcc_function_get_nargs(func));
    _argnames.resize(tcc_function_get_nargs(func));
    _argtypes.resize(tcc_function_get_nargs(func));

    /* parameter count check */
    if (_argtypes.size() > UINT32_MAX)
    {
        throw Exceptions::TypeError(Utils::Strings::format(
            "Too many arguments for function \"%s\"",
            _name
        ));
    }

    /* convert each type */
    for (size_t i = 0; i < _argtypes.size(); i++)
    {
        /* cache argument name and type */
        _argnames[i] = tcc_function_get_arg_name(func, i);
        _argtypes[i] = self->makeForeignType(tcc_function_get_arg_type(func, i));

        /* cache all the pointers */
        _argsv[i].resize(align(_argtypes[i]->size(), sizeof(void *)));
        _argsf[i] = _argtypes[i]->ftype();
        _argsp[i] = _argsv[i].data();
    }

    /* prepare FFI context */
    _ret.resize(align(_rettype->size(), sizeof(void *)));
    ffi_prep_cif(&_cif, FFI_DEFAULT_ABI, static_cast<uint32_t>(_argtypes.size()), _rettype->ftype(), _argsf.data());
}

ObjectRef ForeignFunction::invoke(Utils::NFI::VariadicArgs args, Utils::NFI::KeywordArgs kwargs)
{
    /* check for variadic arguments */
    if (_isVarg)
    {
        throw Exceptions::TypeError(Utils::Strings::format(
            "Variadic functions is not supported by native classes: %s",
            _name
        ));
    }

    /* build each argument */
    for (size_t i = 0; i < _argtypes.size(); i++)
    {
        /* keyword arguments have higher priority than positional arguments */
        auto &name = _argnames[i];
        ObjectRef arg = kwargs->pop(StringObject::fromStringInterned(name));

        /* no keyword arguments, try positional arguments */
        if (arg.isNull())
        {
            /* check for positional argument count */
            if (i >= args->size())
            {
                throw Exceptions::TypeError(Utils::Strings::format(
                    "Missing required argument \"%s\"",
                    _argnames[i]
                ));
            }

            /* use positional argument if any */
            arg = args->items()[i];
        }

        /* pack object into buffer */
        _argtypes[i]->pack(
            _argsv[i].data(),
            _argsv[i].size(),
            std::move(arg)
        );
    }

    /* invoke the function */
    ffi_call(
        &_cif,
        reinterpret_cast<void (*)(void)>(_func),
        static_cast<void *>(_ret.data()),
        _argsp.data()
    );

    /* unpack the result */
    return _rettype->unpack(_ret.data(), _ret.size());
}

/** Foreign Raw Buffer Implementations **/

ForeignRawBuffer::~ForeignRawBuffer()
{
    if (_free)
        Engine::Memory::free(_data);
}

ForeignRawBuffer::ForeignRawBuffer(TypeRef type, void *data, size_t size, bool autoRelease) : ForeignInstance(type)
{
    /* reference only, copy the pointer */
    if (!autoRelease)
    {
        _data = data;
        _size = size;
        _free = false;
    }
    else
    {
        _free = true;
        _size = size;
        _data = Engine::Memory::alloc(size);

        /* copy data as needed */
        if (data == nullptr)
            memset(_data, 0, _size);
        else
            memcpy(_data, data, _size);
    }
}

void ForeignRawBuffer::set(ObjectRef value)
{
    /* cannot set void pointers */
    throw Exceptions::TypeError("Cannot set value of raw pointers");
}

ObjectRef ForeignRawBuffer::get(void) const
{
    /* cannot get void pointers */
    throw Exceptions::TypeError("Cannot get value of raw pointers");
}

/** Foreign String Buffer Implementations **/

ForeignStringBuffer::ForeignStringBuffer(char *value, size_t size) :
    ForeignRawBuffer(ForeignCStringTypeObject, value, sizeChecked(size) + 1, false)
{
    /* clear the last character (NULL terminator) */
    str()[size] = 0;
}

ForeignStringBuffer::ForeignStringBuffer(const std::string &value) :
    ForeignRawBuffer(ForeignCStringTypeObject, value.data(), sizeChecked(value.size()) + 1)
{
    /* clear the last character (NULL terminator) */
    str()[value.size()] = 0;
}

void ForeignStringBuffer::set(ObjectRef value)
{
    /* type check */
    if (value->isNotInstanceOf(StringTypeObject))
    {
        throw Exceptions::TypeError(Utils::Strings::format(
            "Argument must be a string, not \"%s\"",
            value->type()->name()
        ));
    }

    /* read the string value */
    auto obj = value.as<StringObject>();
    auto &val = obj->value();

    /* cannot set strings that length is unknown */
    if (bufferSize() == 0)
        throw Exceptions::RuntimeError("Cannot set value of a string with unknwon size, use slice instead");

    /* check for boundary */
    if (bufferSize() <= val.size())
    {
        throw Exceptions::IndexError(Utils::Strings::format(
            "String length exceeds maximum buffer size (%zu): %zu",
            length(),
            val.size()
        ));
    }

    /* overwrite the string buffer */
    str()[val.size()] = 0;
    memcpy(buffer(), val.data(), val.size());
}

ObjectRef ForeignStringBuffer::get(void) const
{
    /* cannot get strings that length is unknown */
    if (bufferSize() == 0)
        throw Exceptions::RuntimeError("Cannot get value of a string with unknwon size, use slice instead");
    else
        return StringObject::fromString(std::string(str(), str() + length()));
}
}
