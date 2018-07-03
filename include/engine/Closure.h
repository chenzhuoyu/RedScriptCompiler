#ifndef REDSCRIPT_ENGINE_CLOSURE_H
#define REDSCRIPT_ENGINE_CLOSURE_H

#include <memory>
#include <vector>

#include "runtime/Object.h"

namespace RedScript::Engine
{
class Closure;
typedef std::shared_ptr<Closure> ClosureRef;

class Closure final
{
    struct Tag {};

private:
    size_t _id;
    Runtime::ObjectRef _object;
    std::vector<Runtime::ObjectRef> *_locals;

public:
    struct Context
    {
        ClosureRef ref;

    public:
       ~Context() { ref->freeze(); }
        Context(Runtime::ObjectRef object) : ref(Closure::ref(object)) {}
        Context(std::vector<Runtime::ObjectRef> *locals, size_t id) : ref(Closure::ref(locals, id)) {}

    };

public:
    Closure(Runtime::ObjectRef object, Tag) : _id(0), _locals(nullptr), _object(object) {}
    Closure(std::vector<Runtime::ObjectRef> *locals, size_t id, Tag) : _id(id), _locals(locals) {}

public:
    static ClosureRef ref(Runtime::ObjectRef object) { return std::make_shared<Closure>(object, Tag()); }
    static ClosureRef ref(std::vector<Runtime::ObjectRef> *locals, size_t id) { return std::make_shared<Closure>(locals, id, Tag()); }

public:
    void freeze(void)
    {
        if (_locals)
        {
            _object = _locals->at(_id);
            _locals = nullptr;
        }
    }

public:
    Runtime::ObjectRef get(void) const
    {
        if (!_locals)
            return _object;
        else
            return _locals->at(_id);
    }
};
}

#endif /* REDSCRIPT_ENGINE_CLOSURE_H */
