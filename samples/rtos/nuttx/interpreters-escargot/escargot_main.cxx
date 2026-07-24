/****************************************************************************
 * apps/interpreters/escargot/escargot_main.cxx
 *
 * NSH command that boots the Escargot JS engine and runs a small workload:
 * a sanity check, the exception-handling regression set carried over from
 * the FreeRTOS/QEMU port, and the full embedded SunSpider 1.0.2 suite (26 tests).
 * Prints GC heap stats and NuttX mallinfo() before/after so memory usage
 * can be compared against the FreeRTOS port's porting-report.md numbers.
 ****************************************************************************/

#include <nuttx/config.h>
#include <cstdio>
#include <cstring>
#include <malloc.h>
#include <time.h>
#include <nuttx/sched.h>
#include <nuttx/arch.h>

#include <gc.h>
#include "EscargotPublic.h"
#include "embedded_tests.h"

extern Escargot::PlatformRef* escargot_platform();

static void print_mallinfo(const char* label)
{
    struct mallinfo mi = mallinfo();
    printf("[mm] %-12s arena=%7d KB used=%7d KB free=%7d KB\n",
           label, mi.arena / 1024, mi.uordblks / 1024, mi.fordblks / 1024);
}

static void print_gc_stats(const char* label)
{
    printf("[gc] %-12s heap=%7lu KB since_gc=%7lu KB free=%7lu KB\n",
           label,
           (unsigned long)(GC_get_heap_size() / 1024),
           (unsigned long)(GC_get_bytes_since_gc() / 1024),
           (unsigned long)(GC_get_free_bytes() / 1024));
}

static void print_value(Escargot::ValueRef* v)
{
    using namespace Escargot;
    if (v->isInt32()) {
        printf("%d", v->asInt32());
    } else if (v->isNumber()) {
        printf("%f", v->asNumber());
    } else if (v->isUndefined()) {
        printf("undefined");
    } else if (v->isBoolean()) {
        printf("%s", v->asBoolean() ? "true" : "false");
    } else if (v->isString()) {
        auto d = v->asString()->stringBufferAccessData();
        char buf[128];
        size_t n = d.length < sizeof(buf) - 1 ? d.length : sizeof(buf) - 1;
        for (size_t i = 0; i < n; i++) {
            buf[i] = (char)(d.charAt(i) & 0x7f);
        }
        buf[n] = '\0';
        printf("\"%s\"", buf);
    } else {
        printf("<object/...>");
    }
}

static bool run_js(Escargot::ContextRef* ctx, const char* src, size_t srcLen, const char* name)
{
    using namespace Escargot;

    auto result = ctx->scriptParser()->initializeScript(
        StringRef::createFromASCII(src, srcLen),
        StringRef::createFromASCII(name, strlen(name)));

    if (!result.isSuccessful()) {
        auto emsg = result.parseErrorMessage;
        char buf[256];
        size_t n = 0;
        if (emsg) {
            auto d = emsg->stringBufferAccessData();
            n = d.length < sizeof(buf) - 1 ? d.length : sizeof(buf) - 1;
            for (size_t i = 0; i < n; i++) {
                buf[i] = (char)(d.charAt(i) & 0x7f);
            }
        }
        buf[n] = '\0';
        printf("[PARSE_ERR] %s code=%d msg=\"%s\"\n", name, (int)result.parseErrorCode, buf);
        return false;
    }

    auto er = Evaluator::execute(
        ctx,
        [](ExecutionStateRef* state, ScriptRef* script) -> ValueRef* {
            return script->execute(state);
        },
        result.script.get());

    if (!er.isSuccessful()) {
        printf("[THROW] %s\n", name);
        return false;
    }

    printf("[ok]  %-28s => ", name);
    print_value(er.result);
    printf("\n");
    return true;
}

__attribute__((noinline)) static void cpp_exc_test(void)
{
    bool ok = false;
    try {
        throw 42;
    } catch (int v) {
        ok = (v == 42);
    }
    printf("[cpp-exc] %s\n", ok ? "OK" : "BROKEN");
}

extern "C" int main(int argc, FAR char* argv[])
{
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    printf("[escargot] starting on NuttX / QEMU mps3-an547 (Cortex-M55)\n");

    print_mallinfo("before-init");

    Escargot::Globals::initialize(escargot_platform());
    auto vm = Escargot::VMInstanceRef::create();
    auto ctx = Escargot::ContextRef::create(vm.get());

    printf("[escargot] context ready\n");
    print_gc_stats("init");
    print_mallinfo("after-init");

    run_js(ctx.get(), "1+2", 3, "1+2");
    cpp_exc_test();

    printf("\n--- exception tests ---\n");
#define RJS(name, src) run_js(ctx.get(), src, sizeof(src) - 1, name)
    RJS("uncaught-throw-42", "throw 42");
    RJS("catch-throw-int", "try { throw 42 } catch(e) { e }");
    RJS("catch-Error-message", "try { throw new Error('hello') } catch(e) { e.message }");
    RJS("uncaught-null-deref", "null.x");
    RJS("catch-TypeError-instanceof", "try { null.x } catch(e) { e instanceof TypeError }");
    RJS("catch-TypeError-name", "try { undefined() } catch(e) { e.name }");
    RJS("catch-RangeError", "try { new Array(-1) } catch(e) { e.name }");
    RJS("finally-runs", "(function(){ var x=0; try { throw 1 } finally { x=99 } }())");
    RJS("finally-catch-value", "var r=0; try { throw 7 } catch(e) { r=e } finally { r+=1 } r");
    RJS("rethrow", "try { try { throw new TypeError('orig') } catch(e) { throw e } } catch(e) { e.message }");
    RJS("nested-try", "var r=''; try { try { throw 'inner' } catch(e) { r+=e; throw 'outer' } } catch(e) { r+=e } r");
    RJS("error-has-stack", "try { throw new Error('x') } catch(e) { typeof e.stack }");
    RJS("syntax-error-function-ctor", "try { new Function('}{') } catch(e) { e instanceof SyntaxError }");
    RJS("throw-in-callback", "var r=0; try { [1,2,3].forEach(function(x){ if(x===2) throw x; r+=x }) } catch(e) { r+e }");
    RJS("promise-reject-value", "var p = Promise.reject(55); typeof p");
#undef RJS
    printf("--- exception tests done ---\n\n");

    printf("--- SunSpider 1.0.2 (full suite, %d tests) ---\n", EMBEDDED_TEST_COUNT);
    int sunspider_pass = 0;
    for (int i = 0; i < EMBEDDED_TEST_COUNT; i++) {
        const EmbeddedTest& t = k_embedded_tests[i];
        /* Each SunSpider 1.0.2 test self-validates its own result and
         * `throw`s a descriptive Error if wrong (that's the upstream test
         * format, not something added here) -- so a clean [ok] here means
         * the test's internal correctness check passed, not merely "did
         * not crash". */
        bool ok = run_js(ctx.get(), t.src, strlen(t.src), t.name);
        if (ok) {
            sunspider_pass++;
        }
        print_gc_stats(t.name);
        GC_gcollect();
    }
    printf("--- SunSpider 1.0.2 done: %d/%d passed ---\n\n", sunspider_pass, EMBEDDED_TEST_COUNT);

    print_gc_stats("final");
    print_mallinfo("final");

    clock_gettime(CLOCK_MONOTONIC, &t1);
    double elapsed = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9;
    printf("[escargot] done in %.3f s\n", elapsed);

    /* DIAGNOSTIC (not a permanent test-harness feature): real stack
     * high-water mark for this task, via CONFIG_STACK_COLORATION -- same
     * mechanism NSH's own `ps`/procfs "StackUsed:" field uses
     * (fs/procfs/fs_procfsproc.c: up_check_tcbstack(tcb,
     * tcb->adj_stack_size)), read directly here because the task exits
     * (and its TCB goes away) right after this function returns, so
     * there is no window to `ps` it externally afterward. */
    {
        FAR struct tcb_s* tcb = nxsched_self();
        size_t stackUsed = up_check_tcbstack(tcb, tcb->adj_stack_size);
        printf("[stack] high-water: %zu / %lu bytes (%.1f KB / %.1f KB)\n",
               stackUsed, (unsigned long)tcb->adj_stack_size,
               stackUsed / 1024.0, tcb->adj_stack_size / 1024.0);
    }

    return 0;
}
