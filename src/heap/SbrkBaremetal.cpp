/* Bare-metal/RTOS sbrk() core -- BDWGC's NOSYS `GET_MEM` path
 * (`GC_unix_sbrk_get_mem`, third_party/GCutil/os_dep.c) calls plain
 * `sbrk()` unconditionally; on a target with no OS-managed heap there is
 * nothing to back it but a raw pointer bump over a linker-reserved
 * region. Every RTOS port needs the exact same ~15-line bump-pointer
 * logic over its own `__gc_heap_start__`/`__gc_heap_size__` linker
 * symbols (identical names, different underlying memory region per
 * port's own linker script) -- shared here instead of being copy-pasted
 * per port. A new RTOS port does not need to write this logic itself.
 *
 * Lives under src/heap/, not third_party/yarr/: this file feeds BDWGC's
 * heap growth, consumed indirectly via src/heap/Heap.cpp's GC_init() call
 * -- it has nothing to do with yarr (WebKit's regex engine subsystem,
 * which third_party/yarr/ actually is) and only lived there originally
 * out of convenience while the bare-metal porting layer was being built.
 * `third_party/GCutil` itself is deliberately not an option either: it's
 * the vendored, rebase-sensitive upstream BDWGC mirror this repo keeps a
 * near-zero diff against, so new Escargot-side files don't belong there.
 *
 * This heap is grow-only: memory is never returned to the OS by this
 * path (see docs/porting/RTOS_PORTING_GUIDE.md, §1) -- there is no
 * shrink case to support, `increment < 0` is simply rejected.
 *
 * This file deliberately does NOT export a symbol literally named
 * `sbrk`/`_sbrk` itself -- which libc-facing name is needed differs by
 * toolchain (newlib's own libc.a already provides a `sbrk()` that
 * forwards to `_sbrk()`; NuttX's flat-build apps have neither, since
 * umm_sbrk.c is gated on CONFIG_BUILD_KERNEL, and need `sbrk()`
 * directly), AND because this file is compiled as part of the engine's
 * own static library (escargot-lib): a static-library member is only
 * pulled into the final link if something already has an outstanding
 * *undefined reference* to one of the symbols it defines at the point
 * the linker scans that archive. newlib's own `libnosys`/`libc.a`
 * already defines both `sbrk` and a dummy `_sbrk`, which would already
 * satisfy those names before the archive is even scanned -- so a
 * same-named definition buried in here would silently be skipped by the
 * linker in favor of newlib's non-functional stub (verified: this is
 * exactly what happened on a first attempt at this consolidation --
 * `_sbrk` resolved to libnosys's `libgloss/libnosys/sbrk.c` stub instead
 * of this file, and the FreeRTOS sample hung inside `GC_init()` as a
 * result). Exporting a neutral, non-colliding name instead and letting
 * each port's own (always directly linked, never archived) platform
 * file define the 1-line real `sbrk()`/`_sbrk()` forwarder guarantees
 * the reference exists and this archive member gets pulled in.
 *
 * Self-guarded so this compiles to an empty translation unit on every
 * non-OS_BAREMETAL build -- the main engine build and both existing RTOS
 * ports' own CMakeLists.txt all already glob every .cpp file under src/
 * recursively (FILE(GLOB_RECURSE ... "src" ".cpp")), so this file is
 * picked up automatically with no build-file change needed, the same way
 * it previously was via third_party/yarr's own .cpp glob. */
#if defined(OS_BAREMETAL)

#include <stdint.h>

namespace {

uint8_t* g_baremetalHeapPtr = nullptr;
uint8_t* g_baremetalHeapEnd = nullptr;

} // anonymous namespace

extern "C" void* escargotBaremetalSbrk(intptr_t increment)
{
    extern uint8_t __gc_heap_start__[];
    extern uint8_t __gc_heap_size__[];

    if (!g_baremetalHeapPtr) {
        g_baremetalHeapPtr = __gc_heap_start__;
        g_baremetalHeapEnd = __gc_heap_start__ + (uintptr_t)__gc_heap_size__;
    }

    if (increment == 0) {
        return g_baremetalHeapPtr;
    }
    if (increment < 0 || g_baremetalHeapPtr + increment > g_baremetalHeapEnd) {
        return (void*)-1;
    }

    uint8_t* prev = g_baremetalHeapPtr;
    g_baremetalHeapPtr += increment;
    return prev;
}

#endif /* OS_BAREMETAL */
