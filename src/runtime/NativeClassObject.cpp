#include "runtime/StringObject.h"
#include "runtime/NativeClassObject.h"

// TODO: remove this
#include <iostream>

namespace RedScript::Runtime
{
/* type object for native class */
TypeRef NativeClassTypeObject;

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

NativeClassObject::NativeClassObject(
    const std::string &code,
    Runtime::Reference<Runtime::MapObject> &&options
) : Object(NativeClassTypeObject), _tcc(tcc_new())
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
        self->_errors.emplace_back(file, line, (isWarning != 0), message);
    });

    /* compile the code */
    if (tcc_compile_string_ex(_tcc, "<native>", code.data(), code.size()) < 0)
        for (const auto &ctx : _errors)
            if (!(ctx->isWarning()))
                throw ctx;

    /* link the code in memory */
    if (tcc_relocate(_tcc) < 0)
        throw Exceptions::RuntimeError("Unable to link native object in memory");

    tcc_list_types(_tcc, [](TCCState *s, const char *name, TCCType *type, void *) -> char
    {
        std::cout << "* TYPE :: " << type2str(s, type, name) << std::endl;
        return 1;
    }, nullptr);

    tcc_list_functions(_tcc, [](TCCState *s, const char *name, TCCFunction *func, void *) -> char
    {
        void *fp = tcc_function_get_addr(s, func);
        size_t nargs = tcc_function_get_nargs(func);
        TCCType *rettype = tcc_function_get_return_type(func);
        std::cout << "----------- FUNCTION \"" << name << "\" -----------" << std::endl;
        std::cout << "tcc-function-get-addr(): " << fp << std::endl;
        std::cout << "tcc-function-get-nargs(): " << nargs << std::endl;
        std::cout << "tcc-function-get-return-type(): " << type2str(s, rettype, nullptr) << std::endl;

        for (size_t i = 0; i < nargs; i++)
        {
            TCCType *t = tcc_function_get_arg_type(func, i);
            const char *n = tcc_function_get_arg_name(func, i);
            std::cout << "- arg " << i << ": name=" << std::string(n) << ", type=" << type2str(s, t, nullptr) << std::endl;
        }

        return 1;
    }, nullptr);
}

void NativeClassObject::shutdown(void)
{
    /* clear type instance */
    NativeClassTypeObject = nullptr;
}

void NativeClassObject::initialize(void)
{
    /* native class type object */
    static NativeClassType nullType;
    NativeClassTypeObject = Reference<NativeClassType>::refStatic(nullType);
}
}
