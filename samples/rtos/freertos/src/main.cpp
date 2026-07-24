#include "FreeRTOS.h"
#include "task.h"
#include "EscargotPublic.h"
#include <gc.h>
#include <string.h>
#include <stdint.h>
#include <exception>

uint32_t SystemCoreClock = 25000000U;

static inline void _semi(const char* s) {
    register const char* r1 __asm("r1") = s;
    register int r0 __asm("r0") = 4;
    __asm volatile("bkpt #0xAB" : "+r"(r0) : "r"(r1) : "memory");
}

/* Intercept abort/kill so we get a diagnostic instead of silent hang */
extern "C" {
    void abort(void) {
        _semi("[ABORT] called\n"); __asm volatile("bkpt #0"); while (1);
    }
    int _kill(int pid, int sig) {
        (void)pid; (void)sig; _semi("[KILL] called\n"); return -1;
    }
    int _getpid(void) { return 1; }
}

static void my_terminate() noexcept {
    _semi("[TERMINATE] called\n"); __asm volatile("bkpt #0"); while (1);
}

extern Escargot::PlatformRef* escargot_platform();

/* ── semihosting primitives ─────────────────────────────────────────────── */
static void semi_puts(const char* s)
{
    register const char* r1 __asm("r1") = s;
    register int         r0 __asm("r0") = 4;
    __asm volatile("bkpt #0xAB" : "+r"(r0) : "r"(r1) : "memory");
}

/* SYS_OPEN=1  mode 0=r, returns handle or -1 */
static int semi_open(const char* path, int mode)
{
    struct { const char* name; int mode; uint32_t len; } args = { path, mode, (uint32_t)strlen(path) };
    register void* r1 __asm("r1") = &args;
    register int   r0 __asm("r0") = 1;
    __asm volatile("bkpt #0xAB" : "+r"(r0) : "r"(r1) : "memory");
    return r0;
}

/* SYS_CLOSE=2 */
static void semi_close(int fh)
{
    register int* r1 __asm("r1") = &fh;
    register int  r0 __asm("r0") = 2;
    __asm volatile("bkpt #0xAB" : "+r"(r0) : "r"(r1) : "memory");
}

/* SYS_FLEN=0x0C  returns file size or -1 */
static int semi_flen(int fh)
{
    register int* r1 __asm("r1") = &fh;
    register int  r0 __asm("r0") = 0x0C;
    __asm volatile("bkpt #0xAB" : "+r"(r0) : "r"(r1) : "memory");
    return r0;
}

/* SYS_READ=6  reads up to len bytes; returns bytes NOT transferred (0=all ok) */
static int semi_read(int fh, char* buf, int len)
{
    struct { int fh; char* buf; int len; } args = { fh, buf, len };
    register void* r1 __asm("r1") = &args;
    register int   r0 __asm("r0") = 6;
    __asm volatile("bkpt #0xAB" : "+r"(r0) : "r"(r1) : "memory");
    return r0; /* bytes NOT transferred; 0 = success */
}

/* Load a host-side file into a GC-allocated buffer. Returns pointer+size, or null. */
static char* load_file(const char* path, int* out_size)
{
    int fh = semi_open(path, 0);
    if (fh == -1) return nullptr;
    int sz = semi_flen(fh);
    if (sz <= 0) { semi_close(fh); return nullptr; }
    char* buf = (char*)GC_malloc_atomic(sz + 1);
    if (!buf)    { semi_close(fh); return nullptr; }
    int remaining = semi_read(fh, buf, sz);
    semi_close(fh);
    int got = sz - remaining;
    buf[got] = '\0';
    *out_size = got;
    return buf;
}

/* ── number/string helpers ──────────────────────────────────────────────── */
static char* put_u32(char* p, unsigned v)
{
    if (v == 0) { *p++ = '0'; return p; }
    char tmp[12]; int n = 0;
    while (v) { tmp[n++] = '0' + v % 10; v /= 10; }
    while (n > 0) *p++ = tmp[--n];
    return p;
}

static void print_gc_stats(const char* label)
{
    char buf[48]; char* p = buf;
    const char pfx[] = "[gc] ";
    for (int i = 0; pfx[i]; i++) *p++ = pfx[i];
    for (const char* s = label; *s; s++) *p++ = *s;
    *p++ = ' '; *p++ = 'h'; *p++ = '=';
    p = put_u32(p, (unsigned)(GC_get_heap_size() / 1024)); *p++ = 'K';
    *p++ = ' '; *p++ = 's'; *p++ = '=';
    p = put_u32(p, (unsigned)(GC_get_bytes_since_gc() / 1024)); *p++ = 'K';
    *p++ = '\n'; *p = '\0';
    semi_puts(buf);
}

/* ── task stack ─────────────────────────────────────────────────────────── */
#define JS_STACK_WORDS 131072
static StackType_t  js_stack[JS_STACK_WORDS];
static StaticTask_t js_tcb;

static StaticTask_t idle_tcb;
static StackType_t  idle_stack[configMINIMAL_STACK_SIZE];
static StaticTask_t timer_tcb;
static StackType_t  timer_stack[configTIMER_TASK_STACK_DEPTH];

void* g_js_stack_base = js_stack;
/* HIGH end of the same buffer -- see PlatformRef::stackTop() in
 * src/escargot_platform.cpp for how this is used (fed to BDWGC via
 * Heap::initialize()/GC_set_stackbottom(), *before* GC_init() runs, so
 * the collector scans js_stack itself rather than the small separate
 * boot/interrupt stack the linker script's __stack_top__ symbol used to
 * point BDWGC at). */
void* g_js_stack_top = js_stack + JS_STACK_WORDS;

/* print up to maxLen chars of a StringRef via semihosting */
static void semi_print_strref(Escargot::StringRef* s, int maxLen = 80)
{
    if (!s) return;
    auto d = s->stringBufferAccessData();
    char buf[84];
    int n = (int)d.length < maxLen ? (int)d.length : maxLen;
    for (int i = 0; i < n; i++)
        buf[i] = (char)(d.charAt((size_t)i) & 0x7f);
    buf[n] = '\n'; buf[n + 1] = '\0';
    semi_puts(buf);
}

/* ── run one JS source string, print result or error ────────────────────── */
static bool run_js(Escargot::ContextRef* ctx,
                   const char* src, int srcLen,
                   const char* name)
{
    using namespace Escargot;

    auto result = ctx->scriptParser()->initializeScript(
        StringRef::createFromASCII(src, (size_t)srcLen),
        StringRef::createFromASCII(name, strlen(name)));

    if (!result.isSuccessful()) {
        semi_puts("[PARSE_ERR] "); semi_puts(name); semi_puts("\n");
        return false;
    }

    auto er = Evaluator::execute(ctx,
        [](ExecutionStateRef* state, ScriptRef* script) -> ValueRef* {
            return script->execute(state);
        }, result.script.get());

    if (!er.isSuccessful()) {
        semi_puts("[THROW] "); semi_puts(name); semi_puts("\n");
        return false;
    }

    /* print result value */
    char buf[64]; char* p = buf;
    const char pfx[] = "[ok]  ";
    for (int i = 0; pfx[i]; i++) *p++ = pfx[i];
    for (const char* s = name; *s; s++) *p++ = *s;
    *p++ = ' '; *p++ = '='; *p++ = '>';
    *p++ = ' ';

    ValueRef* v = er.result;
    if (v->isInt32()) {
        int iv = v->asInt32();
        if (iv < 0) { *p++ = '-'; iv = -iv; }
        p = put_u32(p, (unsigned)iv);
    } else if (v->isNumber()) {
        const char fs[] = "<float>";
        for (int i = 0; fs[i]; i++) *p++ = fs[i];
    } else if (v->isUndefined()) {
        const char us[] = "undefined";
        for (int i = 0; us[i]; i++) *p++ = us[i];
    } else if (v->isBoolean()) {
        const char* bs = v->asBoolean() ? "true" : "false";
        while (*bs) *p++ = *bs++;
    } else if (v->isString()) {
        *p++ = '"';
        *p++ = '\0';
        semi_puts(buf);
        semi_print_strref(v->asString(), 60);
        return true;
    } else {
        const char os[] = "<object/..>";
        for (int i = 0; os[i]; i++) *p++ = os[i];
    }
    *p++ = '\n'; *p = '\0';
    semi_puts(buf);
    return true;
}

/* ── JS task ─────────────────────────────────────────────────────────────── */

/* SunSpider 1.0 test list */
static const char* const k_sunspider[] = {
    "3d-cube",
    "3d-morph",
    "3d-raytrace",
    "access-binary-trees",
    "access-fannkuch",
    "access-nbody",
    "access-nsieve",
    "bitops-3bit-bits-in-byte",
    "bitops-bits-in-byte",
    "bitops-bitwise-and",
    "bitops-nsieve-bits",
    "controlflow-recursive",
    "crypto-aes",
    "crypto-md5",
    "crypto-sha1",
    "date-format-tofte",
    "date-format-xparb",
    "math-cordic",
    "math-partial-sums",
    "math-spectral-norm",
    "regexp-dna",
    "string-base64",
    "string-fasta",
    "string-tagcloud",
    "string-unpack-code",
    "string-validate-input",
};
#define SUNSPIDER_COUNT ((int)(sizeof(k_sunspider)/sizeof(k_sunspider[0])))

/* Base path to SunSpider tests on the host (semihosting uses host CWD) */
#define SUNSPIDER_BASE "test/vendortest/SunSpider/tests/sunspider-1.0.2/"

__attribute__((noinline))
static void cpp_exc_test(void)
{
    bool ok = false;
    try { throw 42; } catch (int v) { ok = (v == 42); }
    semi_puts(ok ? "[cpp-exc] OK\n" : "[cpp-exc] BROKEN\n");
}

static void js_task(void* arg)
{
    (void)arg;
    std::set_terminate(my_terminate);
    semi_puts("[js] task started\n");

    Escargot::Globals::initialize(escargot_platform());
    auto vm  = Escargot::VMInstanceRef::create();
    auto ctx = Escargot::ContextRef::create(vm.get());
    semi_puts("[js] context ready\n");
    print_gc_stats("init      ");

    /* ── basic sanity ── */
    run_js(ctx.get(), "1+2", 3, "1+2");

    /* ── C++ exception self-test ── */
    cpp_exc_test();

    /* ── exception tests ── */
    semi_puts("\n--- exception tests ---\n");

#define RJS(name, src) run_js(ctx.get(), src, sizeof(src)-1, name)

    /* 1. throw primitive — uncaught */
    RJS("uncaught-throw-42",
        "throw 42");

    /* 2. catch thrown integer */
    RJS("catch-throw-int",
        "try { throw 42 } catch(e) { e }");

    /* 3. throw and catch Error object, read message */
    RJS("catch-Error-message",
        "try { throw new Error('hello') } catch(e) { e.message }");

    /* 4. TypeError from null deref — uncaught */
    RJS("uncaught-null-deref",
        "null.x");

    /* 5. catch TypeError, check instanceof */
    RJS("catch-TypeError-instanceof",
        "try { null.x } catch(e) { e instanceof TypeError }");

    /* 6. catch TypeError, read .name */
    RJS("catch-TypeError-name",
        "try { undefined() } catch(e) { e.name }");

    /* 7. RangeError from bad array length */
    RJS("catch-RangeError",
        "try { new Array(-1) } catch(e) { e.name }");

    /* 8. finally always runs */
    RJS("finally-runs",
        "(function(){ var x=0; try { throw 1 } finally { x=99 } }())");

    /* 9. finally runs, catch value survives */
    RJS("finally-catch-value",
        "var r=0; try { throw 7 } catch(e) { r=e } finally { r+=1 } r");

    /* 10. re-throw */
    RJS("rethrow",
        "try { try { throw new TypeError('orig') } catch(e) { throw e } } catch(e) { e.message }");

    /* 11. nested try/catch */
    RJS("nested-try",
        "var r=''; try { try { throw 'inner' } catch(e) { r+=e; throw 'outer' } } catch(e) { r+=e } r");

    /* 12. Error stack trace exists */
    RJS("error-has-stack",
        "try { throw new Error('x') } catch(e) { typeof e.stack }");

    /* 13. SyntaxError from eval-like dynamic parse (Function constructor) */
    RJS("syntax-error-function-ctor",
        "try { new Function('}{') } catch(e) { e instanceof SyntaxError }");

    /* 14. throw in forEach callback */
    RJS("throw-in-callback",
        "var r=0; try { [1,2,3].forEach(function(x){ if(x===2) throw x; r+=x }) } catch(e) { r+e }");

    /* 15. Promise rejection is value (not exception at top level) */
    RJS("promise-reject-value",
        "var p = Promise.reject(55); typeof p");

#undef RJS

    semi_puts("--- exception tests done ---\n\n");

    /* ── SunSpider one by one ── */
    for (int i = 0; i < SUNSPIDER_COUNT; i++) {
        char path[128];
        char* p = path;
        const char* base = SUNSPIDER_BASE;
        while (*base) *p++ = *base++;
        const char* nm = k_sunspider[i];
        while (*nm)   *p++ = *nm++;
        const char* ext = ".js";
        while (*ext)  *p++ = *ext++;
        *p = '\0';

        int sz = 0;
        char* src = load_file(path, &sz);
        if (!src) {
            semi_puts("[err] cannot open: "); semi_puts(k_sunspider[i]); semi_puts("\n");
            continue;
        }

        /* label: just the test name */
        run_js(ctx.get(), src, sz, k_sunspider[i]);

        print_gc_stats(k_sunspider[i]);
        GC_gcollect();
    }

    print_gc_stats("final     ");
    semi_puts("[js] done\n");

    for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
}

/* ── FreeRTOS hooks ─────────────────────────────────────────────────────── */
extern "C" {
void vApplicationGetIdleTaskMemory(StaticTask_t** ppxIdleTaskTCBBuffer,
                                   StackType_t**  ppxIdleTaskStackBuffer,
                                   uint32_t*      pulIdleTaskStackSize)
{
    *ppxIdleTaskTCBBuffer   = &idle_tcb;
    *ppxIdleTaskStackBuffer = idle_stack;
    *pulIdleTaskStackSize   = configMINIMAL_STACK_SIZE;
}
void vApplicationGetTimerTaskMemory(StaticTask_t** ppxTimerTaskTCBBuffer,
                                    StackType_t**  ppxTimerTaskStackBuffer,
                                    uint32_t*      pulTimerTaskStackSize)
{
    *ppxTimerTaskTCBBuffer   = &timer_tcb;
    *ppxTimerTaskStackBuffer = timer_stack;
    *pulTimerTaskStackSize   = configTIMER_TASK_STACK_DEPTH;
}
} /* extern "C" */

int main(void)
{
    semi_puts("[main] entered\n");
    xTaskCreateStatic(js_task, "js", JS_STACK_WORDS, NULL,
                      tskIDLE_PRIORITY + 1, js_stack, &js_tcb);
    vTaskStartScheduler();
    for (;;);
}
