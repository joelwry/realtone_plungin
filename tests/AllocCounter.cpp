#include "AllocCounter.h"

#include <cstdlib>
#include <new>

namespace {
thread_local bool g_tracking = false;
thread_local long g_count = 0;
}  // namespace

namespace alloc_counter {
void begin()
{
    g_count = 0;
    g_tracking = true;
}
long end()
{
    g_tracking = false;
    return g_count;
}
}  // namespace alloc_counter

void* operator new(std::size_t size)
{
    if (g_tracking)
        ++g_count;
    if (void* p = std::malloc(size ? size : 1))
        return p;
    throw std::bad_alloc();
}

void* operator new[](std::size_t size)
{
    if (g_tracking)
        ++g_count;
    if (void* p = std::malloc(size ? size : 1))
        return p;
    throw std::bad_alloc();
}

void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
