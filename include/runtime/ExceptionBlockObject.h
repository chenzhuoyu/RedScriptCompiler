#ifndef REDSCRIPT_RUNTIME_EXCEPTIONBLOCKOBJECT_H
#define REDSCRIPT_RUNTIME_EXCEPTIONBLOCKOBJECT_H

#include <string>
#include <cstdint>

#include "runtime/Object.h"

namespace RedScript::Runtime
{
class ExceptionBlockType : public Type
{
public:
    explicit ExceptionBlockType() : Type("_ExceptionBlock") {}

};

/* type object for exception block */
extern TypeRef ExceptionBlockTypeObject;

class ExceptionBlockObject : public Object
{
    friend class ExceptionBlockType;

private:
    uint32_t _except = 0;
    uint32_t _finally = 0;

private:
    bool _hasExcept = false;
    bool _hasFinally = false;

public:
    virtual ~ExceptionBlockObject() = default;
    explicit ExceptionBlockObject() : Object(ExceptionBlockTypeObject) {}

public:
    uint32_t except(void) const { return _except; }
    uint32_t finally(void) const { return _finally; }

public:
    bool hasExcept(void) const { return _hasExcept; }
    bool hasFinally(void) const { return _hasFinally; }

public:
    void setExcept(uint32_t value) { _except = value; _hasExcept = true; }
    void setFinally(uint32_t value) { _finally = value; _hasFinally = true; }

public:
    static void shutdown(void) {}
    static void initialize(void);

};
}

#endif /* REDSCRIPT_RUNTIME_EXCEPTIONBLOCKOBJECT_H */
