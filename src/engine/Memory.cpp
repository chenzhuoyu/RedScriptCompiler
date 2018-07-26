#include <cstdint>
#include "engine/Memory.h"

/*** System `new` and `delete` hooks ***/

void *operator new(size_t size) { return RedScript::Engine::Memory::alloc(size); }
void *operator new[](size_t size) { return RedScript::Engine::Memory::alloc(size); }

void operator delete(void *ptr) noexcept { RedScript::Engine::Memory::free(ptr); }
void operator delete[](void *ptr) noexcept { RedScript::Engine::Memory::free(ptr); }
