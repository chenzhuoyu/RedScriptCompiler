#include "utils/Strings.h"
#include "runtime/IntObject.h"
#include "runtime/NullObject.h"
#include "runtime/StringObject.h"
#include "runtime/DecimalObject.h"
#include "runtime/NativeClassObject.h"

namespace RedScript::Runtime
{
/* type object for native class */
TypeRef NativeClassTypeObject;

/* wrapped FFI types */
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

/** Primitive Boxer Functions **/

static void int8Boxer(ObjectRef value, void *buffer, size_t size)
{
    /* buffer size check */
    if (size != sizeof(int8_t))
        throw Exceptions::RuntimeError("Integer size mismatch");

    /* value type check */
    if (value->isNotInstanceOf(IntTypeObject))
    {
        throw Exceptions::TypeError(Utils::Strings::format(
            "Integer required, not \"%s\"",
            value->type()->name()
        ));
    }
}

/** Primitive Unboxer Functions **/

static ObjectRef voidUnboxer(const void *, size_t size)
{
    if (size != 0)
        throw Exceptions::RuntimeError("Non-empty void value");
    else
        return NullObject;
}

template <typename T>
static ObjectRef intUnboxer(const void *buffer, size_t size)
{
    if (size != sizeof(T))
        throw Exceptions::RuntimeError("Integer size mismatch");
    else
        return IntObject::fromInt(*(reinterpret_cast<const T *>(buffer)));
}

template <typename T>
static ObjectRef uintUnboxer(const void *buffer, size_t size)
{
    if (size != sizeof(T))
        throw Exceptions::RuntimeError("Integer size mismatch");
    else
        return IntObject::fromUInt(*(reinterpret_cast<const T *>(buffer)));
}

template <typename T>
static ObjectRef decimalUnboxer(const void *buffer, size_t size)
{
    if (size != sizeof(T))
        throw Exceptions::RuntimeError("Floating-point size mismatch");
    else
        return DecimalObject::fromDouble(*(reinterpret_cast<const T *>(buffer)));
}

static ObjectRef longDoubleUnboxer(const void *buffer, size_t size)
{
    if (size != sizeof(long double))
        throw Exceptions::RuntimeError("Floating-point size mismatch");
    else
        return DecimalObject::fromDecimal(*(reinterpret_cast<const long double *>(buffer)));
}

void NativeClassObject::initialize(void)
{
    /* native class type object */
    static NativeClassType nullType;
    NativeClassTypeObject = Reference<NativeClassType>::refStatic(nullType);

    /* FFI types */
    ForeignVoidTypeObject       = Object::newObject<ForeignType>("void"       , &ffi_type_void      , nullptr, voidUnboxer);
    ForeignInt8TypeObject       = Object::newObject<ForeignType>("int8_t"     , &ffi_type_sint8     , nullptr, intUnboxer<int8_t>);
    ForeignUInt8TypeObject      = Object::newObject<ForeignType>("uint8_t"    , &ffi_type_uint8     , nullptr, uintUnboxer<uint8_t>);
    ForeignInt16TypeObject      = Object::newObject<ForeignType>("int16_t"    , &ffi_type_sint16    , nullptr, intUnboxer<int16_t>);
    ForeignUInt16TypeObject     = Object::newObject<ForeignType>("uint16_t"   , &ffi_type_uint16    , nullptr, uintUnboxer<uint16_t>);
    ForeignInt32TypeObject      = Object::newObject<ForeignType>("int32_t"    , &ffi_type_sint32    , nullptr, intUnboxer<int32_t>);
    ForeignUInt32TypeObject     = Object::newObject<ForeignType>("uint32_t"   , &ffi_type_uint32    , nullptr, uintUnboxer<uint32_t>);
    ForeignInt64TypeObject      = Object::newObject<ForeignType>("int64_t"    , &ffi_type_sint64    , nullptr, intUnboxer<int64_t>);
    ForeignUInt64TypeObject     = Object::newObject<ForeignType>("uint64_t"   , &ffi_type_uint64    , nullptr, uintUnboxer<uint64_t>);
    ForeignFloatTypeObject      = Object::newObject<ForeignType>("float"      , &ffi_type_float     , nullptr, decimalUnboxer<float>);
    ForeignDoubleTypeObject     = Object::newObject<ForeignType>("double"     , &ffi_type_double    , nullptr, decimalUnboxer<double>);
    ForeignLongDoubleTypeObject = Object::newObject<ForeignType>("long double", &ffi_type_longdouble, nullptr, longDoubleUnboxer);
}

Reference<ForeignType> ForeignType::buildFrom(TCCType *type)
{
    /* type ID and name */
    int id = tcc_type_get_id(type);
    std::string name = tcc_type_get_name(type);
    Reference<ForeignType> result = nullptr;

    /* convert types */
    switch (id & VT_BTYPE)
    {
        case VT_VOID    : result = ForeignVoidTypeObject; break;
        case VT_BOOL    : result = ForeignInt8TypeObject; break;
        case VT_FLOAT   : result = ForeignFloatTypeObject; break;
        case VT_DOUBLE  : result = ForeignDoubleTypeObject; break;
        case VT_LDOUBLE : result = ForeignLongDoubleTypeObject; break;
        case VT_BYTE    : result = (id & VT_UNSIGNED) ? ForeignUInt8TypeObject  : ForeignInt8TypeObject ; break;
        case VT_SHORT   : result = (id & VT_UNSIGNED) ? ForeignUInt16TypeObject : ForeignInt16TypeObject; break;
        case VT_INT     : result = (id & VT_UNSIGNED) ? ForeignUInt32TypeObject : ForeignInt32TypeObject; break;
        case VT_LLONG   : result = (id & VT_UNSIGNED) ? ForeignUInt64TypeObject : ForeignInt64TypeObject; break;
        case VT_PTR     : result = Object::newObject<ForeignType>(name, buildFrom(tcc_type_get_ref(type))); break;

        case VT_QLONG:
            throw Exceptions::TypeError("FFI currently doesn't support 128-bit integers");

        case VT_QFLOAT:
            throw Exceptions::TypeError("FFI currently doesn't support 128-bit floating point numbers");

        case VT_FUNC:
        {
            // TODO: implement VT_FUNC
            throw Exceptions::InternalError("not implemented: VT_FUNC");
        }

        case VT_STRUCT:
        {
            // TODO: implement VT_STRUCT
            throw Exceptions::InternalError("not implemented: VT_STRUCT");
        }

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
    _rettype(ForeignType::buildFrom(tcc_function_get_return_type(func))),
    _argtypes(tcc_function_get_nargs(func))
{
    for (size_t i = 0; i < _argtypes.size(); i++)
    {
        _argtypes[i].first = tcc_function_get_arg_name(func, i);
        _argtypes[i].second = ForeignType::buildFrom(tcc_function_get_arg_type(func, i));
    }
}

ObjectRef ForeignFunction::invoke(Utils::NFI::VariadicArgs args, Utils::NFI::KeywordArgs kwargs)
{
    printf("** invoke(%s) **\n", _name.c_str());
    return NullObject;
}
}
