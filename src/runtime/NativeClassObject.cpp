
#include <runtime/NativeClassObject.h>

#include "utils/Decimal.h"
#include "utils/Integer.h"
#include "utils/Strings.h"

#include "runtime/IntObject.h"
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

    return Utils::Strings::format("(%p)%s", static_cast<void *>(t), name);
}

void NativeClassType::addBuiltins(void)
{
    /* shutdown FFI types */
    ForeignVoidTypeObject            ->typeInitialize();
    ForeignInt8TypeObject            ->typeInitialize();
    ForeignUInt8TypeObject           ->typeInitialize();
    ForeignInt16TypeObject           ->typeInitialize();
    ForeignUInt16TypeObject          ->typeInitialize();
    ForeignInt32TypeObject           ->typeInitialize();
    ForeignUInt32TypeObject          ->typeInitialize();
    ForeignInt64TypeObject           ->typeInitialize();
    ForeignUInt64TypeObject          ->typeInitialize();
    ForeignFloatTypeObject           ->typeInitialize();
    ForeignDoubleTypeObject          ->typeInitialize();
    ForeignLongDoubleTypeObject      ->typeInitialize();

    /* shutdown raw pointer types */
    ForeignRawPointerTypeObject      ->typeInitialize();
    ForeignConstRawPointerTypeObject ->typeInitialize();

    /* shutdown string types */
    ForeignCStringTypeObject         ->typeInitialize();
    ForeignConstCStringTypeObject    ->typeInitialize();
}

void NativeClassType::clearBuiltins(void)
{
    /* shutdown FFI types */
    ForeignVoidTypeObject            ->typeShutdown();
    ForeignInt8TypeObject            ->typeShutdown();
    ForeignUInt8TypeObject           ->typeShutdown();
    ForeignInt16TypeObject           ->typeShutdown();
    ForeignUInt16TypeObject          ->typeShutdown();
    ForeignInt32TypeObject           ->typeShutdown();
    ForeignUInt32TypeObject          ->typeShutdown();
    ForeignInt64TypeObject           ->typeShutdown();
    ForeignUInt64TypeObject          ->typeShutdown();
    ForeignFloatTypeObject           ->typeShutdown();
    ForeignDoubleTypeObject          ->typeShutdown();
    ForeignLongDoubleTypeObject      ->typeShutdown();

    /* shutdown raw pointer types */
    ForeignRawPointerTypeObject      ->typeShutdown();
    ForeignConstRawPointerTypeObject ->typeShutdown();

    /* shutdown string types */
    ForeignCStringTypeObject         ->typeShutdown();
    ForeignConstCStringTypeObject    ->typeShutdown();
}

std::string NativeClassType::nativeObjectRepr(ObjectRef self)
{
    return Utils::Strings::format(
        "<native class \"%s\" at %p>",
        self.as<NativeClassObject>()->name(),
        static_cast<void *>(self.get())
    );
}

static inline bool buildNativeSource(TCCState *tcc, const std::string &source)
{
    auto size = source.size();
    auto buffer = source.data();
    return (tcc_compile_string_ex(tcc, "<native>", buffer, size) >= 0) && (tcc_relocate(tcc) >= 0);
}

NativeClassObject::NativeClassObject(
    const std::string &name,
    const std::string &code,
    Runtime::Reference<Runtime::MapObject> &&options
) : Object(NativeClassTypeObject), _tcc(tcc_new()), _name(name)
{
    /* initialize tcc */
    tcc_set_options(_tcc, "-nostdlib");
    tcc_set_options(_tcc, "-rdynamic");
    tcc_set_output_type(_tcc, TCC_OUTPUT_MEMORY);

    /* error output function */
    tcc_set_error_func(_tcc, this, [](TCCState *s, int isWarning, const char *file, int line, const char *message, void *ctx)
    {
        NativeClassObject *self = static_cast<NativeClassObject *>(ctx);
        self->_errors.emplace_back(file ?: "", line, (isWarning != 0), message);
    });

    /* parse options */
    options->enumerate([&](ObjectRef key, ObjectRef value)
    {
        /* key must be string */
        if (key->isNotInstanceOf(StringTypeObject))
            throw Exceptions::InternalError("Non-string keys");

        /* value must be string either */
        if (value->isNotInstanceOf(StringTypeObject))
            throw Exceptions::InternalError("Non-string values");

        /* extract key and value strings */
        auto &skey = key.as<StringObject>()->value();
        auto &svalue = value.as<StringObject>()->value();

        /* add CFLAGS and LDFLAGS */
        if ((skey == "cflags") || (skey == "ldflags"))
            tcc_set_options(_tcc, svalue.c_str());

        /* continue enumerating */
        return true;
    });

    /* compile the code and link the code in memory */
    if (!(buildNativeSource(_tcc, code)))
        if (_errors.empty())
            throw Exceptions::RuntimeError("Unable to link native object in memory");

    /* handle all the errors */
    for (const auto &ctx : _errors)
        if (!(ctx->isWarning()))
            throw ctx;

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

    /* clear the type caches */
    _enums.clear();
    _types.clear();
}

void NativeClassObject::addType(const char *name, TCCType *type)
{
    auto ntype = makeForeignType(type);
    addObject(ntype->name(), std::move(ntype));
}

void NativeClassObject::addFunction(const char *name, TCCFunction *func)
{
    auto fname = name ?: tcc_function_get_name(func);
    addObject(fname, Object::newObject<ForeignFunction>(this, _tcc, fname, func));
}

static inline std::string makeTypeName(const char *name)
{
    if (name[0] && (name[0] == 'L') &&
        name[1] && (name[1] == '.'))
        return Utils::Strings::format("__anonymous_%s__", makeTypeName(name + 2));
    else
        return name;
}

Reference<ForeignType> NativeClassObject::makeForeignType(TCCType *type)
{
    /* type ID */
    int id = tcc_type_get_id(type);
    decltype(_types.find(type)) iter;

    /* special case: enums */
    if (IS_ENUM(id))
    {
        /* enum name */
        auto tname = tcc_type_get_name(type);
        auto ename = makeTypeName(tname);

        /* read from cache if possible */
        if (_enums.find(ename) == _enums.end())
        {
            /* create the enumeration type and object */
            auto etype = Object::newObject<ForeignEnumType>(ename);
            auto evalues = Object::newObject<Object>(etype);

            /* add all fields into enum namespace */
            tcc_type_list_items(_tcc, type, [](TCCState *s, TCCType *t, const char *name, long long val, void *pn) -> char
            {
                (*(reinterpret_cast<decltype(etype) *>(pn)))->attrs().emplace(name, IntObject::fromInt(val));
                return 1;
            }, &etype);

            /* add to enums and objects */
            addObject(ename, std::move(evalues));
            _enums.emplace(ename, std::move(etype));
        }
    }

    /* other types */
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
            return ForeignConstRawPointerTypeObject;
        }

        /* structs */
        case VT_STRUCT:
        {
            /* read from cache if possible */
            if ((iter = _types.find(type)) != _types.end())
                return iter->second;

            /* field enumeration context */
            struct ContextType
            {
                NativeClassObject *self;
                Reference<ForeignStructType> result;
            } ctx;

            /* type name and field count */
            auto align = tcc_type_get_alignment(type);
            auto tname = makeTypeName(tcc_type_get_name(type));
            auto fcount = static_cast<size_t>(tcc_type_get_nkeys(type));

            /* add to type cache first */
            ctx.self = this;
            ctx.result = Object::newObject<ForeignStructType>(tname, fcount, align);
            _types.emplace(type, ctx.result);

            /* enumerate all fields */
            tcc_type_list_fields(_tcc, type, [](TCCState *s, TCCType *t, const char *name, TCCType *type, void *pn) -> char
            {
                ContextType *pctx = reinterpret_cast<ContextType *>(pn);
                pctx->result->addField(name, pctx->self->makeForeignType(type));
                return 1;
            }, &ctx);

            /* add type properties */
            ctx.result->addProperties();
            return std::move(ctx.result);
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

namespace
{
template <typename Type, typename Instance, typename Arg>
struct ForeignTypeFactory : public Type
{
    using Type::Type;
    virtual ObjectRef nativeObjectNew(TypeRef type, Reference<TupleObject> args, Reference<MapObject> kwargs) override
    {
        using Utils::NFI::MetaConstructor;
        return MetaConstructor<Instance, Arg>::construct(args, kwargs, {"value"}, {});
    }
};

struct ArgsParser
{
    ObjectRef parse(Reference<TupleObject> &args, Reference<MapObject> &kwargs)
    {
        /* takes at most 1 argument */
        if (args->size() > 1)
        {
            throw Exceptions::TypeError(Utils::Strings::format(
                "Constructor takes at most 1 argument, but %zu given",
                args->size()
            ));
        }

        /* try popping from keyword arguments first */
        auto name = StringObject::fromStringInterned("value");
        auto value = kwargs->pop(std::move(name));

        /* no value, check positional arguments */
        if (args->size() && value.isNull())
            value = args->items()[0];

        /* should have no more keyword arguments */
        if (kwargs->size())
        {
            auto front = kwargs->firstKey();
            throw Exceptions::TypeError(Utils::Strings::format(
                "Constructor does not accept keyword argument \"%s\"",
                front->type()->objectStr(front)
            ));
        }

        /* move to prevent copy */
        return std::move(value);
    }
};

struct PointerFactory : public ForeignPointerType, public ArgsParser
{
    using ForeignPointerType::ForeignPointerType;
    virtual ObjectRef nativeObjectNew(TypeRef type, Reference<TupleObject> args, Reference<MapObject> kwargs) override
    {
        /* parse arguments */
        auto value = parse(args, kwargs);
        auto &vtype = value->type();

        /* no arguments or a single null */
        if (value.isNull() || value.isIdenticalWith(NullObject))
            return Object::newObject<ForeignRawBuffer>(ForeignRawPointerTypeObject, nullptr, 0, false);

        /* it's a string, but we can't handle it here */
        if (vtype.isIdenticalWith(StringTypeObject))
            throw Exceptions::TypeError("Cannot convert strings into raw pointers, use `ffi.char_p` instead");

        /* already raw pointers */
        if (vtype.isIdenticalWith(ForeignRawPointerTypeObject) ||
            vtype.isIdenticalWith(ForeignConstRawPointerTypeObject))
            return std::move(value);

        /* try convert to raw buffer */
        Object *obj = value.get();
        ForeignRawBuffer *frb = dynamic_cast<ForeignRawBuffer *>(obj);

        /* otherwise, it must be a raw buffer */
        if (frb == nullptr)
        {
            throw Exceptions::TypeError(Utils::Strings::format(
                "Cannot convert \"%s\" object into pointers",
                vtype->name()
            ));
        }

        /* wrap with raw pointers */
        return Object::newObject<ForeignRawBuffer>(
            ForeignRawPointerTypeObject,
            frb->buffer(),
            frb->bufferSize(),
            false
        );
    }
};

struct CStringFactory : public ForeignCStringType, public ArgsParser
{
    using ForeignCStringType::ForeignCStringType;
    virtual ObjectRef nativeObjectNew(TypeRef type, Reference<TupleObject> args, Reference<MapObject> kwargs) override
    {
        /* parse arguments */
        auto value = parse(args, kwargs);
        auto &vtype = value->type();

        /* no arguments or a single null */
        if (value.isNull() || value.isIdenticalWith(NullObject))
            return Object::newObject<ForeignStringBuffer>(nullptr);

        /* it's a string */
        if (vtype.isIdenticalWith(StringTypeObject))
            return Object::newObject<ForeignStringBuffer>(value.as<StringObject>()->value());

        /* already string buffers */
        if (vtype.isIdenticalWith(ForeignCStringTypeObject) ||
            vtype.isIdenticalWith(ForeignConstCStringTypeObject))
            return std::move(value);

        /* otherwise it's an error */
        throw Exceptions::TypeError(Utils::Strings::format(
            "Argument must be a string or string buffer, not \"%s\"",
            vtype->objectStr(value)
        ));
    }
};
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
    static ForeignVoidType                                                           foreignVoidType;
    static ForeignTypeFactory<ForeignInt8Type      , ForeignInt8      , int8_t     > foreignInt8Type;
    static ForeignTypeFactory<ForeignUInt8Type     , ForeignUInt8     , uint8_t    > foreignUInt8Type;
    static ForeignTypeFactory<ForeignInt16Type     , ForeignInt16     , int16_t    > foreignInt16Type;
    static ForeignTypeFactory<ForeignUInt16Type    , ForeignUInt16    , uint16_t   > foreignUInt16Type;
    static ForeignTypeFactory<ForeignInt32Type     , ForeignInt32     , int32_t    > foreignInt32Type;
    static ForeignTypeFactory<ForeignUInt32Type    , ForeignUInt32    , uint32_t   > foreignUInt32Type;
    static ForeignTypeFactory<ForeignInt64Type     , ForeignInt64     , int64_t    > foreignInt64Type;
    static ForeignTypeFactory<ForeignUInt64Type    , ForeignUInt64    , uint64_t   > foreignUInt64Type;
    static ForeignTypeFactory<ForeignFloatType     , ForeignFloat     , float      > foreignFloatType;
    static ForeignTypeFactory<ForeignDoubleType    , ForeignDouble    , double     > foreignDoubleType;
    static ForeignTypeFactory<ForeignLongDoubleType, ForeignLongDouble, long double> foreignLongDoubleType;

    /* wrapped primitive FFI types */
    ForeignVoidTypeObject            = Reference<ForeignVoidType                                                          >::refStatic(foreignVoidType      );
    ForeignInt8TypeObject            = Reference<ForeignTypeFactory<ForeignInt8Type      , ForeignInt8      , int8_t     >>::refStatic(foreignInt8Type      );
    ForeignUInt8TypeObject           = Reference<ForeignTypeFactory<ForeignUInt8Type     , ForeignUInt8     , uint8_t    >>::refStatic(foreignUInt8Type     );
    ForeignInt16TypeObject           = Reference<ForeignTypeFactory<ForeignInt16Type     , ForeignInt16     , int16_t    >>::refStatic(foreignInt16Type     );
    ForeignUInt16TypeObject          = Reference<ForeignTypeFactory<ForeignUInt16Type    , ForeignUInt16    , uint16_t   >>::refStatic(foreignUInt16Type    );
    ForeignInt32TypeObject           = Reference<ForeignTypeFactory<ForeignInt32Type     , ForeignInt32     , int32_t    >>::refStatic(foreignInt32Type     );
    ForeignUInt32TypeObject          = Reference<ForeignTypeFactory<ForeignUInt32Type    , ForeignUInt32    , uint32_t   >>::refStatic(foreignUInt32Type    );
    ForeignInt64TypeObject           = Reference<ForeignTypeFactory<ForeignInt64Type     , ForeignInt64     , int64_t    >>::refStatic(foreignInt64Type     );
    ForeignUInt64TypeObject          = Reference<ForeignTypeFactory<ForeignUInt64Type    , ForeignUInt64    , uint64_t   >>::refStatic(foreignUInt64Type    );
    ForeignFloatTypeObject           = Reference<ForeignTypeFactory<ForeignFloatType     , ForeignFloat     , float      >>::refStatic(foreignFloatType     );
    ForeignDoubleTypeObject          = Reference<ForeignTypeFactory<ForeignDoubleType    , ForeignDouble    , double     >>::refStatic(foreignDoubleType    );
    ForeignLongDoubleTypeObject      = Reference<ForeignTypeFactory<ForeignLongDoubleType, ForeignLongDouble, long double>>::refStatic(foreignLongDoubleType);

    /* raw pointer types */
    static PointerFactory foreignPointerType(false);
    static ForeignPointerType foreignConstPointerType(true);

    /* raw pointer types */
    ForeignRawPointerTypeObject      = Reference<PointerFactory>::refStatic(foreignPointerType);
    ForeignConstRawPointerTypeObject = Reference<ForeignPointerType>::refStatic(foreignConstPointerType);

    /* string types */
    static CStringFactory foreignCStringType(false);
    static ForeignCStringType foreignConstCStringType(true);

    /* string types */
    ForeignCStringTypeObject         = Reference<CStringFactory>::refStatic(foreignCStringType);
    ForeignConstCStringTypeObject    = Reference<ForeignCStringType>::refStatic(foreignConstCStringType);
}

/** Abstract Foreign Type **/

static inline ForeignInstance *foreignInstanceCheck(ObjectRef &self)
{
    /* convert to root foreign instance */
    Object *obj = self.get();
    ForeignInstance *fvalue = dynamic_cast<ForeignInstance *>(obj);

    /* foreign type check */
    if (fvalue != nullptr)
        return fvalue;

    /* instance type mismatch */
    throw Exceptions::TypeError(Utils::Strings::format(
        "Argument must be a foreign instance, not \"%s\"",
        obj->type()->name()
    ));
}

static ObjectRef foreignTypeValueGetter(ObjectRef self, ObjectRef type)
{
    /* read it's value */
    return foreignInstanceCheck(self)->get();
}

static ObjectRef foreignTypeValueSetter(ObjectRef self, ObjectRef type, ObjectRef value)
{
    /* set it's value */
    foreignInstanceCheck(self)->set(value);
    return std::move(value);
}

ForeignType::ForeignType(const std::string &name, ffi_type *type, bool addBuiltins) : NativeType(name), _ftype(type)
{
    if (addBuiltins)
    {
        addObject("value", ProxyObject::newReadWrite(
            NativeFunctionObject::newBinary("__get__", &foreignTypeValueGetter),
            NativeFunctionObject::newTernary("__set__", &foreignTypeValueSetter)
        ));
    }
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

/** Foreign Struct Type Implementations **/

static inline ffi_type *newStructType(size_t fields, uint16_t align)
{
    /* create a new struct type */
    ffi_type *result = Engine::Memory::alloc<ffi_type>();
    ffi_type **elements =  Engine::Memory::alloc<ffi_type *>(fields);

    /* initialize struct type */
    result->size = 0;
    result->type = FFI_TYPE_STRUCT;
    result->elements = elements;
    result->alignment = align;
    return result;
}

ForeignStructType::~ForeignStructType()
{
    Engine::Memory::free(ftype()->elements);
    Engine::Memory::free(ftype());
}

ForeignStructType::ForeignStructType(const std::string &name, size_t fields, uint16_t align) :
    ForeignType(name, newStructType(fields, align), false)
{
    track();
    _names.reserve(fields);
    _types.reserve(fields);
    _fields.reserve(fields);
}

void ForeignStructType::addField(std::string &&name, Reference<ForeignType> &&type)
{
    /* type size cannot be zero */
    if (type->size() == 0)
        throw Exceptions::InternalError("Void field type");

    /* size, offset and field alignment */
    size_t size = type->size();
    size_t offset = ftype()->size;
    size_t alignment = ftype()->alignment;

    /* align the size as needed */
    if (alignment != 1)
        size = ((size - 1) / alignment + 1) * alignment;

    /* add to FFI type descriptor */
    ftype()->size += size;
    ftype()->elements[_names.size()] = type->ftype();

    /* add to name and type list */
    _names.emplace_back(std::move(name));
    _types.emplace_back(std::move(type));
    _fields.emplace_back(offset);
}

void ForeignStructType::addProperties(void)
{
    /* add each field in order */
    for (size_t i = 0; i < _names.size(); i++)
    {
        /* property getter */
        auto getter = [=](ObjectRef self, ObjectRef type) -> ObjectRef
        {
            return _types[i]->unpack(
                foreignInstanceCheck(self)->field(_fields[i]),
                _types[i]->size()
            );
        };

        /* property setter */
        auto setter = [=](ObjectRef self, ObjectRef type, ObjectRef value) -> ObjectRef
        {
            _types[i]->pack(foreignInstanceCheck(self)->field(_fields[i]), _types[i]->size(), value);
            return std::move(value);
        };

        /* create attribute proxy */
        addObject(_names[i], ProxyObject::newReadWrite(
            NativeFunctionObject::newBinary("__get__", getter),
            NativeFunctionObject::newTernary("__set__", setter)
        ));
    }
}

void ForeignStructType::pack(void *buffer, size_t size, ObjectRef value) const
{
    /* type size check */
    if (size < this->size())
        throw Exceptions::InternalError("Insufficient space for structs");

    // TODO: pack struct
    throw Exceptions::InternalError("not implemented: pack struct");
}

ObjectRef ForeignStructType::unpack(const void *buffer, size_t size) const
{
    /* type size check */
    if (size < this->size())
        throw Exceptions::RuntimeError("EOF when unpacking structs");

    // TODO: unpack struct
    throw Exceptions::InternalError("not implemented: unpack struct");
}

std::string ForeignStructType::nativeObjectRepr(ObjectRef self)
{
    // TODO: struct representation
    return ForeignType::nativeObjectRepr(self);
}

ObjectRef ForeignStructType::nativeObjectNew(TypeRef type, Reference<TupleObject> args, Reference<MapObject> kwargs)
{
    // TODO: handle kwargs
    auto result = Object::newObject<ForeignInstance>(type);
    return result;
}

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
    /* null pointer check */
    if (buffer == nullptr)
        throw Exceptions::RuntimeError("Null-pointer dereferencing");

    /* get the underlying value type */
    auto obj = const_cast<ForeignPointerType *>(this);
    auto type = obj->self().as<ForeignType>();

    /* type size check */
    if (size < obj->size())
        throw Exceptions::RuntimeError("EOF when unpacking pointers");

    /* wrap as raw pointer */
    auto sp = *(reinterpret_cast<void * const *>(buffer));
    return Object::newObject<ForeignRawBuffer>(type, sp, obj->size(), false);
}

std::string ForeignPointerType::nativeObjectRepr(ObjectRef self)
{
    /* read the pointer */
    auto obj = self.as<ForeignRawBuffer>();
    auto pointer = obj->buffer();

    /* null pointer */
    if (pointer == nullptr)
        return Utils::Strings::format("ffi.%s(null)", name());

    /* other pointers */
    return Utils::Strings::format(
        "ffi.%s(ptr=%p, size=%zu, auto_release=%s)",
        name(),
        pointer,
        self.as<ForeignRawBuffer>()->bufferSize(),
        self.as<ForeignRawBuffer>()->isAutoRelease() ? "true" : "false"
    );
}

/** Foreign Pointer Type Implementations **/

static inline ForeignRawBuffer *foreignPointerCheck(ObjectRef &self)
{
    /* convert to raw buffer */
    Object *obj = self.get();
    ForeignRawBuffer *frb = dynamic_cast<ForeignRawBuffer *>(obj);

    /* object type check */
    if (frb != nullptr)
        return frb;

    /* buffer type mismatch */
    throw Exceptions::TypeError(Utils::Strings::format(
        "Argument must be a buffer, not \"%s\"",
        self->type()->name()
    ));
}

static uintptr_t foreignPointerAddrGetter(ObjectRef self, ObjectRef type)
{
    /* get it's address */
    return reinterpret_cast<uintptr_t>(foreignPointerCheck(self)->buffer());
}

static size_t foreignPointerAllocSizeGetter(ObjectRef self, ObjectRef type)
{
    /* get it's allocation size */
    return foreignPointerCheck(self)->allocationSize();
}

static size_t foreignPointerSizeGetter(ObjectRef self, ObjectRef type)
{
    /* get it's size */
    return foreignPointerCheck(self)->bufferSize();
}

static size_t foreignPointerSizeSetter(ObjectRef self, ObjectRef type, size_t value)
{
    /* set it's size */
    foreignPointerCheck(self)->setRawBufferSize(value);
    return value;
}

static bool foreignPointerAutoReleaseGetter(ObjectRef self, ObjectRef type)
{
    /* read it's auto release property */
    return foreignPointerCheck(self)->isAutoRelease();
}

static bool foreignPointerAutoReleaseSetter(ObjectRef self, ObjectRef type, bool value)
{
    /* set it's auto release property */
    foreignPointerCheck(self)->setAutoRelease(value);
    return value;
}

ForeignPointerType::ForeignPointerType(const std::string &name, Reference<ForeignType> base, bool isConst) :
    ForeignType(name, &ffi_type_pointer),
    _base(base),
    _isConst(isConst)
{
    /* addr and alloc property */
    addObject("addr", ProxyObject::newReadOnly(NativeFunctionObject::fromFunction("__get__", &foreignPointerAddrGetter)));
    addObject("alloc_size", ProxyObject::newReadOnly(NativeFunctionObject::fromFunction("__get__", &foreignPointerAllocSizeGetter)));

    /* size property */
    addObject("size", ProxyObject::newReadWrite(
        NativeFunctionObject::fromFunction("__get__", &foreignPointerSizeGetter),
        NativeFunctionObject::fromFunction("__set__", &foreignPointerSizeSetter)
    ));

    /* auto release property */
    addObject("auto_release", ProxyObject::newReadWrite(
        NativeFunctionObject::fromFunction("__get__", &foreignPointerAutoReleaseGetter),
        NativeFunctionObject::fromFunction("__set__", &foreignPointerAutoReleaseSetter)
    ));

    /* track the pointer */
    track();
}

/** Foreign CString Type Implementations **/

ForeignCStringType::ForeignCStringType(bool isConst) :
    ForeignPointerType(isConst ? "const_char_p" : "char_p", ForeignInt8TypeObject, isConst)
{
    /* length property */
    addObject("length", ProxyObject::newReadOnly(NativeFunctionObject::fromFunction("__get__", [](ObjectRef self, ObjectRef type)
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
        return fsb->length();
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
    auto obj = self.as<ForeignStringBuffer>();
    return (obj->buffer() && obj->bufferSize()) ? ForeignType::nativeObjectRepr(self) : ForeignPointerType::nativeObjectRepr(self);
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
    /* check function address */
    if (_func == nullptr)
    {
        throw Exceptions::AttributeError(Utils::Strings::format(
            "Undefined reference to function \"%s\"",
            _name
        ));
    }

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

static inline ForeignType *foreignTypeCheck(TypeRef &type)
{
    /* convert to root foreign instance */
    Type *obj = type.get();
    ForeignType *ftype = dynamic_cast<ForeignType *>(obj);

    /* foreign type check */
    if (ftype != nullptr)
        return ftype;

    /* instance type mismatch */
    throw Exceptions::TypeError(Utils::Strings::format(
        "Argument must be a foreign type, not \"%s\"",
        obj->name()
    ));
}

ForeignRawBuffer::~ForeignRawBuffer()
{
    if (_free)
        Engine::Memory::free(buffer());
}

ForeignRawBuffer::ForeignRawBuffer(TypeRef type, void *data, size_t size, bool autoRelease) :
    ForeignInstance(type),
    _size(size),
    _alloc(size)
{
    /* must be pointer types */
    if (ftype() != &ffi_type_pointer)
        throw Exceptions::InternalError("Raw buffer must be pointers");

    /* reference only, copy the pointer */
    if (!autoRelease)
    {
        _free = false;
        buffer() = data;
    }
    else
    {
        _free = true;
        buffer() = Engine::Memory::alloc(size);

        /* copy data as needed */
        if (data == nullptr)
            memset(buffer(), 0, _size);
        else
            memcpy(buffer(), data, _size);
    }
}

void ForeignRawBuffer::set(ObjectRef value)
{
    auto &type = this->type();
    foreignTypeCheck(type)->pack(buffer(), bufferSize(), std::move(value));
}

ObjectRef ForeignRawBuffer::get(void) const
{
    auto &type = const_cast<ForeignRawBuffer *>(this)->type();
    return foreignTypeCheck(type)->unpack(buffer(), bufferSize());
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
        throw Exceptions::RuntimeError("Cannot set value of a string with unknwon size");

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
        throw Exceptions::RuntimeError("Cannot get value of a string with unknwon size");
    else
        return StringObject::fromString(std::string(str(), str() + length()));
}
}
