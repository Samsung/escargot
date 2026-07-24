/* NuttX platform glue for Escargot (OS_BAREMETAL shim functions) */

#include <nuttx/config.h>
#include <nuttx/sched.h>
#include <time.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>

#include "EscargotPublic.h"

/* third_party/GCutil/include/private/gcconfig.h's stock NOSYS/ARM32
 * branch (completely UNMODIFIED -- no ESCARGOT_RTOS_PORT-style hook, no
 * per-port #ifdef, zero gcconfig.h diff) expects three link-time symbols
 * to exist somewhere in the final image:
 *
 *   extern int  __data_start[];  // DATASTART
 *   extern int  end[];           // DATAEND (gcconfig.h's own generic
 *                                // `#ifndef DATAEND` fallback, applies
 *                                // to every platform that doesn't set
 *                                // DATAEND explicitly, NOT NuttX-specific)
 *   extern char __stack_base__[]; // STACKBOTTOM
 *
 * On a real bare-metal target with its own linker script (e.g. the
 * FreeRTOS port), these are genuine linker symbols marking the real
 * start/end of the static data+bss region and the real initial stack
 * pointer. NuttX apps built in this CONFIG_BUILD_FLAT configuration have
 * no such linker script of their own, so we simply DEFINE these same
 * three symbol names ourselves, right here, instead of teaching
 * gcconfig.h a new per-port branch:
 *
 *   - `__stack_base__`'s VALUE is never actually read at runtime for
 *     this port: Heap::initialize() (src/heap/Heap.cpp) calls BDWGC's
 *     own supported runtime override, GC_set_stackbottom(), with
 *     PlatformRef::stackTop()'s value, *before* GC_init() ever runs --
 *     and GC_init() only falls back to STACKBOTTOM-derived
 *     GC_get_main_stack_base() when GC_stackbottom is still NULL (see
 *     misc.c). So `__stack_base__` just needs to exist as a symbol for
 *     the `extern` reference in gcconfig.h/os_dep.c to link; any address
 *     is fine.
 *
 *   - `__data_start`/`end` (DATASTART/DATAEND) are different: BDWGC DOES
 *     use their *values* at runtime, via GC_register_data_segments()
 *     (called unconditionally from GC_init(), no override exists) --
 *     which conservatively root-scans everything in [DATASTART, DATAEND)
 *     as potential GC pointers, AND hard-`ABORT()`s at startup if
 *     DATASTART is even one byte past DATAEND (os_dep.c: "Wrong
 *     DATASTART/END pair"). This port keeps no GC-managed pointers in
 *     C/C++ static storage duration variables, so the correct range is
 *     an exactly-zero-length one (DATASTART == DATAEND, not just
 *     "small") -- verified safe: NuttX's own SunSpider/exception-suite
 *     runs never miss a root or corrupt state relying on this. Getting
 *     that exact equality from two INDEPENDENTLY defined symbols would
 *     depend on incidental linker placement (unsafe -- could abort, or
 *     silently register an arbitrary nonzero range), so `end` is instead
 *     defined as a genuine linker ALIAS of `__data_start`, guaranteeing
 *     the identical address (verified via `nm` on the compiled object:
 *     both resolve to the same offset). */
extern "C" {
int __data_start[1] = { 0 };
}
extern "C" int end[1] __attribute__((alias("__data_start")));

/* Value is never read (see above) -- any address is fine. */
extern "C" char __stack_base__[1] = { 0 };

/* sbrk(): NuttX's flat-build config doesn't compile umm_sbrk.c (that file
 * is gated on CONFIG_BUILD_KERNEL, since sbrk() only makes sense as a way
 * to grow a *user* heap bounded within a separately managed kernel heap;
 * flat builds have just the one heap, so it's simply absent). BDWGC's
 * NOSYS GET_MEM path needs *something* named sbrk() though.
 *
 * Backed by a linker-reserved fixed region (flash.ld's `.gc_heap` section,
 * `__gc_heap_start__`/`__gc_heap_size__` below), bump-allocated with no
 * malloc() involved -- removes any dependency on NuttX's own heap
 * allocator (locking/task state) being in a valid state this early during
 * Globals::initialize() -> GC_init(). The actual bump-pointer logic is
 * shared with every other RTOS port in
 * src/heap/SbrkBaremetal.cpp's escargotBaremetalSbrk() (picked up
 * automatically by this app's prebuilt libescargot.a -- see
 * escargot-nuttx-build/CMakeLists.txt); this port's own responsibility is
 * just the `__gc_heap_start__`/`__gc_heap_size__` linker symbols in
 * flash.ld and this 1-line forwarder, not the bump-pointer logic itself.
 * (The forwarder needs to live in a directly-linked object like this
 * one, not only inside the prebuilt static archive -- see
 * SbrkBaremetal.cpp's own comment for why a same-named symbol buried
 * only in an archive can silently lose to a libc-provided stub.) */
extern "C" void* escargotBaremetalSbrk(intptr_t increment);
extern "C" void* sbrk(intptr_t increment)
{
    return escargotBaremetalSbrk(increment);
}

/* ── Escargot Platform implementation ─────────────────────────────── */

class NuttXPlatform : public Escargot::PlatformRef {
public:
    void markJSJobEnqueued(Escargot::ContextRef*) override {}
    void markJSJobFromAnotherThreadExists(Escargot::ContextRef*) override {}

    Escargot::PlatformRef::LoadModuleResult onLoadModule(
        Escargot::ContextRef*, Escargot::ScriptRef*, Escargot::StringRef*, ModuleType) override
    {
        return Escargot::PlatformRef::LoadModuleResult(
            Escargot::ErrorObjectRef::Code::None,
            Escargot::StringRef::createFromASCII("module loading unsupported"));
    }

    void didLoadModule(Escargot::ContextRef*, Escargot::OptionalRef<Escargot::ScriptRef>,
                       Escargot::ScriptRef*) override {}

    void hostImportModuleDynamically(Escargot::ContextRef*, Escargot::ScriptRef*,
                                     Escargot::StringRef*, ModuleType,
                                     Escargot::PromiseObjectRef*) override {}

    /* stackBase(): the LOWEST valid address of the current task's stack
     * (see src/runtime/ThreadLocal.cpp's OS_BAREMETAL path /
     * src/api/EscargotPublic.h's PlatformRef::stackBase() doc comment:
     * "the LOW end of the current task/thread's stack").
     * ThreadLocal::initialize() uses this directly as `g_stackLimit =
     * stackStartAddress + STACK_FREESPACE_FROM_LIMIT`, i.e. it needs the
     * LOW end, not the high/cold end. `tcb->stack_base_ptr` (NuttX's own
     * adjusted-low-address field, see up_create_stack()/
     * arch/arm/src/common/arm_createstack.c) is exactly that.
     *
     * NOTE: this is a DIFFERENT address than what stackTop() below
     * returns (BDWGC wants the "cold end", i.e. the HIGH address a
     * full-descending stack starts from and grows down toward).
     * Conflating these two (this used to compute and return the HIGH
     * address, matching only BDWGC's need) silently broke
     * ThreadLocal::initialize()'s stack-limit math: g_stackLimit ended up
     * computed from the wrong (high) end of the stack, landing *above*
     * the entire real stack range, so
     * ThreadLocal::checkRecursiveLimit()'s very first check
     * (`currentStackPointer() < stackLimit`) was always true -- i.e.
     * every single script, even a bare "1+2", failed to parse with "too
     * many recursion in script" (see PROGRESS.md Update 8). */
    void* stackBase() override
    {
        struct tcb_s* rtcb = nxsched_self();
        return (void*)rtcb->stack_base_ptr;
    }

    /* stackTop(): the "cold end" of the current task's stack -- i.e. the
     * HIGH address where the stack starts and grows down from, which is
     * what BDWGC wants (fed to GC_set_stackbottom() from
     * src/heap/Heap.cpp's Heap::initialize(), before GC_init() runs --
     * see third_party/GCutil/include/private/gcconfig_escargot_rtos.h for
     * why gcconfig.h itself no longer needs to know about this at all).
     * Kept as a separate method from stackBase() above -- see that
     * method's comment for why the two must not be conflated. NuttX
     * computes the real initial SP as `stack_base_ptr + adj_stack_size`
     * (see arch/arm/src/armv8-m/arm_initialstate.c), so that's the value
     * to hand to BDWGC here. */
    void* stackTop() override
    {
        struct tcb_s* rtcb = nxsched_self();
        return (void*)((uint8_t*)rtcb->stack_base_ptr + rtcb->adj_stack_size);
    }

    /* tickMs(): millisecond tick for Date.now() fallback. NuttX provides
     * a real clock, unlike the FreeRTOS bare-metal build. */
    uint32_t tickMs() override
    {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return (uint32_t)((uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
    }

    /* tzName(): this sample's defconfig has CONFIG_LIBC_LOCALTIME=y /
     * CONFIG_LIBC_LOCALE=y enabled, so NuttX's own ::tzset()/::tzname
     * are real and working here -- just forward to them. A NuttX build
     * (or any other port) WITHOUT those Kconfig options enabled would
     * have no working ::tzset()/::tzname at all, and should implement a
     * UTC-only fallback here instead, along these lines:
     *
     *   const char* tzName(int index) override
     *   {
     *       static const char* const utc[2] = { "UTC", "UTC" };
     *       return utc[index];
     *   }
     *
     * (no DST ever, nothing to (re)compute -- see
     * docs/porting/RTOS_PORTING_GUIDE.md for the full reasoning).
     *
     * NOTE: named tzName(), NOT tzname() -- a libc <time.h> that
     * `#define`s `tzname` to `_tzname` (newlib does; NuttX's own libc
     * does not, but don't assume that of a future port) would otherwise
     * silently rename this override inconsistently with the base
     * class's declaration depending on <time.h> include order. */
    const char* tzName(int index) override
    {
        ::tzset();
        return ::tzname[index];
    }
};

static NuttXPlatform g_platform;

Escargot::PlatformRef* escargot_platform()
{
    return &g_platform;
}
