/* Bare-metal / FreeRTOS platform glue for Escargot */

#include "FreeRTOS.h"
#include "task.h"
#include "EscargotPublic.h"
#include <time.h>

/* ── Escargot Platform implementation ─────────────────────────────── */

static void semi_puts(const char* s)
{
    register const char* r1 __asm("r1") = s;
    register int         r0 __asm("r0") = 4; /* SYS_WRITE0 */
    __asm volatile("bkpt #0xAB" : "+r"(r0) : "r"(r1) : "memory");
}

class FreeRTOSPlatform : public Escargot::PlatformRef {
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

    /* stackBase(): LOWEST valid address for the current task's stack.
     * Used by ThreadLocal::initialize() to set g_stackLimit. js_stack[]
     * (see src/main.cpp) is a static array, so its own start address is
     * the lowest valid address. */
    void* stackBase() override
    {
        extern void* g_js_stack_base;
        return g_js_stack_base;
    }

    /* stackTop(): the "cold end" (HIGH address) of the same js_stack[]
     * buffer -- fed to BDWGC via Heap::initialize()/GC_set_stackbottom()
     * before GC_init() runs (see src/heap/Heap.cpp and
     * third_party/GCutil/include/private/gcconfig_escargot_rtos.h).
     * NOTE: this used to come from a `__stack_base__` linker symbol
     * pointing at an unrelated, separate 64 KB boot/interrupt stack
     * region (the `__stack_top__` label in startup/an552_ns.ld) rather than
     * js_stack[] itself; it happened to still be safe only because that
     * region's address is always higher than js_stack[]'s own end, so
     * BDWGC's scan range [current SP, that symbol] was a strict (if
     * imprecise) superset of the real js_stack[] contents. Returning
     * js_stack[]'s own real high end here instead is the same idea
     * NuttX's port already uses (see escargot_get_stack_top() in
     * apps/interpreters/escargot/escargot_platform.cxx there) -- more
     * precise, and keeps both ports on the same mechanism. */
    void* stackTop() override
    {
        extern void* g_js_stack_top;
        return g_js_stack_top;
    }

    /* tickMs(): millisecond tick for Date.now() fallback. */
    uint32_t tickMs() override
    {
        return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
    }

    /* tzName(): this port links a full newlib libc, which already has a
     * real, working ::tzset()/::tzname -- so just forward to it. No
     * UTC-only fallback needed here (see
     * docs/porting/RTOS_PORTING_GUIDE.md for what a port without a real
     * libc tzset would implement instead).
     *
     * NOTE: this method is deliberately named tzName(), NOT tzname() --
     * newlib's <time.h> #define's `tzname` to `_tzname` for MT-safety,
     * which would otherwise silently rename this override (and, worse,
     * inconsistently with the base class's own declaration depending on
     * <time.h> include order -- verified this exact collision breaks the
     * build with "marked 'override', but does not override"). */
    const char* tzName(int index) override
    {
        ::tzset();
        return ::tzname[index];
    }
};

static FreeRTOSPlatform g_platform;

Escargot::PlatformRef* escargot_platform()
{
    return &g_platform;
}
