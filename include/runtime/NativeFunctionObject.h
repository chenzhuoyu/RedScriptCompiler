#ifndef REDSCRIPT_RUNTIME_NATIVEFUNCTIONOBJECT_H
#define REDSCRIPT_RUNTIME_NATIVEFUNCTIONOBJECT_H

#include <string>
#include <vector>
#include <stdexcept>

#include "utils/NFI.h"
#include "runtime/Object.h"

namespace RedScript::Runtime
{
class NativeFunctionType : public NativeType
{
public:
    explicit NativeFunctionType() : NativeType("native_function") {}

protected:
    virtual void addBuiltins(void) override;
    virtual void clearBuiltins(void) override {}

/*** Native Object Protocol ***/

public:
    virtual std::string nativeObjectRepr(ObjectRef self) override;
    virtual ObjectRef   nativeObjectInvoke(ObjectRef self, Reference<TupleObject> args, Reference<MapObject> kwargs) override;

};

/* type object for native function */
extern TypeRef NativeFunctionTypeObject;

/* native function types */
typedef std::function<ObjectRef(void)> NullaryFunction;
typedef std::function<ObjectRef(ObjectRef)> UnaryFunction;
typedef std::function<ObjectRef(ObjectRef, ObjectRef)> BinaryFunction;
typedef std::function<ObjectRef(ObjectRef, ObjectRef, ObjectRef)> TernaryFunction;
typedef std::function<ObjectRef(Utils::NFI::VariadicArgs, Utils::NFI::KeywordArgs)> NativeFunction;

/* function reference type */
class NativeFunctionObject;
typedef Reference<NativeFunctionObject> FunctionRef;

class NativeFunctionObject : public Object
{
    std::string _name;
    NativeFunction _function;

public:
    virtual ~NativeFunctionObject() = default;
    explicit NativeFunctionObject(const std::string &name) : NativeFunctionObject(name, nullptr) {}
    explicit NativeFunctionObject(const std::string &name, NativeFunction function) : Object(NativeFunctionTypeObject), _name(name), _function(function) {}

public:
    std::string &name(void) { return _name; }
    NativeFunction &function(void) { return _function; }

public:
    static void shutdown(void);
    static void initialize(void);

public:
    static FunctionRef newNullary (const std::string &name, NullaryFunction function);
    static FunctionRef newUnary   (const std::string &name, UnaryFunction   function);
    static FunctionRef newBinary  (const std::string &name, BinaryFunction  function);
    static FunctionRef newTernary (const std::string &name, TernaryFunction function);
    static FunctionRef newVariadic(const std::string &name, NativeFunction  function) { return Object::newObject<NativeFunctionObject>(name, function); }

public:
    template <typename Function>
    static FunctionRef fromFunction(const std::string &name, Function &&function)
    {
        /* wrap the function-like object as `std::function` */
        return fromFunction(name, Utils::NFI::forwardAsFunction(std::forward<Function>(function)));
    }

public:
    template <typename Function>
    static FunctionRef fromFunction(const std::string &name, Utils::NFI::KeywordNames keywords, Function &&function)
    {
        /* wrap the function-like object as `std::function` */
        return fromFunction(name, std::move(keywords), Utils::NFI::forwardAsFunction(std::forward<Function>(function)));
    }

public:
    template <typename Function>
    static FunctionRef fromFunction(const std::string &name, Utils::NFI::DefaultValues defaults, Function &&function)
    {
        /* wrap the function-like object as `std::function` */
        return fromFunction(name, std::move(defaults), Utils::NFI::forwardAsFunction(std::forward<Function>(function)));
    }

public:
    template <typename Function>
    static FunctionRef fromFunction(
        const std::string          &name,
        Utils::NFI::KeywordNames    keywords,
        Utils::NFI::DefaultValues   defaults,
        Function                  &&function)
    {
        /* wrap the function-like object as `std::function` */
        return fromFunction(
            name,
            std::move(keywords),
            std::move(defaults),
            Utils::NFI::forwardAsFunction(std::forward<Function>(function))
        );
    }

public:
    template <typename Ret, typename ... Args>
    static FunctionRef fromFunction(const std::string &name, std::function<Ret(Args ...)> function)
    {
        /* without both keywords and default values */
        Utils::NFI::KeywordNames names(sizeof ... (Args));
        return fromFunction(name, std::move(names), Utils::NFI::DefaultValues(), std::move(function));
    }

public:
    template <typename Ret, typename ... Args>
    static FunctionRef fromFunction(
        const std::string            &name,
        Utils::NFI::KeywordNames      keywords,
        std::function<Ret(Args ...)>  function)
    {
        /* with keywords, without default values */
        return fromFunction(name, std::move(keywords), Utils::NFI::DefaultValues(), std::move(function));
    }

public:
    template <typename Ret, typename ... Args>
    static FunctionRef fromFunction(
        const std::string            &name,
        Utils::NFI::DefaultValues     defaults,
        std::function<Ret(Args ...)>  function)
    {
        /* with defaults, without keywords */
        Utils::NFI::KeywordNames names(sizeof ... (Args));
        return fromFunction(name, std::move(names), std::move(defaults), std::move(function));
    }

public:
    template <typename Ret, typename ... Args>
    static FunctionRef fromFunction(
        const std::string            &name,
        Utils::NFI::KeywordNames      keywords,
        Utils::NFI::DefaultValues     defaults,
        std::function<Ret(Args ...)>  function)
    {
        /* check for default count */
        if (keywords.size() < defaults.size())
            throw std::invalid_argument("default count error");

        /* check for keyword count */
        if (keywords.size() != sizeof ... (Args))
            throw std::invalid_argument("keyword count mismatch");

        /* wrap as variadic function */
        return newVariadic(name, [=](Utils::NFI::VariadicArgs args, Utils::NFI::KeywordArgs kwargs)
        {
            /* result type might be `void`, so need a template SFINAE here */
            return Utils::NFI::MetaFunction<Ret, Args ...>::invoke(function, args, kwargs, keywords, defaults);
        });
    }
};
}

#endif /* REDSCRIPT_RUNTIME_NATIVEFUNCTIONOBJECT_H */
