#ifndef REDSCRIPT_RUNTIME_NATIVEFUNCTIONOBJECT_H
#define REDSCRIPT_RUNTIME_NATIVEFUNCTIONOBJECT_H

#include <string>
#include <vector>
#include <functional>

#include "utils/NFI.h"
#include "runtime/Object.h"

namespace RedScript::Runtime
{
class NativeFunctionType : public Type
{
public:
    explicit NativeFunctionType() : Type("native_function") {}

protected:
    virtual void addBuiltins(void) override;
    virtual void clearBuiltins(void) override {}

/*** Native Object Protocol ***/

public:
    virtual ObjectRef nativeObjectInvoke(ObjectRef self, Reference<TupleObject> args, Reference<MapObject> kwargs) override;

};

/* type object for native function */
extern TypeRef NativeFunctionTypeObject;

/* native function types */
typedef std::function<ObjectRef(void)> NullaryFunction;
typedef std::function<ObjectRef(ObjectRef)> UnaryFunction;
typedef std::function<ObjectRef(ObjectRef, ObjectRef)> BinaryFunction;
typedef std::function<ObjectRef(ObjectRef, ObjectRef, ObjectRef)> TernaryFunction;
typedef std::function<ObjectRef(Utils::NFI::VariadicArgs, Utils::NFI::KeywordArgs)> NativeFunction;

class NativeFunctionObject : public Object
{
    NativeFunction _function;

public:
    virtual ~NativeFunctionObject() = default;
    explicit NativeFunctionObject(NativeFunction function) : Object(NativeFunctionTypeObject), _function(function) {}

public:
    NativeFunction function(void) const { return _function; }

public:
    static void shutdown(void) {}
    static void initialize(void);

public:
    static ObjectRef newNullary (NullaryFunction function);
    static ObjectRef newUnary   (UnaryFunction   function);
    static ObjectRef newBinary  (BinaryFunction  function);
    static ObjectRef newTernary (TernaryFunction function);
    static ObjectRef newVariadic(NativeFunction  function) { return Object::newObject<NativeFunctionObject>(function); }

public:
    template <typename Function>
    static ObjectRef fromFunction(Function &&function)
    {
        /* wrap the function-like object as `std::function` */
        return fromFunction(Utils::NFI::forwardAsFunction(std::forward<Function>(function)));
    }

public:
    template <typename Function>
    static ObjectRef fromFunction(Utils::NFI::KeywordNames keywords, Function &&function)
    {
        /* wrap the function-like object as `std::function` */
        return fromFunction(std::move(keywords), Utils::NFI::forwardAsFunction(std::forward<Function>(function)));
    }

public:
    template <typename Function>
    static ObjectRef fromFunction(Utils::NFI::DefaultValues defaults, Function &&function)
    {
        /* wrap the function-like object as `std::function` */
        return fromFunction(std::move(defaults), Utils::NFI::forwardAsFunction(std::forward<Function>(function)));
    }

public:
    template <typename Function>
    static ObjectRef fromFunction(Utils::NFI::KeywordNames keywords, Utils::NFI::DefaultValues defaults, Function &&function)
    {
        /* wrap the function-like object as `std::function` */
        return fromFunction(
            std::move(keywords),
            std::move(defaults),
            Utils::NFI::forwardAsFunction(std::forward<Function>(function))
        );
    }

public:
    template <typename Ret, typename ... Args>
    static ObjectRef fromFunction(std::function<Ret(Args ...)> function)
    {
        /* without both keywords and default values */
        Utils::NFI::KeywordNames names(sizeof ... (Args));
        return fromFunction(std::move(names), Utils::NFI::DefaultValues(), std::move(function));
    }

public:
    template <typename Ret, typename ... Args>
    static ObjectRef fromFunction(
        Utils::NFI::KeywordNames     keywords,
        std::function<Ret(Args ...)> function)
    {
        /* with keywords, without default values */
        return fromFunction(std::move(keywords), Utils::NFI::DefaultValues(), std::move(function));
    }

public:
    template <typename Ret, typename ... Args>
    static ObjectRef fromFunction(
        Utils::NFI::DefaultValues    defaults,
        std::function<Ret(Args ...)> function)
    {
        /* with defaults, without keywords */
        Utils::NFI::KeywordNames names(sizeof ... (Args));
        return fromFunction(std::move(names), std::move(defaults), std::move(function));
    }

public:
    template <typename Ret, typename ... Args>
    static ObjectRef fromFunction(
        Utils::NFI::KeywordNames     keywords,
        Utils::NFI::DefaultValues    defaults,
        std::function<Ret(Args ...)> function)
    {
        /* check for default count */
        if (keywords.size() < defaults.size())
            throw std::invalid_argument("default count error");

        /* check for keyword count */
        if (keywords.size() != sizeof ... (Args))
            throw std::invalid_argument("keyword count mismatch");

        /* wrap as variadic function */
        return newVariadic([=](Utils::NFI::VariadicArgs args, Utils::NFI::KeywordArgs kwargs)
        {
            /* result type might be `void`, so need a template SFINAE here */
            return Utils::NFI::MetaFunction<Ret, Args ...>::invoke(function, args, kwargs, keywords, defaults);
        });
    }
};
}

#endif /* REDSCRIPT_RUNTIME_NATIVEFUNCTIONOBJECT_H */
