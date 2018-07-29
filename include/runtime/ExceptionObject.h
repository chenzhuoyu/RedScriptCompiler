#ifndef REDSCRIPT_RUNTIME_EXCEPTIONOBJECT_H
#define REDSCRIPT_RUNTIME_EXCEPTIONOBJECT_H

#include "utils/Strings.h"
#include "runtime/Object.h"

namespace RedScript::Runtime
{
class MapObject;
class TupleObject;

class ExceptionType : public NativeType
{
public:
    using NativeType::NativeType;

public:
    virtual ObjectRef nativeObjectNew(TypeRef type, Reference<TupleObject> args, Reference<MapObject> kwargs) override;
    virtual ObjectRef nativeObjectInit(ObjectRef self, Reference<TupleObject> args, Reference<MapObject> kwargs) override;

public:
    static void shutdown(void);
    static void initialize(void);

protected:
    virtual void addBuiltins(void) override;
    virtual void clearBuiltins(void) override {}

};

class ExceptionObject : public Object
{
    struct Traceback
    {
        int row;
        int col;
        std::string file;
        std::string name;
    };

private:
    std::string _message;
    std::vector<Traceback> _traceback;
    Reference<ExceptionObject> _parent;

public:
    virtual ~ExceptionObject() = default;
    explicit ExceptionObject(TypeRef type, const std::string &message);

public:
    auto &parent(void) { return _parent; }
    auto &message(void) { return _message; }
    auto &traceback(void) { return _traceback; }

public:
    const auto &name(void) const { return const_cast<ExceptionObject *>(this)->type()->name(); }
    const auto &message(void) const { return _message; }
    const auto &traceback(void) const { return _traceback; }

public:
    virtual std::string what(void) const { return Utils::Strings::format("%s: %s", name(), _message); }
    virtual std::string format(void) const;

};

/* built-in base exceptions */
extern TypeRef ExceptionTypeObject;
extern TypeRef BaseExceptionTypeObject;

/* built-in exception objects */
extern TypeRef NameErrorTypeObject;
extern TypeRef TypeErrorTypeObject;
extern TypeRef IndexErrorTypeObject;
extern TypeRef ValueErrorTypeObject;
extern TypeRef SyntaxErrorTypeObject;
extern TypeRef RuntimeErrorTypeObject;
extern TypeRef InternalErrorTypeObject;
extern TypeRef AttributeErrorTypeObject;
extern TypeRef ZeroDivisionErrorTypeObject;
extern TypeRef NativeSyntaxErrorTypeObject;

/* built-in non-error exceptions */
extern TypeRef SystemExitTypeObject;
extern TypeRef StopIterationTypeObject;

/** Simple Exceptions **/

#define RSX_DEFINE(name)                                                                                    \
    struct name ## Impl : public ExceptionObject                                                            \
    {                                                                                                       \
        virtual ~name ## Impl() = default;                                                                  \
        explicit name ## Impl(const std::string &message) : ExceptionObject(name ## TypeObject, message) {} \
    }

/* built-in base exceptions */
RSX_DEFINE(Exception);
RSX_DEFINE(BaseException);

/* built-in exception objects */
RSX_DEFINE(TypeError);
RSX_DEFINE(IndexError);
RSX_DEFINE(ValueError);
RSX_DEFINE(RuntimeError);
RSX_DEFINE(InternalError);
RSX_DEFINE(AttributeError);
RSX_DEFINE(ZeroDivisionError);

#undef RSX_DEFINE

/** Special Exceptions **/

struct NameErrorType : public ExceptionType
{
    virtual ~NameErrorType() = default;
    explicit NameErrorType() : ExceptionType("NameError", ExceptionTypeObject) {}

public:
    virtual ObjectRef nativeObjectNew(TypeRef type, Reference<TupleObject> args, Reference<MapObject> kwargs) override;
    virtual ObjectRef nativeObjectInit(ObjectRef self, Reference<TupleObject> args, Reference<MapObject> kwargs) override;

};

class NameErrorImpl : public ExceptionObject
{
    int _row;
    int _col;

public:
    virtual ~NameErrorImpl() = default;
    explicit NameErrorImpl(int row, int col, const std::string &message) :
        ExceptionObject(NameErrorTypeObject, message), _row(row), _col(col) {}

public:
    int row(void) const { return _row; }
    int col(void) const { return _col; }

public:
    virtual std::string what(void) const override
    {
        return Utils::Strings::format(
            "NameError: %s at %d:%d",
            message(),
            _row,
            _col
        );
    }
};

struct SyntaxErrorType : public ExceptionType
{
    virtual ~SyntaxErrorType() = default;
    explicit SyntaxErrorType() : ExceptionType("SyntaxError", ExceptionTypeObject) {}
    explicit SyntaxErrorType(const std::string &name) : ExceptionType(name, SyntaxErrorTypeObject) {}

public:
    virtual ObjectRef nativeObjectNew(TypeRef type, Reference<TupleObject> args, Reference<MapObject> kwargs) override;
    virtual ObjectRef nativeObjectInit(ObjectRef self, Reference<TupleObject> args, Reference<MapObject> kwargs) override;

};

class SyntaxErrorImpl : public ExceptionObject
{
    int _row;
    int _col;

public:
    virtual ~SyntaxErrorImpl() = default;
    explicit SyntaxErrorImpl(int row, int col, const std::string &message) : SyntaxErrorImpl(SyntaxErrorTypeObject, row, col, message) {}
    explicit SyntaxErrorImpl(TypeRef type, int row, int col, const std::string &message) : ExceptionObject(type, message), _row(row), _col(col) {}

public:
    int row(void) const { return _row; }
    int col(void) const { return _col; }

public:
    virtual std::string what(void) const override
    {
        return Utils::Strings::format(
            "SyntaxError: %s at %d:%d",
            message(),
            _row,
            _col
        );
    }
};

struct NativeSyntaxErrorType : public SyntaxErrorType
{
    virtual ~NativeSyntaxErrorType() = default;
    explicit NativeSyntaxErrorType() : SyntaxErrorType("NativeSyntaxError") {}

public:
    virtual ObjectRef nativeObjectNew(TypeRef type, Reference<TupleObject> args, Reference<MapObject> kwargs) override;
    virtual ObjectRef nativeObjectInit(ObjectRef self, Reference<TupleObject> args, Reference<MapObject> kwargs) override;

};

class NativeSyntaxErrorImpl : public SyntaxErrorImpl
{
    bool _isWarning;
    std::string _filename;

public:
    virtual ~NativeSyntaxErrorImpl() = default;
    explicit NativeSyntaxErrorImpl(const std::string &filename, int row, bool isWarning, const std::string &message) :
        SyntaxErrorImpl(NativeSyntaxErrorTypeObject, row, -1, message), _isWarning(isWarning), _filename(filename) {}

public:
    bool isWarning(void) const { return _isWarning; }
    const std::string &filename(void) const { return _filename; }

public:
    virtual std::string what(void) const override
    {
        return Utils::Strings::format(
            "SyntaxError: (%s:%d) %s :: %s",
            _filename,
            row(),
            _isWarning ? "WARNING" : "ERROR",
            message()
        );
    }
};

/** Non-error Exceptions **/

struct SystemExitType : public ExceptionType
{
    virtual ~SystemExitType() = default;
    explicit SystemExitType() : ExceptionType("SystemExit", BaseExceptionTypeObject) {}

public:
    virtual ObjectRef nativeObjectNew(TypeRef type, Reference<TupleObject> args, Reference<MapObject> kwargs) override;

};

struct SystemExitImpl : public ExceptionObject
{
    virtual ~SystemExitImpl() = default;
    explicit SystemExitImpl() : ExceptionObject(SystemExitTypeObject, "System exit") {}

public:
    virtual std::string what(void) const override { return "SystemExit"; }

};

struct StopIterationType : public ExceptionType
{
    virtual ~StopIterationType() = default;
    explicit StopIterationType() : ExceptionType("StopIteration", BaseExceptionTypeObject) {}

public:
    virtual ObjectRef nativeObjectNew(TypeRef type, Reference<TupleObject> args, Reference<MapObject> kwargs) override;

};

struct StopIterationImpl : public ExceptionObject
{
    virtual ~StopIterationImpl() = default;
    explicit StopIterationImpl() : ExceptionObject(StopIterationTypeObject, "Stop iteration") {}

public:
    virtual std::string what(void) const override { return "StopIteration"; }

};

/** Exception Wrapper Classes **/

namespace Exceptions
{
struct Throwable : public std::exception
{
    virtual const char *what() const noexcept override
    {
        static thread_local std::string what;
        return (what = Utils::Strings::format("<Throwable at %p>", static_cast<const void *>(this))).c_str();
    }
};

template <typename Exception, typename ... Args>
struct ThrowableReference : public Reference<Exception>, public Throwable
{
    explicit ThrowableReference(Args ... args) : Reference<Exception>::Reference(
        typename Reference<Exception>::TagSubRef(),
        std::forward<Args>(args) ...
    ) {}

public:
    virtual const char *what() const noexcept override
    {
        static thread_local std::string what;
        return (what = this->get()->what()).c_str();
    }
};

/* built-in base exceptions */
using Exception         = ThrowableReference<ExceptionImpl          , const std::string &>;
using BaseException     = ThrowableReference<BaseExceptionImpl      , const std::string &>;

/* built-in exception objects */
using NameError         = ThrowableReference<NameErrorImpl          , int, int, const std::string &>;
using TypeError         = ThrowableReference<TypeErrorImpl          , const std::string &>;
using IndexError        = ThrowableReference<IndexErrorImpl         , const std::string &>;
using ValueError        = ThrowableReference<ValueErrorImpl         , const std::string &>;
using RuntimeError      = ThrowableReference<RuntimeErrorImpl       , const std::string &>;
using InternalError     = ThrowableReference<InternalErrorImpl      , const std::string &>;
using AttributeError    = ThrowableReference<AttributeErrorImpl     , const std::string &>;
using ZeroDivisionError = ThrowableReference<ZeroDivisionErrorImpl  , const std::string &>;
using NativeSyntaxError = ThrowableReference<NativeSyntaxErrorImpl  , const std::string &, int, bool, const std::string &>;

/* built-in non-error exceptions */
using SystemExit        = ThrowableReference<SystemExitImpl>;
using StopIteration     = ThrowableReference<StopIterationImpl>;

/* special case for `SyntaxError` */
struct SyntaxError : public ThrowableReference<SyntaxErrorImpl, int, int, const std::string &>
{
    using ThrowableReference<SyntaxErrorImpl, int, int, const std::string &>::ThrowableReference;
    template <typename T> explicit SyntaxError(T &&t) : SyntaxError(t, "Unexpected token " + t->toString()) {}
    template <typename T> explicit SyntaxError(T &&t, const std::string &message) : ThrowableReference(t->row(), t->col(), message) {}
};
}
}

#endif /* REDSCRIPT_RUNTIME_EXCEPTIONOBJECT_H */
