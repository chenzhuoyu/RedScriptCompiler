#include "modules/FFI.h"
#include "runtime/ExceptionObject.h"
#include "runtime/NativeClassObject.h"
#include "runtime/NativeFunctionObject.h"

namespace RedScript::Modules
{
/* FFI module object */
Runtime::ModuleRef FFIModule;

static Runtime::ObjectRef ffiRef(Runtime::ObjectRef value)
{
    /* try converting to FFI instance */
    Runtime::Object *obj = value.get();
    Runtime::ForeignInstance *fip = dynamic_cast<Runtime::ForeignInstance *>(obj);

    /* FFI type checking */
    if (fip == nullptr)
    {
        throw Runtime::Exceptions::TypeError(Utils::Strings::format(
            "Argument must be a FFI instance, not \"%s\"",
            obj->type()->name()
        ));
    }

    /* create pointer type */
    auto type = fip->type().as<Runtime::ForeignType>();
    auto ptype = Runtime::ForeignPointerType::ref(type);

    /* wrap with pointer */
    return Runtime::Object::newObject<Runtime::ForeignRawBuffer>(
        std::move(ptype),
        fip->data(),
        fip->size(),
        false
    );
}

static Runtime::ObjectRef ffiStringAt(Runtime::ObjectRef addr, Runtime::ObjectRef size)
{
    /* string buffer address and length */
    void *buffer = nullptr;
    size_t length = SIZE_T_MAX;

    /* address may also be an integer */
    if (addr->isInstanceOf(Runtime::IntTypeObject))
    {
        /* convert to integer */
        auto vaddr = addr.as<Runtime::IntObject>();
        auto &value = vaddr->value();
        uint64_t vbuffer;

        /* must fit into one machine word */
        if (!(value.isSafeUInt()) || ((vbuffer = value.toUInt()) > UINTPTR_MAX))
        {
            throw Runtime::Exceptions::ValueError(Utils::Strings::format(
                "Invalid pointer value: %s",
                value.toString()
            ));
        }

        /* convert to pointer */
        buffer = reinterpret_cast<void *>(vbuffer);
    }

    /* or maybe a valid buffer object; notice that `bufferSize()` might
     * be zero here, which subtract by one will underflows to `SIZE_T_MAX`,
     * which is intended, since `SIZE_T_MAX` represents an invalid length */
    else if (auto *frb = dynamic_cast<Runtime::ForeignRawBuffer *>(addr.get()))
    {
        buffer = frb->buffer();
        length = frb->bufferSize() - 1;
    }

    /* otherwise, it's an error */
    else
    {
        throw Runtime::Exceptions::TypeError(Utils::Strings::format(
            "Argument \"addr\" must be an integer or buffer object, not \"%s\"",
            addr->type()->name()
        ));
    }

    /* create a string buffer */
    auto sp = reinterpret_cast<char *>(buffer);
    auto fsb = Runtime::Object::newObject<Runtime::ForeignStringBuffer>(sp);

    /* set string buffer size if any */
    if (size.isNotNull())
    {
        /* size must be an integer */
        if (size->isNotInstanceOf(Runtime::IntTypeObject))
        {
            throw Runtime::Exceptions::TypeError(Utils::Strings::format(
                "Argument \"size\" must be an integer, not \"%s\"",
                size->type()->name()
            ));
        }

        /* convert to integer */
        auto vsize = size.as<Runtime::IntObject>();
        auto &value = vsize->value();

        /* must be a valid unsigned int */
        if (!(value.isSafeUInt()))
        {
            throw Runtime::Exceptions::ValueError(Utils::Strings::format(
                "Invalid string length: %s",
                value.toString()
            ));
        }

        /* check for length rage */
        if ((length = value.toUInt()) >= SIZE_T_MAX)
        {
            throw Runtime::Exceptions::ValueError(Utils::Strings::format(
                "String too long: %lu",
                length
            ));
        }
    }

    /* set the length, if any, plus 1 byte NULL terminator */
    if (length != SIZE_T_MAX)
        fsb->setRawBufferSize(length + 1);

    /* move to prevent copy */
    return std::move(fsb);
}

static Runtime::ObjectRef ffiPointerOf(Runtime::ObjectRef type)
{
    /* base type check */
    if (dynamic_cast<Runtime::ForeignType *>(type.get()) == nullptr)
    {
        throw Runtime::Exceptions::TypeError(Utils::Strings::format(
            "\"pointer_of\" requires the first argument to be a FFI type, not \"%s\" object",
            type->type()->name()
        ));
    }

    /* wrap with pointer type */
    return Runtime::ForeignPointerType::ref(type.as<Runtime::ForeignType>());
}

FFI::FFI() : Runtime::ModuleObject("ffi")
{
    /* primitive types */
    addObject("void"        , Runtime::ForeignVoidTypeObject);
    addObject("int8_t"      , Runtime::ForeignInt8TypeObject);
    addObject("int16_t"     , Runtime::ForeignInt16TypeObject);
    addObject("int32_t"     , Runtime::ForeignInt32TypeObject);
    addObject("int64_t"     , Runtime::ForeignInt64TypeObject);
    addObject("uint8_t"     , Runtime::ForeignUInt8TypeObject);
    addObject("uint16_t"    , Runtime::ForeignUInt16TypeObject);
    addObject("uint32_t"    , Runtime::ForeignUInt32TypeObject);
    addObject("uint64_t"    , Runtime::ForeignUInt64TypeObject);
    addObject("float"       , Runtime::ForeignFloatTypeObject);
    addObject("double"      , Runtime::ForeignDoubleTypeObject);
    addObject("long_double" , Runtime::ForeignLongDoubleTypeObject);

    /* raw pointer types */
    addObject("void_p"       , Runtime::ForeignRawPointerTypeObject);
    addObject("const_void_p" , Runtime::ForeignConstRawPointerTypeObject);

    /* string types */
    addObject("char_p"       , Runtime::ForeignCStringTypeObject);
    addObject("const_char_p" , Runtime::ForeignConstCStringTypeObject);

    /* memory access functions */
    addFunction(Runtime::NativeFunctionObject::fromFunction("ref"       , {"value"}                  , &ffiRef      ));
    addFunction(Runtime::NativeFunctionObject::fromFunction("string_at" , {"addr", "size"}, {nullptr}, &ffiStringAt ));
    addFunction(Runtime::NativeFunctionObject::fromFunction("pointer_of", {"type"}                   , &ffiPointerOf));

    /* raw buffer allocator */
    addFunction(Runtime::NativeFunctionObject::fromFunction(
        "create_raw_buffer",
        {"size"},
        Runtime::Object::newObject<Runtime::ForeignRawBuffer, size_t>
    ));

    /* string buffer allocator */
    addFunction(Runtime::NativeFunctionObject::fromFunction(
        "create_string_buffer",
        {"length"},
        Runtime::Object::newObject<Runtime::ForeignStringBuffer, size_t>
    ));
}
}
