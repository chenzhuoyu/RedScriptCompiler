
#include <runtime/NativeClassObject.h>

#include "utils/Decimal.h"
#include "utils/Integer.h"
#include "utils/Strings.h"

#include "runtime/IntObject.h"
#include "runtime/NullObject.h"
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

static std::string type2str(TCCState *s, TCCType *t, const char *n)
{
    int id = tcc_type_get_id(t);
    std::string name = n ?: tcc_type_get_name(t);

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

    if (IS_ENUM(id))
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

    return name;
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
        printf("* TYPE :: %s\n", type2str(s, type, name).c_str());
        return 1;
    }, this);

    /* add all the functions */
    tcc_list_functions(_tcc, [](TCCState *s, const char *name, TCCFunction *func, void *self) -> char
    {
        static_cast<NativeClassObject *>(self)->addObject(name, Object::newObject<ForeignFunction>(s, name, func));
        return 1;
    }, this);
}

void NativeClassObject::shutdown(void)
{
    /* clear FFI types */
    ForeignVoidTypeObject        = nullptr;
    ForeignInt8TypeObject        = nullptr;
    ForeignUInt8TypeObject       = nullptr;
    ForeignInt16TypeObject       = nullptr;
    ForeignUInt16TypeObject      = nullptr;
    ForeignInt32TypeObject       = nullptr;
    ForeignUInt32TypeObject      = nullptr;
    ForeignInt64TypeObject       = nullptr;
    ForeignUInt64TypeObject      = nullptr;
    ForeignFloatTypeObject       = nullptr;
    ForeignDoubleTypeObject      = nullptr;
    ForeignLongDoubleTypeObject  = nullptr;

    /* clear type instance */
    NativeClassTypeObject = nullptr;
}

void NativeClassObject::initialize(void)
{
    /* native class type object */
    static NativeClassType nullType;
    NativeClassTypeObject = Reference<NativeClassType>::refStatic(nullType);

    /* wrapped primitive FFI types */
    ForeignVoidTypeObject       = Object::newObject<ForeignVoidType>();
    ForeignInt8TypeObject       = Object::newObject<ForeignInt8Type>();
    ForeignUInt8TypeObject      = Object::newObject<ForeignUInt8Type>();
    ForeignInt16TypeObject      = Object::newObject<ForeignInt16Type>();
    ForeignUInt16TypeObject     = Object::newObject<ForeignUInt16Type>();
    ForeignInt32TypeObject      = Object::newObject<ForeignInt32Type>();
    ForeignUInt32TypeObject     = Object::newObject<ForeignUInt32Type>();
    ForeignInt64TypeObject      = Object::newObject<ForeignInt64Type>();
    ForeignUInt64TypeObject     = Object::newObject<ForeignUInt64Type>();
    ForeignFloatTypeObject      = Object::newObject<ForeignFloatType>();
    ForeignDoubleTypeObject     = Object::newObject<ForeignDoubleType>();
    ForeignLongDoubleTypeObject = Object::newObject<ForeignLongDoubleType>();
}

static ObjectRef foreignTypeGetter(ObjectRef self, ObjectRef type)
{
    /* convert to root foreign instance */
    ObjectRef result;
    ForeignInstance *fvalue = dynamic_cast<ForeignInstance *>(self.get());

    /* foreign type check */
    if (fvalue == nullptr)
    {
        throw Exceptions::TypeError(Utils::Strings::format(
            "Must be a foreign instance, not \"%s\"",
            self->type()->name()
        ));
    }

    /* read it's value */
    fvalue->get(result);
    return std::move(result);
}

static ObjectRef foreignTypeSetter(ObjectRef self, ObjectRef type, ObjectRef value)
{
    /* convert to root foreign instance */
    Object *obj = self.get();
    ForeignInstance *fvalue = dynamic_cast<ForeignInstance *>(obj);

    /* foreign type check */
    if (fvalue == nullptr)
    {
        throw Exceptions::TypeError(Utils::Strings::format(
            "Must be a foreign instance, not \"%s\"",
            self->type()->name()
        ));
    }

    /* set it's value */
    fvalue->set(value);
    return std::move(value);
}

ForeignType::ForeignType(const std::string &name, ffi_type *type) : NativeType(name), _size(type->size), _ftype(type)
{
    attrs().emplace("value", ProxyObject::newReadWrite(
        NativeFunctionObject::newBinary("__get__", &foreignTypeGetter),
        NativeFunctionObject::newTernary("__set__", &foreignTypeSetter)
    ));
}

Reference<ForeignType> ForeignType::buildFrom(TCCType *type)
{
    /* type ID and name */
    int id = tcc_type_get_id(type);
    Reference<ForeignType> result = nullptr;

    /* convert types */
    switch (id & VT_BTYPE)
    {
        /* primitive types */
        case VT_VOID    : result = ForeignVoidTypeObject; break;
        case VT_BOOL    : result = ForeignInt8TypeObject; break;
        case VT_FLOAT   : result = ForeignFloatTypeObject; break;
        case VT_DOUBLE  : result = ForeignDoubleTypeObject; break;
        case VT_LDOUBLE : result = ForeignLongDoubleTypeObject; break;
        case VT_BYTE    : result = (id & VT_UNSIGNED) ? ForeignUInt8TypeObject  : ForeignInt8TypeObject ; break;
        case VT_SHORT   : result = (id & VT_UNSIGNED) ? ForeignUInt16TypeObject : ForeignInt16TypeObject; break;
        case VT_INT     : result = (id & VT_UNSIGNED) ? ForeignUInt32TypeObject : ForeignInt32TypeObject; break;
        case VT_LLONG   : result = (id & VT_UNSIGNED) ? ForeignUInt64TypeObject : ForeignInt64TypeObject; break;

        /* pointers */
        case VT_PTR:
        {
            /* pointer base type */
            auto base = tcc_type_get_ref(type);
            auto typeId = tcc_type_get_id(base);
            bool isConst = (typeId & VT_CONSTANT) != 0;

            /* special case for "char *" and "const char *" */
            if ((typeId & VT_BTYPE) == VT_BYTE)
            {
                result = Object::newObject<ForeignCStringType>(isConst);
                break;
            }

            /* wrap with pointer type */
            result = Object::newObject<ForeignPointerType>(buildFrom(base), isConst);
            break;
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
                "Unknown type code: %#08x",
                static_cast<uint32_t>(id)
            ));
        }
    }

    /* move to prevent copy */
    return std::move(result);
}

/** Primitive Type Packers **/

void ForeignVoidType::pack(ObjectRef value, void *buffer, size_t size) const
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

void ForeignInt8Type ::pack(ObjectRef value, void *buffer, size_t size) const FFI_PACK_INT(8)
void ForeignInt16Type::pack(ObjectRef value, void *buffer, size_t size) const FFI_PACK_INT(16)
void ForeignInt32Type::pack(ObjectRef value, void *buffer, size_t size) const FFI_PACK_INT(32)
void ForeignInt64Type::pack(ObjectRef value, void *buffer, size_t size) const FFI_PACK_INT(64)

void ForeignUInt8Type ::pack(ObjectRef value, void *buffer, size_t size) const FFI_PACK_UINT(8)
void ForeignUInt16Type::pack(ObjectRef value, void *buffer, size_t size) const FFI_PACK_UINT(16)
void ForeignUInt32Type::pack(ObjectRef value, void *buffer, size_t size) const FFI_PACK_UINT(32)
void ForeignUInt64Type::pack(ObjectRef value, void *buffer, size_t size) const FFI_PACK_UINT(64)

void ForeignFloatType     ::pack(ObjectRef value, void *buffer, size_t size) const FFI_PACK_FLOAT(float      , Float     )
void ForeignDoubleType    ::pack(ObjectRef value, void *buffer, size_t size) const FFI_PACK_FLOAT(double     , Double    )
void ForeignLongDoubleType::pack(ObjectRef value, void *buffer, size_t size) const FFI_PACK_FLOAT(long double, LongDouble)

#undef FFI_PACK_INT
#undef FFI_PACK_UINT
#undef FFI_PACK_FLOAT

/** Primitive Type Unpackers **/

void ForeignVoidType::unpack(ObjectRef &value, const void *buffer, size_t size) const
{
    /* type size check */
    if (size != 0)
        throw Exceptions::RuntimeError("\"void\" type size mismatch");

    /* unpack as null */
    value = NullObject;
}

#define FFI_UNPACK_INT(bits)                                                            \
{                                                                                       \
    /* type size check */                                                               \
    if (size < sizeof(int ## bits ## _t))                                               \
        throw Exceptions::RuntimeError("EOF when unpacking \"int" #bits "_t\"");        \
                                                                                        \
    /* unpack as integer */                                                             \
    value = IntObject::fromInt(*(static_cast<const int ## bits ## _t *>(buffer)));      \
}

#define FFI_UNPACK_UINT(bits)                                                           \
{                                                                                       \
    /* type size check */                                                               \
    if (size < sizeof(uint ## bits ## _t))                                              \
        throw Exceptions::RuntimeError("EOF when unpacking \"uint" #bits "_t\"");       \
                                                                                        \
    /* unpack as integer */                                                             \
    value = IntObject::fromUInt(*(static_cast<const uint ## bits ## _t *>(buffer)));    \
}

#define FFI_UNPACK_FLOAT(vtype, dtype)                                                  \
{                                                                                       \
    /* type size check */                                                               \
    if (size < sizeof(vtype))                                                           \
        throw Exceptions::RuntimeError("EOF when unpacking \"" #vtype "\"");            \
                                                                                        \
    /* unpack as decimal */                                                             \
    value = DecimalObject::from ## dtype(*(static_cast<const vtype *>(buffer)));        \
}

void ForeignInt8Type ::unpack(ObjectRef &value, const void *buffer, size_t size) const FFI_UNPACK_INT(8)
void ForeignInt16Type::unpack(ObjectRef &value, const void *buffer, size_t size) const FFI_UNPACK_INT(16)
void ForeignInt32Type::unpack(ObjectRef &value, const void *buffer, size_t size) const FFI_UNPACK_INT(32)
void ForeignInt64Type::unpack(ObjectRef &value, const void *buffer, size_t size) const FFI_UNPACK_INT(64)

void ForeignUInt8Type ::unpack(ObjectRef &value, const void *buffer, size_t size) const FFI_UNPACK_UINT(8)
void ForeignUInt16Type::unpack(ObjectRef &value, const void *buffer, size_t size) const FFI_UNPACK_UINT(16)
void ForeignUInt32Type::unpack(ObjectRef &value, const void *buffer, size_t size) const FFI_UNPACK_UINT(32)
void ForeignUInt64Type::unpack(ObjectRef &value, const void *buffer, size_t size) const FFI_UNPACK_UINT(64)

void ForeignFloatType     ::unpack(ObjectRef &value, const void *buffer, size_t size) const FFI_UNPACK_FLOAT(float, Float)
void ForeignDoubleType    ::unpack(ObjectRef &value, const void *buffer, size_t size) const FFI_UNPACK_FLOAT(double, Double)
void ForeignLongDoubleType::unpack(ObjectRef &value, const void *buffer, size_t size) const FFI_UNPACK_FLOAT(long double, LongDouble)

#undef FFI_UNPACK_INT
#undef FFI_UNPACK_UINT
#undef FFI_UNPACK_FLOAT

/** Foreign Pointer Type **/

void ForeignPointerType::pack(ObjectRef value, void *buffer, size_t size) const
{
    /* type size check */
    if (size < sizeof(void *))
        throw Exceptions::InternalError("Insufficient space for pointers");

    // TODO: implement pack pointer
    throw Exceptions::InternalError("not implemented: pack pointer");
}

void ForeignPointerType::unpack(ObjectRef &value, const void *buffer, size_t size) const
{
    /* type size check */
    if (size < sizeof(void *))
        throw Exceptions::RuntimeError("EOF when unpacking pointers");

    // TODO: implement unpack pointer
    throw Exceptions::InternalError("not implemented: unpack pointer");
}

/** Foreign CString Type **/

void ForeignCStringType::pack(ObjectRef value, void *buffer, size_t size) const
{
    /* type size check */
    if (size < sizeof(char *))
        throw Exceptions::InternalError("Insufficient space for pointers");

    /* const string, use the content directly */
    if (isConst() && value->isInstanceOf(StringTypeObject))
    {
        *(reinterpret_cast<const char **>(buffer)) = value.as<StringObject>()->value().c_str();
        return;
    }

    // TODO: implement pack char *
    throw Exceptions::InternalError("not implemented: pack char *");
}

void ForeignCStringType::unpack(ObjectRef &value, const void *buffer, size_t size) const
{
    /* type size check */
    if (size < sizeof(const char *))
        throw Exceptions::RuntimeError("EOF when unpacking pointers");

    /* get the string pointer */
    auto sp = reinterpret_cast<char * const *>(buffer);
    auto *str = *sp;

    /* null pointer, unpack as null */
    if (str == nullptr)
    {
        value = NullObject;
        return;
    }

    /* unpack as string, if constant */
    if (isConst())
    {
        value = StringObject::fromString(str);
        return;
    }

    // TODO: implement unpack char *
    throw Exceptions::InternalError("not implemented: unpack char *");
}

/** Foreign Function Implementations **/

ForeignFunction::ForeignFunction(TCCState *s, const char *name, TCCFunction *func) :
    NativeFunctionObject(name, std::bind(
        &ForeignFunction::invoke,
        this,
        std::placeholders::_1,
        std::placeholders::_2
    )),
    _name(name),
    _func(tcc_function_get_addr(s, func)),
    _isVarg(tcc_function_is_variadic(func)),
    _rettype(ForeignType::buildFrom(tcc_function_get_return_type(func)))
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
        _argtypes[i] = ForeignType::buildFrom(tcc_function_get_arg_type(func, i));

        /* cache all the pointers */
        _argsv[i].resize(_argtypes[i]->size());
        _argsf[i] = _argtypes[i]->ftype();
        _argsp[i] = _argsv[i].data();
    }

    /* prepare FFI context */
    _ret.resize(_rettype->size());
    ffi_prep_cif(&_cif, FFI_DEFAULT_ABI, static_cast<uint32_t>(_argtypes.size()), _rettype->ftype(), _argsf.data());
}

ObjectRef ForeignFunction::invoke(Utils::NFI::VariadicArgs args, Utils::NFI::KeywordArgs kwargs)
{
    ObjectRef arg;
    ObjectRef result;

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
        if ((arg = kwargs->pop(StringObject::fromStringInterned(_argnames[i]))).isNull())
        {
            /* check for positional arguments */
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
            std::move(arg),
            _argsv[i].data(),
            _argsv[i].size()
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
    _rettype->unpack(result, _ret.data(), _ret.size());
    return std::move(result);
}
}
