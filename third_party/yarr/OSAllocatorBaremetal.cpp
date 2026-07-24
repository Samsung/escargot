/* Bare-metal/RTOS OSAllocator stub -- yarr's BumpPointerAllocator backed
 * by plain malloc()/free(). Used whenever OS_BAREMETAL is defined (see
 * src/runtime/Platform.h and docs/porting/RTOS_PORTING_GUIDE.md); no
 * mmap()/mprotect() exist on these targets, and none of reserve/commit/
 * decommit granularity actually matters without an MMU, so this same
 * implementation is shared by every RTOS port -- it used to be
 * copy-pasted per port (escargot-rtos's own src/OSAllocatorBaremetal.cpp,
 * nuttx-escargot's own OSAllocatorNuttx.cxx, byte-for-byte identical)
 * before being consolidated here. A new RTOS port does not need its own
 * copy of this file at all.
 *
 * Self-guarded (like OSAllocatorPosix.cpp/OSAllocatorWin.cpp) so this
 * compiles to an empty translation unit on every non-OS_BAREMETAL build
 * -- the main engine CMakeLists globs every .cpp file under
 * third_party/yarr unconditionally and relies on exactly this pattern to
 * pick the right one. */
#if defined(OS_BAREMETAL)

#include <stdlib.h>
#include "OSAllocator.h"

namespace WTF {

void* OSAllocator::reserveUncommitted(size_t bytes, Usage, bool, bool, bool)
{
    return malloc(bytes);
}

void* OSAllocator::reserveAndCommit(size_t bytes, Usage, bool, bool, bool)
{
    return malloc(bytes);
}

void OSAllocator::releaseDecommitted(void* ptr, size_t)
{
    free(ptr);
}

void OSAllocator::commit(void*, size_t, bool, bool) {}
void OSAllocator::decommit(void*, size_t) {}

} // namespace WTF

#endif /* OS_BAREMETAL */
