#include "engine/Thread.h"
#include "exceptions/BaseException.h"

namespace RedScript::Exceptions
{
BaseException::BaseException(const char *name, const std::string &message) : _name(name), _message(message)
{
    /* frame count and buffer */
    auto size = Engine::Thread::self()->frames.size();
    auto *stack = Engine::Thread::self()->frames.data();

    /* reserve space for traceback */
    _traceback.resize(size);

    /* traverse each frame */
    for (size_t i = 0; i < size; i++)
    {
        _traceback[i].row  = stack[i]->line().first;
        _traceback[i].col  = stack[i]->line().second;
        _traceback[i].file = stack[i]->file();
        _traceback[i].name = stack[i]->name();
    }
}
}
