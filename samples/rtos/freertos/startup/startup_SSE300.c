/* Minimal C startup for Corstone-300 (AN552) NS world, GCC */
#include <stdint.h>

extern uint32_t __etext;
extern uint32_t __data_start__;
extern uint32_t __data_end__;
extern uint32_t __bss_start__;
extern uint32_t __bss_end__;
extern uint32_t __stack_top__;

extern int main(void);

/* C++ global constructor / destructor table */
extern void (*__init_array_start[])(void);
extern void (*__init_array_end[])(void);

void Reset_Handler(void);
void Default_Handler(void);

#define WEAK_ALIAS(x) __attribute__((weak, alias(#x)))

void NMI_Handler(void)           WEAK_ALIAS(Default_Handler);
void HardFault_Handler(void); /* defined below */
void MemManage_Handler(void)     WEAK_ALIAS(Default_Handler);
void BusFault_Handler(void)      WEAK_ALIAS(Default_Handler);
void UsageFault_Handler(void)    WEAK_ALIAS(Default_Handler);
void SecureFault_Handler(void)   WEAK_ALIAS(Default_Handler);
void SVC_Handler(void)           WEAK_ALIAS(Default_Handler);
void DebugMon_Handler(void)      WEAK_ALIAS(Default_Handler);
void PendSV_Handler(void)        WEAK_ALIAS(Default_Handler);
void SysTick_Handler(void)       WEAK_ALIAS(Default_Handler);

__attribute__((section(".vectors"), used))
const void *g_vectors[] = {
    (void *)&__stack_top__,
    Reset_Handler,
    NMI_Handler,
    HardFault_Handler,
    MemManage_Handler,
    BusFault_Handler,
    UsageFault_Handler,
    SecureFault_Handler,
    0, 0, 0,
    SVC_Handler,
    DebugMon_Handler,
    0,
    PendSV_Handler,
    SysTick_Handler,
};

/* Raw Cortex-M semihosting: SYS_WRITE0 (print null-terminated string) */
static void semihost_puts(const char *s) {
    register const char *r1 __asm("r1") = s;
    register int         r0 __asm("r0") = 4; /* SYS_WRITE0 */
    __asm volatile ("bkpt #0xAB" : "+r"(r0) : "r"(r1) : "memory");
}

void Reset_Handler(void) {
    /* Enable FPU and MVE: set CPACR CP10/CP11 to full access */
    *((volatile uint32_t *)0xE000ED88U) |= (3UL << 20) | (3UL << 22);
    __asm volatile("dsb\n\t isb");

    semihost_puts("[boot] Reset_Handler entered\n");

    /* Copy .data from flash to SRAM */
    uint32_t *src = &__etext;
    uint32_t *dst = &__data_start__;
    while (dst < &__data_end__) {
        *dst++ = *src++;
    }
    /* Zero .bss */
    dst = &__bss_start__;
    while (dst < &__bss_end__) {
        *dst++ = 0;
    }

    /* Call C++ global constructors */
    for (void (**fn)(void) = __init_array_start; fn < __init_array_end; fn++) {
        (*fn)();
    }

    semihost_puts("[boot] calling main\n");
    main();
    while (1);
}

/* newlib _write: redirect stdout/stderr to semihosting SYS_WRITE0 */
int _write(int fd, char *buf, int len) {
    (void)fd;
    /* SYS_WRITE (0x05): {fd, buf, len} */
    uint32_t args[3] = {1, (uint32_t)buf, (uint32_t)len};
    register uint32_t *r1 __asm("r1") = args;
    register int       r0 __asm("r0") = 5; /* SYS_WRITE */
    __asm volatile ("bkpt #0xAB" : "+r"(r0) : "r"(r1) : "memory");
    return len;
}

/* sbrk(): backed by the GC heap region (DDR, after code -- see
 * `__gc_heap_start__`/`__gc_heap_size__` in startup/an552_ns.ld). The
 * actual bump-pointer logic is shared with every other RTOS port in
 * src/heap/SbrkBaremetal.cpp's escargotBaremetalSbrk(); this tiny
 * forwarder just needs to live in a directly-linked object (like this
 * one), NOT inside the escargot-lib static archive -- newlib's own
 * libc.a already provides a `sbrk()` that calls `_sbrk()`, and archive
 * members are only pulled in to satisfy an already-outstanding undefined
 * reference, so a same-named `_sbrk` buried in the archive would
 * silently lose to newlib's non-functional stub (see
 * SbrkBaremetal.cpp's own comment for the full explanation -- this is
 * exactly the bug hit and fixed while consolidating this logic). */
extern void *escargotBaremetalSbrk(intptr_t increment);
void *_sbrk(int incr) { return escargotBaremetalSbrk((intptr_t)incr); }

int _close(int fd) { (void)fd; return -1; }
int _fstat(int fd, void *st) { (void)fd; (void)st; return 0; }
int _isatty(int fd) { (void)fd; return 1; }
int _lseek(int fd, int off, int w) { (void)fd; (void)off; (void)w; return -1; }
int _read(int fd, char *buf, int len) { (void)fd; (void)buf; (void)len; return -1; }

/* BDWGC needs getpagesize */
int getpagesize(void) { return 4096; }

/* ARM newlib may not export _setjmp as a standalone symbol */
#include <setjmp.h>
int _setjmp(jmp_buf env) { return setjmp(env); }

void HardFault_Handler(void) {
    static const char msg[] = "[HARDFAULT] HardFault_Handler\n";
    uint32_t args[3] = {1, (uint32_t)msg, sizeof(msg) - 1};
    register uint32_t *r1 __asm("r1") = args;
    register int       r0 __asm("r0") = 5; /* SYS_WRITE */
    __asm volatile ("bkpt #0xAB" : "+r"(r0) : "r"(r1) : "memory");
    while (1);
}

void Default_Handler(void) {
    while (1);
}
