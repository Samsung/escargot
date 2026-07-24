# Escargot JS Engine — FreeRTOS / Cortex-M55 Porting Guide

> **Date:** 2026-07-03
> **Commits:** `9adc4cea0` (main repo), `c44beaf` (escargot-rtos repo)
> **Branches:** `qemu-mps3-an547` (escargot-rtos), `work74` / `origin/master` (escargot)
> **Simulator:** QEMU `mps3-an547` (Cortex-M55 emulation)
> **Target hardware:** BES2800BP (Cortex-M55 @ 400 MHz, 8.3 MB SRAM, 64 MB PSRAM, 20 MB Flash)

> **Update:** the port described below now also lives in-tree as a runnable,
> CI-verified sample at [`samples/rtos/freertos/`](../samples/rtos/freertos)
> (built via `.github/workflows/rtos-freertos.yml`), instead of only in the
> separate `escargot-rtos` repo this report was originally written against.
> This document is left as-is as the detailed retrospective/porting report;
> see [`docs/porting/RTOS_PORTING_GUIDE.md`](porting/RTOS_PORTING_GUIDE.md)
> for the distilled, forward-looking checklist.

---

## Table of Contents

1. [Overview](#1-overview)
2. [Architecture](#2-architecture)
3. [Repository Layout](#3-repository-layout)
4. [Target Specifications](#4-target-specifications)
5. [Build Environment](#5-build-environment)
6. [QEMU Emulator Setup](#6-qemu-emulator-setup)
7. [Memory Map](#7-memory-map)
8. [Binary Size](#8-binary-size)
9. [Runtime Memory Usage Analysis (QEMU Measured)](#85-runtime-memory-usage-analysis-qemu-measured)
10. [Main Repo Changes (OS\_BAREMETAL Support)](#9-main-repo-changes-os_baremetal-support)
11. [escargot-rtos Porting Layer](#10-escargot-rtos-porting-layer)
12. [Bugs Found and Fixed](#11-bugs-found-and-fixed)
13. [C++ Exception / JS Exception Verification](#12-c-exception--js-exception-verification)
14. [GC Behavior Analysis](#13-gc-behavior-analysis)
15. [SunSpider 1.0 Results](#14-sunspider-10-results)
16. [Real Hardware Porting Guide](#15-real-hardware-porting-guide)
17. [Porting to Other RTOSes](#16-porting-to-other-rtoses)
18. [Future Work](#17-future-work)

---

## 1. Overview

This document describes the porting of the Escargot JavaScript engine to run on top of **FreeRTOS**.
The port was validated on **QEMU `mps3-an547`** (Cortex-M55 emulation), with the ultimate goal of
running on the **BES2800BP** (Cortex-M55 @ 400 MHz) real hardware.

### Goals and Results

| Goal | Result |
|---|---|
| Boot Escargot on FreeRTOS | ✅ |
| `1 + 2` → `3` | ✅ |
| C++ throw/catch | ✅ |
| JS try/catch/finally/Error (15 cases) | ✅ 15/15 |
| SunSpider 1.0 full pass | ✅ 26/26 |
| GC automatic collection | ✅ (5 auto collections) |

---

## 2. Architecture

```
┌─────────────────────────────────────────────────┐
│              QEMU mps3-an547                     │
│           (Cortex-M55 emulation)                 │
├─────────────────────────────────────────────────┤
│  FreeRTOS (ARM_CM4F port)                        │
│  ┌───────────────────────────────────────────┐  │
│  │  JS Task (512 KB stack, statically alloc) │  │
│  │  ┌─────────────────────────────────────┐  │  │
│  │  │  Escargot JS Engine                 │  │  │
│  │  │  (OS_BAREMETAL mode)                │  │  │
│  │  │  - No pthread                       │  │  │
│  │  │  - escargot_get_stack_base()        │  │  │
│  │  │  - escargot_get_tick_ms()           │  │  │
│  │  │  - thread_local → static            │  │  │
│  │  └─────────────────────────────────────┘  │  │
│  │  BDWGC (NOSYS, 4 MB initial heap)        │  │
│  └───────────────────────────────────────────┘  │
│  Semihosting I/O (bkpt #0xAB)                   │
│  - SYS_WRITE0: string output                    │
│  - SYS_OPEN/READ/FLEN/CLOSE: JS file loading     │
└─────────────────────────────────────────────────┘
```

### Software Stack

| Component | Details |
|---|---|
| FreeRTOS | ARM_CM4F port (compatible with Cortex-M55) |
| GC | BDWGC, NOSYS/bare-metal mode |
| ICU | Disabled |
| WASM | Disabled (no-op stub linked) |
| Threading | Disabled (single JS task) |
| libstdc++ | Full (nano unusable — see §11.5) |

---

## 3. Repository Layout

The porting work spans two repositories.

### 3.1 Main repo: `escargot` (`/home/ksh8281/work/escargot`)

- **Commit:** `9adc4cea0` "Support bare metal os(arm-none-eabi)"
- **Branches containing it:** `work74`, `origin/master`, `napi`
- **Changes:** `OS_BAREMETAL` platform support added to Escargot engine core (12 files, +93/-24 lines)
- **Details:** See §9

### 3.2 Porting repo: `escargot-rtos` (`/home/ksh8281/escargot-rtos`)

- **Commit:** `c44beaf` "Escargot FreeRTOS Cortex-M55 port — QEMU working, C++ exceptions, SunSpider 26/26"
- **Follow-up:** `b4d1dad` "Add BuiltinWASM.cpp stub to bare-metal build"
- **Branch:** `qemu-mps3-an547`
- **Changes:** FreeRTOS integration, linker script, startup code, platform glue, JS test runner (11 files, +1,877 lines)
- **Details:** See §10

### 3.3 Directory Structure (escargot-rtos)

```
escargot-rtos/
├── CMakeLists.txt              Build definition
├── FreeRTOSConfig.h            FreeRTOS configuration (stack sizes, tick, etc.)
├── FreeRTOS-Kernel/            FreeRTOS source (git submodule)
├── toolchain-cm55.cmake        CMake toolchain file (arm-none-eabi)
├── startup/
│   ├── an552_ns.ld             Linker script (memory map)
│   └── startup_SSE300.c        Reset_Handler, _sbrk, HardFault, semihosting
├── src/
│   ├── main.cpp                JS task, exception tests, SunSpider runner
│   ├── main.c                  Initial FreeRTOS-only prototype (no JS)
│   ├── escargot_platform.cpp   PlatformRef impl, stack base, tick ms
│   └── OSAllocatorBaremetal.cpp Yarr OSAllocator stub (malloc/free)
├── build/                      Build artifacts
│   ├── escargot-rtos.elf       (10.8 MB, with debug symbols)
│   ├── escargot-rtos-stripped.elf (1.1 MB)
│   ├── libfreertos.a
│   ├── libgc-lib.a
│   └── libescargot-lib.a
└── docs/
    └── porting-report.md       Original porting report (832 lines)
```

---

## 4. Target Specifications

### Simulator (current validation environment)

| Item | Value |
|---|---|
| Model | QEMU `mps3-an547` |
| CPU | Cortex-M55 (ARMv8.1-M, MVE/Helium) |
| SRAM | 4 MB @ `0x21000000` |
| DDR | 64 MB @ `0x60000000` |
| Peripherals | None (semihosting for I/O) |

### Real Hardware (BES2800BP, target)

| Item | Value |
|---|---|
| CPU | ARM Cortex-M55 @ 400 MHz |
| SRAM | 8.3 MB |
| PSRAM | 64 MB (JS heap) |
| Flash | 20 MB |
| OS | FreeRTOS |

---

## 5. Build Environment

### Toolchain

```
arm-none-eabi-gcc 13.2.1
arm-none-eabi-g++ 13.2.1
arm-none-eabi-ar, arm-none-eabi-strip
```

### Toolchain File (`toolchain-cm55.cmake`)

```cmake
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_C_COMPILER   arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)

# Skip link test during compiler detection
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CPU_FLAGS "-mcpu=cortex-m55 -mthumb -mfloat-abi=hard -mfpu=fpv5-d16")
set(CMAKE_C_FLAGS_INIT   "${CPU_FLAGS} -Wall -Os -ffunction-sections -fdata-sections")
set(CMAKE_CXX_FLAGS_INIT "${CPU_FLAGS} -Wall -Os -ffunction-sections -fdata-sections -fexceptions -fno-rtti")
```

### Key Compile Definitions

```cmake
# Escargot definitions
-DOS_BAREMETAL=1                    # bare-metal/RTOS target
-DESCARGOT_SMALL_CONFIG              # memory savings
-DNDEBUG                             # release mode
-DESCARGOT_32=1                      # NaN-boxing 32-bit mode
-DSTACK_FREESPACE_FROM_LIMIT=4096    # stack free space limit
-DSTACK_USAGE_LIMIT=131072           # 128 KB stack usage warning threshold

# BDWGC definitions
-DNOSYS                              # bare-metal mode (no syscalls)
-DSMALL_CONFIG
-DGC_ATOMIC_UNCOLLECTABLE            # enable GC_malloc_atomic_uncollectable
-DGC_DISABLE_INCREMENTAL            # disable incremental GC
-DDONT_USE_MMAP                      # no mmap
-DIGNORE_DYNAMIC_LOADING             # disable dlopen
-DGC_NO_SIGHANDLER                   # no signal handler
-DGC_NO_DLOPEN
"-DGC_INITIAL_HEAP_SIZE=(4*1024*1024)"  # 4 MB initial heap
```

### Linker Flags

```cmake
-T startup/an552_ns.ld              # linker script
-Wl,--gc-sections                    # remove unused sections
-Wl,-Map=${CMAKE_BINARY_DIR}/escargot-rtos.map
--specs=nosys.specs                  # do NOT use nano.specs (see §11.5)
-lc -lstdc++                         # full libstdc++ (exception support)
```

### Build Instructions

```bash
cd ~/escargot-rtos
mkdir -p build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=../toolchain-cm55.cmake
cmake --build .
```

> **Note:** Before building, run a host build of the main escargot repo once to generate
> the Yarr canonicalize tables and Unicode identifier tables.
> (`build/escargot_generated/yarr/`, `build/escargot_generated/parser/`)

---

## 6. QEMU Emulator Setup

### Run Command

```bash
qemu-system-arm \
  -machine mps3-an547 \
  -cpu cortex-m55 \
  -semihosting \
  -semihosting-config enable=on,target=native \
  -kernel build/escargot-rtos.elf \
  -nographic \
  -serial null
```

### Option Reference

| Option | Meaning |
|---|---|
| `-machine mps3-an547` | ARM MPS3 FPGA board (AN547 config), Cortex-M55 |
| `-cpu cortex-m55` | ARMv8.1-M core, MVE included |
| `-semihosting` | Host file I/O via `bkpt #0xAB` |
| `-semihosting-config enable=on,target=native` | Native semihosting (direct host communication) |
| `-nographic` | No GUI |
| `-serial null` | Ignore UART output (all I/O via semihosting) |

### Semihosting I/O Implementation

All I/O is routed to the host via ARM semihosting (`bkpt #0xAB`).

```c
// SYS_WRITE0 (r0=4): print null-terminated string
static void semi_puts(const char* s) {
    register const char* r1 __asm("r1") = s;
    register int r0 __asm("r0") = 4;
    __asm volatile("bkpt #0xAB" : "+r"(r0) : "r"(r1) : "memory");
}

// SYS_OPEN  (r0=1): open file, returns handle
// SYS_FLEN  (r0=0x0C): file size
// SYS_READ  (r0=6): read file (returns bytes NOT transferred)
// SYS_CLOSE  (r0=2): close file
// SYS_WRITE  (r0=5): write to file
```

SunSpider test JS files are loaded directly from the host filesystem via semihosting
(relative paths based on QEMU's current working directory).

### QEMU Limitations

- **No semihosting in HardFault context:** `bkpt #0xAB` in fault handler returns `r0=30` (SYS_WRITE failure). Only works in normal context.
- **Inaccurate timing:** QEMU instruction speed differs from real hardware. SunSpider timing results are meaningless.
- **MVE instructions:** QEMU mps3-an547 supports MVE but performance differs from real hardware.
- **Shutdown:** After JS task completes, it enters a `vTaskDelay` loop — QEMU does not auto-exit. Use `Ctrl-A X` to quit.

### FVP Alternative

```bash
# Arm FVP_Corstone_SSE-300 can be used but currently segfaults (uninvestigated)
FVP_Corstone_SSE-300 \
  --data CPU0=build/escargot-rtos.elf@0x60000000 \
  -C cpu0.INITVTOR=0x60000000
```

---

## 7. Memory Map

### Linker Script Regions (`startup/an552_ns.ld`)

```
Region  Origin      Length   Usage
─────────────────────────────────────────────
ITCM    0x00000000  512 KB   Vector table + Reset_Handler
SRAM    0x21000000    2 MB   .data / .bss / FreeRTOS stack / MSP stack
DDR     0x60000000   64 MB   Code / unwind tables / GC heap
```

### Actual Build Layout

```
Address       Section            Size     Description
──────────────────────────────────────────────────────────────────
0x00000000    .vectors           64 B     Cortex-M vector table (ITCM)

0x60000000    .text              1,527 KB Code + rodata (DDR)
0x6017D970    .init_array        56 B     C++ global constructor pointer array
0x6017D9AC    .ARM.exidx         47 KB    ARM EHABI exception index table
0x60189734    .ARM.extab         74 KB    ARM EHABI unwind opcodes + LSDA
0x6019BF18    .gcc_except_table  0 B      (LSDA embedded in .ARM.extab)
0x6019BF18    .data (LMA)        2.8 KB   SRAM copy source

0x21000000    .data (VMA)        2.8 KB   Initialized globals (SRAM)
0x21000A88    .bss               97 KB    Uninitialized globals
0x21018D88    js_stack           512 KB   FreeRTOS JS task stack
0x21089178    .stack (MSP)       64 KB    Reset/IRQ main stack
0x21099178    __stack_top__      —        MSP initial value

0x601A0000    GC heap (start)    ~62 MB   _sbrk allocation, DDR remainder
```

### GC Heap Calculation

```
__gc_heap_start__ = ALIGN(__etext + SIZEOF(.data), 65536)
                  = 0x601A0000  (current binary)
__gc_heap_size__  = DDR_end - __gc_heap_start__
                  = 0x64000000 - 0x601A0000
                  = ~62 MB
```

---

## 8. Binary Size

### By Section

| Section | Size | Storage |
|---|---|---|
| `.text` (code + rodata) | 1,527 KB | Flash/DDR |
| `.ARM.exidx` + `.ARM.extab` | 121 KB | Flash/DDR |
| `.init_array` | 0.1 KB | Flash/DDR |
| `.data` (LMA) | 2.8 KB | Flash/DDR |
| **Flash total** | **~1.61 MB** | |
| `.bss` + `js_stack` (512 KB) | 609 KB | SRAM |
| `.stack` (MSP) | 64 KB | SRAM |
| **SRAM total** | **~673 KB** | |

### ELF File

| Variant | Size |
|---|---|
| With debug symbols (`.elf`) | 10.8 MB |
| Stripped (`arm-none-eabi-strip -s`) | **1.7 MB** |

Uses approximately **8.1%** of BES2800BP's 20 MB Flash.

> Using `--specs=nano.specs` (libstdc++_nano) reduces this to ~1.13 MB, but
> C++ exceptions do not work at all — see §11.5.

---

## 8.5 Runtime Memory Usage Analysis (QEMU Measured)

The following data was measured by running `escargot-rtos.elf` on QEMU `mps3-an547`
and capturing the semihosting output. The `print_gc_stats()` function in `main.cpp`
reports `GC_get_heap_size()` (reserved heap) and `GC_get_bytes_since_gc()` (allocated
since last collection) at each phase.

### QEMU Console Output (Hello World Phase)

```
[boot] Reset_Handler entered
[boot] calling main
[main] entered
[js] task started
[js] context ready
[gc] init       h=4096K s=177K      ← after engine init + context creation
[ok]  1+2 => 3                       ← hello world
[cpp-exc] OK
```

### Memory Map: Reserved vs Used

#### Flash / DDR (Code + Read-Only Data)

| Item | Reserved | Used | Utilization |
|---|---|---|---|
| `.text` (code + rodata) | — | 1,526 KB | |
| `.ARM.exidx` (exception index) | — | 47 KB | |
| `.ARM.extab` (unwind + LSDA) | — | 74 KB | |
| `.init_array` | — | 0.05 KB | |
| `.data` (LMA, copy-down source) | — | 2.6 KB | |
| **Flash/DDR total** | **20 MB** (BES2800BP) | **~1,650 KB** | **8.1%** |

#### SRAM (Data + BSS + Stack)

| Item | Reserved | Used | Utilization |
|---|---|---|---|
| `.data` (initialized globals) | — | 2.6 KB | |
| `.bss` (uninitialized globals) | — | 546 KB | |
| `js_stack` (FreeRTOS JS task) | 512 KB | ~261 KB (measured) | 51% |
| `.stack` (MSP, Reset/IRQ) | 64 KB | — | |
| **SRAM total** | **8.3 MB** (BES2800BP) | **~610 KB** | **7.3%** |

#### GC Heap (DDR, managed by BDWGC via `_sbrk`)

| Phase | Reserved (h) | Used (s) | Utilization |
|---|---|---|---|
| **Hello world (`1+2`)** | **4,096 KB** | **177 KB** | **4.3%** |
| After 3d-cube | 4,096 KB | 1,977 KB | 48.3% |
| After access-binary-trees | 4,096 KB | 3,654 KB | 89.2% |
| After date-format-tofte | 6,144 KB (expanded) | 5,349 KB | 87.1% |
| After regexp-dna | 10,240 KB (expanded) | 2,051 KB | 20.0% |
| After string-tagcloud | 10,240 KB | 7,880 KB | 76.9% |
| **Final (all SunSpider done)** | **10,264 KB** | **0 KB** (after GC) | **0%** |

> - GC heap starts at 4 MB (`GC_INITIAL_HEAP_SIZE`) and auto-expands via `_sbrk` from DDR.
> - 5 automatic GC collections were triggered during SunSpider.
> - Peak heap reached ~10 MB, well within the 64 MB PSRAM target.
> - At hello world, only 177 KB of the 4 MB reserved heap is used.

### Summary: Reserved vs Used

| Memory Region | Reserved (Target) | Used (Hello World) | Used (Peak, SunSpider) |
|---|---|---|---|
| **Flash** | 20 MB | 1,650 KB (8.1%) | 1,650 KB (8.1%) |
| **SRAM** | 8.3 MB | 610 KB (7.3%) | 610 KB (7.3%) |
| **GC Heap** | 64 MB (PSRAM) | 177 KB (0.3%) | 10,264 KB (15.6%) |
| **Total** | **92.3 MB** | **~2,437 KB (2.6%)** | **~12,524 KB (13.3%)** |

> At hello world, the entire Escargot FreeRTOS system uses only **~2.4 MB** of the
> **92.3 MB** available on the BES2800BP target — a **97.4% headroom**.

---

## 9. Main Repo Changes (OS_BAREMETAL Support)

Commit `9adc4cea0` in the main escargot repo adds `OS_BAREMETAL` platform support
to the Escargot engine core. 12 files changed, +93/-24 lines.

### 9.1 `src/Escargot.h` — OS Detection Branch

```cpp
#elif defined(_POSIX_VERSION)
#define OS_POSIX 1
+#elif defined(OS_BAREMETAL)
+/* bare-metal / RTOS target — no POSIX */
#else
#error "failed to detect target OS"
#endif
```

When `-DOS_BAREMETAL=1` is defined, POSIX header dependencies are avoided.

### 9.2 `src/runtime/ThreadLocal.cpp` — Stack Base Lookup and thread_local Removal

**Stack base lookup:**

```cpp
#elif defined(OS_BAREMETAL)
extern "C" void* escargot_get_stack_base(void);
// ...
#elif defined(OS_BAREMETAL)
    void* stackStartAddress = escargot_get_stack_base();
    void* stackEndAddress = nullptr;
    (void)stackEndAddress;
```

On POSIX, stack info is obtained via `pthread_getattr_np`. On bare-metal, the
platform-provided `escargot_get_stack_base()` function is called instead.

**thread_local → static conversion:**

```cpp
+#if defined(OS_BAREMETAL)
+static bool g_globalsInited;
+#else
 thread_local bool g_globalsInited;
+#endif
```

Since only a single task runs on bare-metal, `thread_local` is replaced with
plain `static`. `GlobalDeleteChecker` follows the same pattern.

### 9.3 `src/runtime/DateObject.cpp` — clock_gettime Stub

```cpp
+#elif defined(OS_BAREMETAL)
+extern "C" uint32_t escargot_get_tick_ms(void);
+static int clock_gettime(int, struct timespec* spec)
+{
+    uint32_t ms = escargot_get_tick_ms();
+    spec->tv_sec = ms / 1000;
+    spec->tv_nsec = (ms % 1000) * 1000000;
+    return 0;
+}
```

Time APIs like `Date.now()` are implemented using the RTOS tick count.

### 9.4 `src/heap/Heap.cpp` — GC Parameter Tuning

```cpp
+#if defined(OS_BAREMETAL)
+    GC_set_free_space_divisor(1);
+#endif
```

`free_space_divisor=1` causes GC to run only when the heap is nearly full.
This prevents hundreds of GC collections during builtin initialization on the
~1 MIPS FVP simulator.

### 9.5 `src/runtime/ValueInlines.h` — ARM Type Matching Fix (Critical)

**Problem:** With `arm-none-eabi` GCC, `int32_t = long int` and `uint32_t = unsigned long int`.
On x86, `int32_t = int`, so there is no issue. But on ARM bare-metal, `long != int`, so
the `Value(long)` constructor calls `Value(int32_t)` → `Value(long)` in an infinite recursion.

**Fix pattern (9 overloads):**

```cpp
// Before (infinite recursion)
inline Value::Value(long i) {
    const int32_t asInt32 = static_cast<int32_t>(i);
    *this = Value(asInt32);  // asInt32 type is long → Value(long) recursion!
}

// After
inline Value::Value(long i) {
    const int32_t asInt32 = static_cast<int32_t>(i);
    *this = Value(static_cast<int>(asInt32));  // Value(int) = base impl
}
```

Affected overloads: `Value(char)`, `Value(unsigned char)`, `Value(short)`,
`Value(unsigned short)`, `Value(unsigned)`, `Value(long)`, `Value(unsigned long)`,
`Value(long long)`, `Value(unsigned long long)`

Additionally, `payload()` return type changed from `int32_t` to `intptr_t`, and
the `FromPayloadTag` constructor cast was unified from `reinterpret_cast` to `static_cast`.

### 9.6 `src/runtime/BigInt.cpp` — bf_get_int32 Type Fix

```cpp
// Before
bf_get_int32(&v2, src->bf(), 0);  // v2 is slimb_t = long

// After
{
    int _tmp32;
    bf_get_int32(&_tmp32, src->bf(), 0);
    v2 = _tmp32;
}
```

`bf_get_int32` expects `int*` but passing `slimb_t` (= `long`) directly causes a type
mismatch on ARM. A temporary `int` variable is used as an intermediary.

### 9.7 `src/runtime/Object.h` — size_t Overload

```cpp
-#if defined(OS_DARWIN)
+#if defined(OS_DARWIN) || defined(OS_BAREMETAL)
     explicit ObjectPropertyName(ExecutionState& state, const size_t& v)
```

On ARM bare-metal, `size_t` is defined as `unsigned long`, requiring an explicit overload.

### 9.8 `third_party/checked_arithmetic/CheckedArithmetic.h` — unsigned long + int Specialization

```cpp
+/* On platforms where uint32_t=unsigned long (e.g. arm-none-eabi),
+ * the 'unsigned long + int' combination lacks a specialization. */
+#if defined(__SIZEOF_LONG__) && (__SIZEOF_LONG__ == 4) && (__SIZEOF_INT__ == 4)
+template <typename ResultType>
+struct ArithmeticOperations<unsigned long, int, ResultType, false, true>
+    : ArithmeticOperations<unsigned, int, ResultType, false, true> {};
+template <typename ResultType>
+struct ArithmeticOperations<int, unsigned long, ResultType, true, false>
+    : ArithmeticOperations<int, unsigned, ResultType, true, false> {};
+#endif
```

### 9.9 `third_party/yarr/WTFBridge.h` and `PageBlock.cpp` — OS Branch and Page Size

```cpp
// WTFBridge.h
+#elif defined(OS_BAREMETAL)
+/* bare-metal / RTOS: no mmap, no errno */

// PageBlock.cpp
+#elif defined(OS_BAREMETAL)
+inline size_t systemPageSize() { return 4096; }
```

### 9.10 `third_party/GCutil` — Submodule Update

GCutil submodule updated to support bare-metal NOSYS mode.

---

## 10. escargot-rtos Porting Layer

### 10.1 `CMakeLists.txt` — Build Definition

Builds three static libraries and one executable.

| Target | Sources | Description |
|---|---|---|
| `freertos` | `tasks.c`, `queue.c`, `list.c`, `timers.c`, `heap_3.c`, `port.c` | FreeRTOS kernel (ARM_CM4F port) |
| `escargot-lib` | Escargot `src/*.cpp` + Yarr + simdutf + double_conversion + lz4 + libbf + xsum | Escargot engine (excl. shell/wasm/debugger/codecache/intl) |
| `gc-lib` | BDWGC C/C++ sources | Garbage collector (NOSYS mode) |
| `escargot-rtos` | `startup_SSE300.c`, `main.cpp`, `escargot_platform.cpp`, `OSAllocatorBaremetal.cpp` | Final ELF |

**Source filtering:**

```cmake
file(GLOB_RECURSE ESCARGOT_CPPSRC ${ESCARGOT_SRC}/*.cpp)
list(FILTER ESCARGOT_CPPSRC EXCLUDE REGEX ".*/shell/.*")
list(FILTER ESCARGOT_CPPSRC EXCLUDE REGEX ".*/wasm/.*")
list(FILTER ESCARGOT_CPPSRC EXCLUDE REGEX ".*/debugger/.*")
list(FILTER ESCARGOT_CPPSRC EXCLUDE REGEX ".*/codecache/.*")
list(FILTER ESCARGOT_CPPSRC EXCLUDE REGEX ".*/intl/.*")
# BuiltinWASM.cpp provides a no-op stub, so it is included as an exception
list(APPEND ESCARGOT_CPPSRC ${ESCARGOT_SRC}/wasm/BuiltinWASM.cpp)
```

**Linking:**

```cmake
target_link_libraries(escargot-rtos escargot-lib gc-lib freertos -lc -lstdc++)
```

### 10.2 `FreeRTOSConfig.h` — FreeRTOS Configuration

Key settings:

| Setting | Value | Description |
|---|---|---|
| `configUSE_PREEMPTION` | 1 | Preemptive scheduling |
| `configCPU_CLOCK_HZ` | 25000000 | FVP REFCLK (25 MHz) |
| `configTICK_RATE_HZ` | 1000 | 1 ms tick |
| `configMAX_PRIORITIES` | 8 | |
| `configMINIMAL_STACK_SIZE` | 256 words | |
| `configTOTAL_HEAP_SIZE` | 512 KB | FreeRTOS heap |
| `configNUM_THREAD_LOCAL_STORAGE_POINTERS` | 5 | |
| `configUSE_TIMERS` | 1 | Software timers enabled |
| `configSUPPORT_STATIC_ALLOCATION` | 1 | Static allocation support |
| `configSUPPORT_DYNAMIC_ALLOCATION` | 1 | Dynamic allocation support |

Interrupt handler mapping:

```c
#define vPortSVCHandler     SVC_Handler
#define xPortPendSVHandler  PendSV_Handler
#define xPortSysTickHandler SysTick_Handler
```

### 10.3 `startup/startup_SSE300.c` — C Startup

Reset_Handler performs:

1. **FPU/MVE enable:** `CPACR` register CP10/CP11 full access
2. **`.data` copy:** Flash(DDR) → SRAM
3. **`.bss` zero initialization**
4. **C++ global constructors:** iterate `__init_array_start` to `__init_array_end`
5. **Call `main()`**

newlib syscall stubs:

| Function | Implementation |
|---|---|
| `_write` | semihosting `SYS_WRITE` (r0=5) |
| `_sbrk` | Linear allocation from DDR GC heap region |
| `_close` | Returns -1 |
| `_fstat` | Returns 0 |
| `_isatty` | Returns 1 |
| `_lseek` | Returns -1 |
| `_read` | Returns -1 |
| `getpagesize` | Returns 4096 |
| `_setjmp` | Wraps `setjmp` |

`_sbrk` implementation:

```c
extern uint8_t __gc_heap_start__[];
extern uint8_t __gc_heap_size__[];

static uint8_t *_heap_ptr;
static uint8_t *_heap_end;

void *_sbrk(int incr) {
    if (!_heap_ptr) _heap_init();
    if (_heap_ptr + incr > _heap_end) return (void *)-1;
    void *prev = _heap_ptr;
    _heap_ptr += incr;
    return prev;
}
```

### 10.4 `startup/an552_ns.ld` — Linker Script

See §7. Key points:

```ld
/* C++ exception tables — KEEP is mandatory to prevent --gc-sections removal */
.ARM.exidx : {
    __exidx_start = .;
    KEEP(*(.ARM.exidx*))
    __exidx_end = .;
} > DDR
.ARM.extab : {
    KEEP(*(.ARM.extab*))
    . = ALIGN(4);
} > DDR
.gcc_except_table : {
    KEEP(*(.gcc_except_table))
    KEEP(*(.gcc_except_table.*))
    . = ALIGN(4);
    __etext = .;
} > DDR
```

### 10.5 `src/escargot_platform.cpp` — Platform Glue

```cpp
/* escargot_get_stack_base(): called by ThreadLocal::initialize().
 * Returns the bottom of the FreeRTOS JS task's stack buffer. */
void* escargot_get_stack_base(void) {
    extern void* g_js_stack_base;
    return g_js_stack_base;
}

/* escargot_get_tick_ms(): millisecond tick for Date.now() fallback. */
uint32_t escargot_get_tick_ms(void) {
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}
```

`PlatformRef` implementation is minimal:

- `markJSJobEnqueued()`: no-op (no event loop)
- `onLoadModule()`: returns "module loading unsupported" error
- `didLoadModule()`: no-op
- `hostImportModuleDynamically()`: no-op

### 10.6 `src/OSAllocatorBaremetal.cpp` — Yarr OSAllocator Stub

```cpp
void* OSAllocator::reserveUncommitted(size_t bytes, ...) { return malloc(bytes); }
void* OSAllocator::reserveAndCommit(size_t bytes, ...) { return malloc(bytes); }
void  OSAllocator::releaseDecommitted(void* ptr, size_t) { free(ptr); }
void  OSAllocator::commit(void*, size_t, bool, bool) {}
void  OSAllocator::decommit(void*, size_t) {}
```

Replaces the mmap-based original implementation with malloc/free.

### 10.7 `src/main.cpp` — JS Task and Test Runner

**Initialization sequence:**

1. `std::set_terminate(my_terminate)` — install terminate handler
2. C++ exception self-test (`throw 42; catch(int)`)
3. `Escargot::Globals::initialize(escargot_platform())` — engine init
4. `Escargot::VMInstanceRef::create()` — create VM instance
5. `Escargot::ContextRef::create()` — create context
6. Run `1 + 2` → verify `3`
7. Run 15 JS exception test cases
8. Run 26 SunSpider 1.0 tests sequentially
9. After each test: print GC stats and call `GC_gcollect()` manually
10. Enter `vTaskDelay` infinite loop

**FreeRTOS static task creation:**

```cpp
#define JS_STACK_WORDS 131072  // 512 KB
static StackType_t  js_stack[JS_STACK_WORDS];
static StaticTask_t  js_tcb;
void* g_js_stack_base = js_stack;

int main(void) {
    xTaskCreateStatic(js_task, "js", JS_STACK_WORDS, NULL,
                      tskIDLE_PRIORITY + 1, js_stack, &js_tcb);
    vTaskStartScheduler();
    for (;;);
}
```

**FreeRTOS static memory callbacks:**

```cpp
void vApplicationGetIdleTaskMemory(StaticTask_t** ppxIdleTaskTCBBuffer,
                                    StackType_t** ppxIdleTaskStackBuffer,
                                    uint32_t* pulIdleTaskStackSize) {
    *ppxIdleTaskTCBBuffer   = &idle_tcb;
    *ppxIdleTaskStackBuffer = idle_stack;
    *pulIdleTaskStackSize   = configMINIMAL_STACK_SIZE;
}
// Timer task callback follows the same pattern
```

---

## 11. Bugs Found and Fixed

### 11.1 `Value` Integer Constructor Infinite Recursion (Critical)

**File:** `escargot/src/runtime/ValueInlines.h`

**Cause:** With `arm-none-eabi` GCC, `int32_t = long int` and `uint32_t = unsigned long int`.
On x86, `int32_t = int`, so there is no issue. But on ARM bare-metal, `long != int`.

```cpp
// Before: in Value(long i)
*this = Value(asInt32);  // asInt32 type is int32_t = long → Value(long) recursion!

// After: explicit cast to int
*this = Value(static_cast<int>(asInt32));  // Value(int) = base impl
```

**Impact:** All 9 overloads affected: `Value(char/unsigned char/short/unsigned short/unsigned/long/unsigned long/long long/unsigned long long)`.

**Symptom:** JS task stack completely exhausted → HardFault.

**Diagnosis:**

```bash
arm-none-eabi-gcc -dM -E - < /dev/null | grep INT32_TYPE
# __INT32_TYPE__ = long int  ← patch needed
# __INT32_TYPE__ = int       ← no patch needed (x86)
```

### 11.2 JS Task Stack Overflow

**Cause:** `Globals::initialize()` → `initializeBuiltins()` recursively creates hundreds of builtin
objects, requiring up to ~261 KB of stack depth.

**Fix:** In `FreeRTOSConfig.h` and `main.cpp`:

```c
#define JS_STACK_WORDS 131072   // 512 KB (4 bytes × 131072)
```

> Actual usage ≈ 261 KB. Headroom ≈ 250 KB.

### 11.3 `Globals::finalize()` Hang

**Cause:** `GC_gcollect_and_unmap()` internally calls `munmap()`, which is unimplemented on bare-metal
→ infinite hang.

**Fix:** No process termination concept on bare-metal. `Globals::finalize()` call removed.

### 11.4 Semihosting — Does Not Work in HardFault Context

QEMU returns `r0=30` (SYS_WRITE failure) when `bkpt #0xAB` is executed in a fault handler.
Semihosting from fault context (MSP) is not supported by QEMU.
→ `HardFault_Handler` diagnostic output may be incomplete.

### 11.5 C++ Exception Hang — Four Root Causes

`throw/catch` would print `[TERMINATE] called` then hang. Four causes were found sequentially.

#### Cause A: `std::terminate()` Duplicate Definition

```cpp
// Attempted but failed
namespace std { void terminate() noexcept { _semi("..."); while(1); } }
// → collect2: multiple definition of std::terminate()
```

**Fix:** Remove `namespace std { }` block, install runtime handler instead:

```cpp
static void my_terminate() noexcept { _semi("[TERMINATE]\n"); __asm("bkpt #0"); while(1); }
// At js_task start:
std::set_terminate(my_terminate);
```

#### Cause B: `.ARM.extab` Removed by GC

`--gc-sections` linker option removes LSDA data in `.ARM.extab` as "unreferenced".
LSDA (Language Specific Data Area) = catch handler address table, type table.
Without it, `__gxx_personality_v0` cannot find handlers → `std::terminate()`.

**ARM EHABI structure:** x86 Linux `.gcc_except_table` ↔ ARM `.ARM.extab` (LSDA embedded)

```
.ARM.extab entry layout:
  [+0x00] prel31 → __gxx_personality_v0
  [+0x04] unwind opcodes (pop regs, finish)
  [+0x08] LSDA embedded:
            @LPStart_encoding (0xff = omit)
            @TType_encoding  (0x10 = pcrel)
            ttype_offset (ULEB128)
            call_site_encoding
            call_site_table_size
            [call site entries: start, len, landing_pad, action]
            [action table]
            [type table: prel31 → typeinfo]
```

**Fix:** Add KEEP to linker script (see §10.4).

#### Cause C: `libstdc++_nano.a` Missing Unwind Tables (Critical)

`--specs=nano.specs` uses `libstdc++_nano.a`, which omits `.ARM.exidx` entries for
exception runtime functions (`__cxa_throw`, `__gxx_personality_v0`, `__cxa_begin_catch`)
to reduce code size.

ARM EHABI `_Unwind_RaiseException` (= `__gnu_Unwind_RaiseException`) walks the stack
backwards, calling the personality function for each frame's exidx entry.
If `__cxa_throw`'s frame has no exidx entry → walk fails → `std::terminate()`.

**Diagnosis:**

```bash
arm-none-eabi-readelf -u escargot-rtos.elf | grep "^0x" | sort -V | \
  awk 'prev && strtonum($1)-strtonum(prev)>0x200 {print "GAP:", prev, "->", $1} {prev=$1}'

# With nano: GAP 0x600c802a -> 0x600cf670 (this range contains __cxa_throw)
# With full: __cxa_throw: 0x80aab0b0 (has compact unwind entry)
```

**Fix:** Remove `--specs=nano.specs`, use full libstdc++.

```cmake
# Before: --specs=nano.specs --specs=nosys.specs
# After:  --specs=nosys.specs
```

Flash: ~1.13 MB → ~1.61 MB (+0.48 MB).

#### Cause D: `GC_ATOMIC_UNCOLLECTABLE` Not Defined

After switching to full libstdc++, `GC_malloc_atomic_uncollectable()` fails to link.

```c
// GCutil/malloc.c
#ifdef GC_ATOMIC_UNCOLLECTABLE
GC_API void* GC_malloc_atomic_uncollectable(size_t lb) { ... }
#endif
```

**Fix:**

```cmake
# Add to CMakeLists.txt GCUTIL_DEFS
-DGC_ATOMIC_UNCOLLECTABLE
```

---

## 12. C++ Exception / JS Exception Verification

### 12.1 C++ Exception Self-Test

```
[cpp-exc] OK
```

```cpp
__attribute__((noinline))
static void cpp_exc_test(void) {
    bool ok = false;
    try { throw 42; } catch (int v) { ok = (v == 42); }
    semi_puts(ok ? "[cpp-exc] OK\n" : "[cpp-exc] BROKEN\n");
}
```

> `__attribute__((noinline))` is mandatory: inlining try/catch inside a large function like
> `js_task` can cause incorrect per-function exidx entries.

### 12.2 JS Exception Behavior (15 Cases)

| # | Test | Output |
|---|---|---|
| 1 | `throw 42` (uncaught) | ✅ `[THROW] uncaught-throw-42` |
| 2 | `try { throw 42 } catch(e) { e }` | ✅ `42` |
| 3 | `throw new Error('hello')` → `.message` | ✅ `"hello` |
| 4 | `null.x` (uncaught TypeError) | ✅ `[THROW] uncaught-null-deref` |
| 5 | catch TypeError, `instanceof TypeError` | ✅ `true` |
| 6 | catch, `.name` | ✅ `"TypeError` |
| 7 | `new Array(-1)` → `.name` | ✅ `"RangeError` |
| 8 | `finally` (during throw propagation) | ✅ `[THROW] finally-runs` |
| 9 | `catch + finally`, value accumulation | ✅ `8` |
| 10 | re-throw → `.message` | ✅ `"orig` |
| 11 | nested try/catch | ✅ `"innerouter` |
| 12 | `typeof e.stack` | ✅ `"string` |
| 13 | `new Function('}{')` → instanceof SyntaxError | ✅ |
| 14 | throw in forEach callback caught | ✅ `3` |
| 15 | `Promise.reject(55)` → typeof | ✅ `"object` |

**15 / 15 passed**

---

## 13. GC Behavior Analysis

### Configuration

```c
GC_set_free_space_divisor(1);    // collect only when heap is nearly full
// GC_INITIAL_HEAP_SIZE = 4 MB  (defined in CMakeLists.txt)
```

### Observations During SunSpider

| Phase | heap | Auto GC |
|---|---|---|
| After context creation | 4,096 KB | — |
| After `3d-cube` | 4,096 KB | #1 (1,977 KB allocated) |
| After `3d-morph` | 4,096 KB | #2 (754 KB) |
| After `3d-raytrace` | 4,096 KB | #3, #4 |
| After `access-binary-trees` | 4,096 KB | #5 (3,654 KB) |
| After `date-format-tofte` | **6,144 KB** | Heap expansion: 4→6 MB |
| After `regexp-dna` | **10,240 KB** | Heap expansion: 6→10 MB |
| After `string-validate-input` | 10,248 KB | — |
| After manual `GC_gcollect()` | 10,264 KB | 0 KB since |

- 5 automatic GC collections triggered
- Heap auto-expanded: 4 → 6 → 10 MB (via `_sbrk` from DDR)
- Peak heap ~10 MB << 64 MB PSRAM

---

## 14. SunSpider 1.0 Results

All 26 tests completed on QEMU `mps3-an547`. (Performance measurement requires real hardware.)

| Test | Result | GC since (KB) |
|---|---|---|
| 3d-cube | ✅ | 1,977 |
| 3d-morph | ✅ | 754 |
| 3d-raytrace | ✅ | 1,201 |
| access-binary-trees | ✅ | 3,654 |
| access-fannkuch | ✅ | 16 |
| access-nbody | ✅ | 39 |
| access-nsieve | ✅ | 553 |
| bitops-3bit-bits-in-byte | ✅ | 4 |
| bitops-bits-in-byte | ✅ | 3 |
| bitops-bitwise-and | ✅ | 4 |
| bitops-nsieve-bits | ✅ | 181 |
| controlflow-recursive | ✅ | 4 |
| crypto-aes | ✅ | 1,123 |
| crypto-md5 | ✅ | 165 |
| crypto-sha1 | ✅ | 164 |
| date-format-tofte | ✅ | 5,349 |
| date-format-xparb | ✅ | 3,061 |
| math-cordic | ✅ | 20 |
| math-partial-sums | ✅ | 7 |
| math-spectral-norm | ✅ | 24 |
| regexp-dna | ✅ | 2,051 |
| string-base64 | ✅ | 1,634 |
| string-fasta | ✅ | 3,308 |
| string-tagcloud | ✅ | 7,880 |
| string-unpack-code | ✅ | 347 |
| string-validate-input | ✅ | 1,943 |

**26 / 26 passed**

---

## 15. Real Hardware Porting Guide

Checklist for porting to BES2800BP or similar Cortex-M55 boards.

### 15.1 Linker Script Modification

Modify `startup/an552_ns.ld` to match the actual hardware memory map:

```ld
MEMORY {
    ITCM (rx)  : ORIGIN = 0x????????, LENGTH = ???K   /* vectors + startup */
    SRAM (rwx) : ORIGIN = 0x????????, LENGTH = ????K  /* data/bss/stack */
    FLASH(rx)  : ORIGIN = 0x????????, LENGTH = ???M   /* code resident */
    PSRAM(rwx) : ORIGIN = 0x????????, LENGTH = ???M   /* GC heap */
}
```

BES2800BP example:

```ld
MEMORY {
    SRAM  (rwx) : ORIGIN = 0x20000000, LENGTH = 8192K   /* 8.3 MB */
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 20480K  /* 20 MB */
    PSRAM (rwx) : ORIGIN = 0x60000000, LENGTH = 65536K  /* 64 MB PSRAM */
}
```

> Code is 1.61 MB, so Flash XIP (eXecute In Place) support is required if placing code in Flash.
> If Flash XIP is not supported, copy code to PSRAM and execute from there.

### 15.2 Startup File Modification

Replace `startup/startup_SSE300.c` with a board-specific version:

1. **FPU/MVE enable:** `CPACR` register setup (current code reusable)
2. **`.data` copy:** Flash → SRAM (current code reusable)
3. **`.bss` init:** (current code reusable)
4. **Remove semihosting:** Replace with UART driver on real hardware
5. **Modify `_sbrk`:** Change to PSRAM address range

```c
// Example real hardware UART output (adapt to board UART driver)
void board_puts(const char* s) {
    while (*s) {
        while (!(UART0->SR & UART_SR_TXE));  // wait until TX buffer empty
        UART0->DR = *s++;
    }
}
```

### 15.3 JS Source File Loading

Current implementation: semihosting `SYS_OPEN` / `SYS_READ` (QEMU only).
On real hardware, JS source must be read from storage (Flash/SD card/network):

```cpp
// Current (QEMU semihosting)
char* src = load_file("path/to/script.js", &sz);

// Real hardware: embedded JS source from Flash
extern const char _js_bundle_start[];
extern const char _js_bundle_end[];
int sz = _js_bundle_end - _js_bundle_start;
const char* src = _js_bundle_start;
```

Add a JS bundle section to the linker script:

```ld
.js_bundle : {
    _js_bundle_start = .;
    KEEP(*(.js_bundle))
    _js_bundle_end = .;
} > FLASH
```

### 15.4 Stack Size Verification

Current 512 KB JS task stack, actual usage ≈ 261 KB.
On real hardware, use `uxTaskGetStackHighWaterMark()` to verify and minimize:

```c
UBaseType_t watermark = uxTaskGetStackHighWaterMark(NULL);
// Unit: words (4 bytes). Example: 64000 → 256 KB free
```

### 15.5 GC Heap Tuning

```cmake
"-DGC_INITIAL_HEAP_SIZE=(4*1024*1024)"   # keep 4 MB initial value
# _sbrk auto-expands up to __gc_heap_size__ from linker script
```

```c
// free_space_divisor: 1=minimal (current), 4=default, higher=more frequent
GC_set_free_space_divisor(2);  // tune for real hardware memory
```

### 15.6 Clock Configuration

```c
uint32_t SystemCoreClock = 400000000U;  // BES2800BP: 400 MHz
```

```c
// FreeRTOSConfig.h
#define configCPU_CLOCK_HZ    ( ( unsigned long ) 400000000 )
#define configTICK_RATE_HZ    ( ( TickType_t ) 1000 )
```

---

## 16. Porting to Other RTOSes

### 16.1 Common Requirements

Any RTOS must satisfy the following:

| Requirement | Details |
|---|---|
| **Stack size** | JS task minimum 384 KB (512 KB recommended) |
| **Heap supply** | `_sbrk(int incr)` implementation required (called by BDWGC) |
| **`getpagesize()`** | Stub returning `4096` |
| **`_setjmp`** | newlib stub: `int _setjmp(jmp_buf e) { return setjmp(e); }` |
| **Exception linking** | Full libstdc++ (nano unusable), `KEEP(.ARM.extab*)` |
| **`GC_ATOMIC_UNCOLLECTABLE`** | Must be defined when building BDWGC |
| **`Globals::finalize()` removal** | Hangs in environments without `munmap` |

### 16.2 Zephyr RTOS

```c
K_THREAD_DEFINE(js_tid, 512 * 1024, js_task, NULL, NULL, NULL, 5, 0, 0);
// Zephyr does not provide _sbrk directly
// Set CONFIG_HEAP_MEM_POOL_SIZE sufficiently or implement separately
```

### 16.3 ThreadX / Azure RTOS

```c
TX_THREAD js_thread;
ULONG js_stack[131072];  // 512 KB

tx_thread_create(&js_thread, "JS", js_task_entry, 0,
                 js_stack, sizeof(js_stack),
                 10, 10, TX_NO_TIME_SLICE, TX_AUTO_START);
```

### 16.4 Mbed OS

```cpp
Thread js_thread(osPriorityNormal, 512 * 1024);
js_thread.start(js_task);
```

### 16.5 FreeRTOS (Current Implementation)

```c
static StackType_t  js_stack[131072];
static StaticTask_t js_tcb;

xTaskCreateStatic(js_task, "js", 131072, NULL,
                  tskIDLE_PRIORITY + 1, js_stack, &js_tcb);
vTaskStartScheduler();
```

### 16.6 No OS (Bare Scheduler)

Can also run in a simple while loop. Note: no preemption, so other tasks are blocked
while JS is executing:

```c
int main(void) {
    heap_init();  // _sbrk init
    js_task(NULL);
    while(1);
}
```

---

## 17. Future Work

| Item | Priority | Description |
|---|---|---|
| BES2800BP real hardware porting | High | Linker script + UART + clock changes |
| JS source file loading | High | semihosting → Flash-embedded or filesystem |
| SunSpider execution time measurement | Medium | QEMU timing is meaningless; only meaningful on real hardware |
| Stack minimization | Medium | Currently 512 KB, measured 261 KB → calculate optimal with 20% headroom |
| GC parameter tuning | Medium | `free_space_divisor`, initial heap size based on real measurements |
| Flash size optimization | Low | Explore `-flto` applicability |
| FVP_Corstone_SSE-300 | Low | Investigate segfault when using FVP instead of QEMU |
| UART driver integration | Low | Remove semihosting dependency |
