#include "lockfree/HazardPointer.h"

namespace RedScript::LockFree
{
std::atomic_size_t HazardThread::_threadCount(0);
HazardThread::ThreadData *HazardThread::_threadData[HazardThread::MAX_THREADS] = {nullptr};
}
