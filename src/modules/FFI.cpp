#include "modules/FFI.h"
#include "runtime/ExceptionObject.h"
#include "runtime/NativeClassObject.h"
#include "runtime/NativeFunctionObject.h"

namespace RedScript::Modules
{
/* FFI module object */
Runtime::ModuleRef FFIModule;

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

    /* value reference function */
    addFunction(Runtime::NativeFunctionObject::fromFunction("ref", {"value"}, [](Runtime::ObjectRef value)
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
    }));

    /* pointer wrapper function */
    addFunction(Runtime::NativeFunctionObject::fromFunction("pointer_of", {"type"}, [](Runtime::ObjectRef type)
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
    }));

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
