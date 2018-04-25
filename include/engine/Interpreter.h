#ifndef REDSCRIPT_ENGINE_INTERPRETER_H
#define REDSCRIPT_ENGINE_INTERPRETER_H

#include "runtime/Object.h"
#include "runtime/CodeObject.h"

namespace RedScript::Engine
{
class Interpreter
{
public:
    Runtime::ObjectRef eval(Runtime::Reference<Runtime::CodeObject> code);

};
}

#endif /* REDSCRIPT_ENGINE_INTERPRETER_H */
