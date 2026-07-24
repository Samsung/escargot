/* Linker script for Corstone-300 (AN552) NS world
 *
 * Memory map (NS regions only):
 *   ITCM : 0x00000000   512KB  vector table + tiny boot stub
 *   SRAM : 0x21000000     2MB  data / bss / stack / FreeRTOS heap
 *   DDR  : 0x60000000    64MB  code + GC heap
 *
 * Text (1+ MB) does not fit in the 512KB ITCM, so the bulk of code
 * lives in DDR.  The vector table and Reset_Handler are kept in ITCM
 * so that execution begins there on reset before jumping into DDR.
 */

MEMORY {
    ITCM (rx)  : ORIGIN = 0x00000000, LENGTH = 512K
    SRAM (rwx) : ORIGIN = 0x21000000, LENGTH = 2M
    DDR  (rwx) : ORIGIN = 0x60000000, LENGTH = 64M
}

ENTRY(Reset_Handler)

SECTIONS {
    /* ── ITCM: vectors + startup ─────────────────────────────────── */
    .vectors : { KEEP(*(.vectors)) } > ITCM

    /* ── DDR: all code + rodata ──────────────────────────────────── */
    .text : {
        *(.text*)
        *(.rodata*)
        *(.init)
        *(.fini)
    } > DDR

    /* C++ constructors / destructors */
    .init_array : {
        PROVIDE_HIDDEN (__init_array_start = .);
        KEEP(*(.init_array SORT_NONE(.init_array.*)))
        PROVIDE_HIDDEN (__init_array_end = .);
    } > DDR
    .fini_array : {
        PROVIDE_HIDDEN (__fini_array_start = .);
        KEEP(*(.fini_array SORT_NONE(.fini_array.*)))
        PROVIDE_HIDDEN (__fini_array_end = .);
    } > DDR

    /* ARM unwind tables — must be in same load region as .text */
    .ARM.exidx : {
        __exidx_start = .;
        KEEP(*(.ARM.exidx*))
        __exidx_end = .;
    } > DDR
    .ARM.extab : {
        KEEP(*(.ARM.extab*))
        . = ALIGN(4);
    } > DDR
    /* C++ exception handler tables (LSDA) — must not be GC'd */
    .gcc_except_table : {
        KEEP(*(.gcc_except_table))
        KEEP(*(.gcc_except_table.*))
        . = ALIGN(4);
        __etext = .;        /* LMA used as source for .data copy-down */
    } > DDR

    /* ── SRAM: data / bss / heap / stack ────────────────────────── */
    .data : AT(__etext) {
        __data_start__ = .;
        *(.data*)
        . = ALIGN(4);
        __data_end__ = .;
    } > SRAM

    .bss (NOLOAD) : {
        __bss_start__ = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end__ = .;
        end = .;            /* newlib / BDWGC: start of unused SRAM */
    } > SRAM

    .stack (NOLOAD) : {
        . = ALIGN(8);
        . += 64K;
        __stack_top__ = .;
    } > SRAM

    /* ── GC heap: rest of DDR after code ────────────────────────── */
    __gc_heap_start__ = ALIGN(__etext + SIZEOF(.data), 65536);  /* after .data LMA */
    __gc_heap_size__  = ORIGIN(DDR) + LENGTH(DDR) - __gc_heap_start__;

    /* BDWGC NOSYS ARM32 expects these symbols */
    __data_start  = __data_start__;
    __stack_base__ = __stack_top__;
}
