/* Bare-metal/RTOS adapter for WTF's OSAllocator interface. */
#include "Escargot.h"
#include "OSAllocator.h"
#include "util/OSMemory.h"

#if defined(OS_BAREMETAL)
namespace WTF {

void* OSAllocator::reserveUncommitted(size_t bytes, Usage usage, bool writable, bool executable, bool includesGuardPages)
{
    return Escargot::OSMemory::reserveUncommitted(bytes, writable, executable, includesGuardPages, static_cast<int>(usage));
}

void* OSAllocator::reserveAndCommit(size_t bytes, Usage usage, bool writable, bool executable, bool includesGuardPages)
{
    return Escargot::OSMemory::reserve(bytes, writable, executable, includesGuardPages, static_cast<int>(usage));
}

void OSAllocator::commit(void* address, size_t bytes, bool writable, bool executable)
{
    Escargot::OSMemory::commit(address, bytes, writable, executable);
}

void OSAllocator::decommit(void* address, size_t bytes)
{
    Escargot::OSMemory::decommit(address, bytes);
}

void OSAllocator::releaseDecommitted(void* address, size_t bytes)
{
    Escargot::OSMemory::release(address, bytes);
}

} // namespace WTF
#endif
