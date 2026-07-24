# Escargot RTOS Porting Guide

Short, generic checklist + contract for porting Escargot to a new
bare-metal/RTOS target (Zephyr, ThreadX, Mbed OS, a from-scratch bare-metal
scheduler, ...). This is deliberately NOT a narrative walkthrough of any
one port -- for two complete, CI-verified worked examples, see:

- [`samples/rtos/freertos/`](../../samples/rtos/freertos) -- the FreeRTOS /
  Cortex-M55 (QEMU mps3-an547) bare-metal port, an in-tree, CI-verified
  sample (see `.github/workflows/rtos-freertos.yml`) built directly against
  this repo's own sources.
- [`samples/rtos/nuttx/`](../../samples/rtos/nuttx) -- the NuttX /
  Cortex-M55 (QEMU mps3-an547) port, also an in-tree, CI-verified sample
  (`.github/workflows/rtos-nuttx.yml`): `interpreters-escargot/` is the
  escargot NSH app itself (drop it into `apps/interpreters/` of your own
  NuttX+apps checkout), `escargot-lib-cmake/` is the out-of-tree CMake
  project that builds the engine for it. Unlike FreeRTOS-Kernel, NuttX
  itself is NOT vendored here -- a NuttX app needs a full NuttX+apps
  source tree, not a standalone library dependency, so the CI job checks
  those out at pinned commits instead (see that job and
  `samples/rtos/nuttx/patches/` for a one-file board patch this exact pin
  needed).

Both are proven working (26/26 SunSpider 1.0.2, 15/15 exception regression
cases). This document only gives you the distilled checklist and the code
contract those two
ports converged on.

## 0. Build the engine itself: `-DESCARGOT_HOST=baremetal`

Before writing any port-specific glue, configure the main engine build
with the dedicated host value:

```sh
cmake -DESCARGOT_HOST=baremetal -DESCARGOT_ARCH=arm ... /path/to/escargot
```

This sets exactly the engine-side (`escargot` target) definitions/flags a
bare-metal/RTOS port needs -- `-DOS_BAREMETAL=1`, the right 32/64-bit
mode per `ESCARGOT_ARCH`, `-Wl,--gc-sections`, and (see `CMakeLists.txt`)
`ESCARGOT_LIBICU_SUPPORT`/`ESCARGOT_THREADING` both defaulted `OFF`
(no ICU, no thread runtime on these targets). A new port does not need to
independently rediscover this list and hand-copy it into its own
CMakeLists.txt -- both existing reference ports instead
`add_subdirectory()` this repo's own top-level `CMakeLists.txt` directly
(with `ESCARGOT_HOST`/`ESCARGOT_ARCH`/`ESCARGOT_OUTPUT`/`ESCARGOT_MODE`
set as cache variables beforehand) to get a real, correctly cross-compiled
`escargot` static-library target, instead of hand-globbing `src/*.cpp` and
hand-copying `ESCARGOT_DEFS` themselves -- see
`samples/rtos/nuttx/escargot-lib-cmake/CMakeLists.txt` /
`samples/rtos/freertos/CMakeLists.txt`'s own "─── Escargot ───" section
for the current, proven-working pattern to copy. (An earlier revision of
both files did hand-copy the glob/defs list instead; that duplication has
since been eliminated in favor of this reuse.) Building the main
top-level project this way for `ESCARGOT_HOST=baremetal` deliberately
skips `ADD_SUBDIRECTORY(third_party/runtime_icu_binder)` (see the guards
in `build/escargot.cmake`) -- it unconditionally needs `<dlfcn.h>`, which
doesn't exist on a newlib/nosys target, so it's out of scope for this
host entirely. BDWGC (`third_party/GCutil`) IS built as part of this same
`ADD_SUBDIRECTORY()` call -- see below for the cache variables a new port
needs to set first.

**This DOES build BDWGC (`third_party/GCutil`) for you, but needs
port-specific cache variables set first.** BDWGC's own
`third_party/GCutil/CMakeLists.txt` has an opt-in NOSYS/bare-metal mode
(`GCUTIL_NOSYS_BAREMETAL`, default `OFF`) selecting a curated source-file
subset and a different define set than its normal hosted build (which
assumes pthreads/mmap are available) -- see that file's own comments, and
`gcconfig.h`'s `ARM32`/`NOSYS` branch, for what it actually expects. It
defaults `OFF` so it's a no-op for every hosted platform;
`-DESCARGOT_HOST=baremetal` does not flip it on automatically. A new RTOS
port sets `GCUTIL_NOSYS_BAREMETAL=ON` plus its own
`GCUTIL_INITIAL_HEAP_SIZE` (varies per port -- the FreeRTOS sample uses
4 MB, the NuttX sample uses 1 MB) and any further
`GCUTIL_CFLAGS_FROM_EXTERNAL` it needs (e.g. NuttX's
`-D_setjmp=setjmp -D_longjmp=longjmp` remap) as cache variables **before**
the single top-level `add_subdirectory()` call above -- CMake `CACHE`
variables are process-global, so `build/escargot.cmake`'s own nested
`ADD_SUBDIRECTORY(third_party/GCutil)` picks them straight up, no separate
`add_subdirectory()` needed in the port's own CMakeLists.txt -- see either
existing port's own `CMakeLists.txt` for a complete, proven example to
copy from. The resulting target is always named `gc-lib` (GCutil's own
fixed name, matching the hosted build too).

**A subtle `-DOS_BAREMETAL=1` detection-order gotcha**: `src/Escargot.h`'s
OS-family detection checks `defined(OS_BAREMETAL)` before the generic
Apple/Linux/Unix/`_POSIX_VERSION` heuristics, specifically because some
bare-metal libcs with a partial POSIX compatibility layer (NuttX's, for
one) define `_POSIX_VERSION` themselves even though there's no real POSIX
environment underneath -- if your target's libc does something similar,
make sure you're on a version of this repo with that ordering fix (an
earlier version checked the heuristics first, which silently mis-detected
`OS_POSIX` and broke compiling `third_party/yarr/OSAllocatorPosix.cpp`
under such a libc, even though it's dead code on a real bare-metal
target).

(Verified for this guide: `cmake -DESCARGOT_HOST=baremetal -DESCARGOT_ARCH=arm
-DESCARGOT_OUTPUT=shell -S <repo> -B <builddir>` configures cleanly from a
clean out-of-tree build directory and prints `ESCARGOT_DEFINITIONS`
containing `-DOS_BAREMETAL=1`, with `ESCARGOT_LIBICU_SUPPORT: OFF` and
`ESCARGOT_THREADING: OFF` -- it isn't expected to produce a working Linux
binary, since `src/shell/Shell.cpp`'s embedder doesn't implement the
`PlatformRef` methods in §2 below; a full working build only happens
inside an actual RTOS port's own project, same as today.)

## 1. Common requirements checklist

Any RTOS port needs all of these, regardless of which RTOS:

| Requirement | Detail |
|---|---|
| JS task stack | ~1 MB+ recommended. 256 KB measured insufficient (NuttX port); FreeRTOS port uses 512 KB with ~261 KB actually used by `Globals::initialize()`'s builtin construction -- size in proportion to your `ESCARGOT_SMALL_CONFIG`/builtin set. |
| Heap supply | A `sbrk(intptr_t increment)`-shaped function BDWGC's `GET_MEM`/NOSYS allocator path calls, over a linker-reserved region (`__gc_heap_start__`/`__gc_heap_size__`) -- no `malloc()` involved, so it doesn't depend on the RTOS's own heap allocator being initialized yet. The bump-pointer logic itself is shared for every port in `src/heap/SbrkBaremetal.cpp`'s `escargotBaremetalSbrk()` -- a new port only needs its own `__gc_heap_start__`/`__gc_heap_size__` linker symbols plus a 1-line forwarder (`_sbrk()` or `sbrk()`, whichever your libc needs -- see that file's own comment) in one of its own, directly-linked (not static-library-only) source files. |
| Heap is grow-only | This `sbrk()` heap never gives memory back to the OS: `GC_unix_sbrk_get_mem` (`third_party/GCutil/os_dep.c`) only ever calls `sbrk()` with a **positive** increment, and `-DDONT_USE_MMAP` (which every port defines) compiles `GC_unmap()` out entirely -- there is no shrink/give-back path in this configuration at all. This is a deliberate, correct choice (avoids depending on any OS free-list/heap semantics a bare-metal target doesn't have), not a limitation to work around -- don't go looking for a give-back mechanism, there isn't one. |
| `mprotect`-based incremental GC is out of scope | BDWGC only uses `mprotect()` for `MPROTECT_VDB` (incremental GC's dirty-page tracking, which needs a working `mprotect()` **and** a `SIGSEGV`-equivalent fault-delivery mechanism). Every port defines `-DGC_DISABLE_INCREMENTAL`, which turns this whole mechanism off -- correctly, since bare-metal targets generally have neither MMU-based page protection nor signal delivery set up. There is nothing to abstract or hook here; it's an entire disabled feature category, not a missing `PlatformRef` hook -- don't spend time trying to wire up an `mprotect`-based VDB for a bare-metal port. |
| Full libstdc++ | Do NOT use `nano.specs` -- `libstdc++_nano.a` is missing `.ARM.exidx` entries for exception-runtime functions (`__cxa_throw`, `__gxx_personality_v0`, ...), causing a silent `std::terminate()` the first time a C++/JS exception unwinds. Link plain `--specs=nosys.specs` (or your RTOS's non-nano equivalent) instead. |
| Linker script `KEEP()` | `.ARM.exidx`/`.ARM.extab` (or your arch's unwind-table sections) must be `KEEP()`-ed; `--gc-sections` will otherwise strip them as "unreferenced" (they're only referenced by the unwinder at runtime, not by any visible call). |
| `-DGC_ATOMIC_UNCOLLECTABLE` | Required at BDWGC build time -- part of GCutil's own `GCUTIL_NOSYS_BAREMETAL` define set (§0), not something your project needs to define itself. |
| `-DENABLE_DISCLAIM` | Required at BDWGC build time by the engine's typed-GC kinds (`BackingStore`/`ByteCodeBlock` disclaim-based finalization) -- also part of GCutil's own `GCUTIL_NOSYS_BAREMETAL` define set (§0). |
| `Globals::finalize()` must NOT be called | It calls `GC_gcollect_and_unmap()`, which does a `munmap()` -- bare-metal/RTOS targets have no such syscall and this hangs forever. Just don't call it (there is no real "process exit" on these targets anyway). |
| Board-owned heap-region audit | **RTOS-specific risk, not a bare-metal-specific one.** If the RTOS itself owns board-level heap-region initialization (e.g. NuttX's `arch/arm/src/mps/hardware/mps_memorymap.h` + `mps_allocateheap.c`), audit that the RTOS's own heap donation doesn't overlap wherever your custom linker script relocates Escargot's code/GC-heap. This exact collision was the NuttX port's long-standing crash root cause (see its report §7.1) -- a plain bare-metal target with no OS-owned board support code has no such layer and no such risk, but Zephyr/ThreadX-with-board-support-packages likely do. |

## 2. The Platform hook contract

Escargot already has an embedder abstraction for everything platform/OS
specific: `Escargot::PlatformRef` (public API, `src/api/EscargotPublic.h`)
and its internal mirror `Escargot::Platform` (`src/runtime/Platform.h`,
implemented by an internal `PlatformBridge` that forwards to your
`PlatformRef` subclass). A bare-metal/RTOS port implements a
`PlatformRef` subclass exactly like a hosted (Linux/Windows/Android)
embedder would -- the only difference is which methods are meaningful.

As of this guide, `OS_BAREMETAL` builds require **four additional
`PlatformRef` methods**, gated behind `#if defined(OS_BAREMETAL)` in both
`Platform.h` and `EscargotPublic.h` (so they add zero footprint to
hosted-platform builds):

```cpp
// The LOW end of the current task/thread's stack (the address closest to
// overflow). Consumed by ThreadLocal::initialize() to compute
// g_stackLimit (src/runtime/ThreadLocal.cpp).
virtual void* stackBase() = 0;

// The COLD end (HIGH address) of the current task/thread's stack -- where
// a full-descending stack starts from and grows down toward. Consumed by
// Heap::initialize() (src/heap/Heap.cpp), which hands it to BDWGC via
// GC_set_stackbottom() *before* GC_init() runs.
virtual void* stackTop() = 0;

// Monotonic millisecond tick -- Date.now()/clock_gettime() fallback when
// no RTC is available (src/runtime/DateObject.cpp).
virtual uint32_t tickMs() = 0;

// Timezone abbreviation name -- index 0 is the standard-time name, index
// 1 is the DST name (mirrors the standard libc ::tzname[0]/::tzname[1]
// array-of-two-strings convention). Consumed by VMInstance.cpp's non-ICU
// ensureTzname() fallback route (src/runtime/VMInstance.cpp) instead of
// calling ::tzset()/::tzname directly, since a bare-metal/RTOS libc may
// not provide either symbol at all. See §3 below for what a port without
// a real libc tzset should implement here.
//
// Named tzName(), NOT tzname() -- newlib's <time.h> (and some other
// libcs') `#define tzname _tzname` for MT-safety would otherwise mangle
// a same-named method inconsistently depending on <time.h> include
// order in a given translation unit (verified: this exact collision
// broke the FreeRTOS reference port's build with "marked 'override',
// but does not override"). Avoid the whole bug class by not colliding
// with the macro token in the first place.
virtual const char* tzName(int index) = 0;
```

**`stackBase()` and `stackTop()` must NOT be conflated.** They are
opposite ends of the same stack, needed by two different, unrelated
call sites for two different reasons. Merging them into one function (or
returning the wrong end from either) once broke script parsing entirely
on the NuttX port -- every script, even a bare `"1+2"`, failed with
`"too many recursion in script"`, because `ThreadLocal`'s recursion-depth
check got the high end where it needed the low end. See both ports'
reports, §7.3 (NuttX) -- this is the single easiest mistake to make when
porting a third RTOS, budget time for it.

Concretely:

- `stackBase()`: the address closest to stack overflow -- e.g. for a
  descending-stack ARM target, the *lowest* address in the allocated
  stack buffer.
- `stackTop()`: the address the stack pointer starts at when the task is
  first scheduled -- e.g. the *highest* address in the same buffer.

### `gcconfig.h` needs ZERO changes -- not even a generic hook

`third_party/GCutil/include/private/gcconfig.h` needs no per-RTOS changes
at all, for two independent reasons:

1. **`STACKBOTTOM` has a runtime override.** BDWGC already supports
   setting this at runtime (`GC_set_stackbottom()`, see
   `third_party/GCutil/include/gc/gc.h`) *before* `GC_init()` ever
   consults the compile-time `STACKBOTTOM` macro. `Heap::initialize()`
   (`src/heap/Heap.cpp`) does exactly that, using `stackTop()` above --
   so whatever `gcconfig.h`'s stock `STACKBOTTOM` macro resolves to is
   provably **dead code** for a port that goes through this path (verified:
   `GC_get_main_stack_base()`, the only function that reads it, has
   exactly one caller in the whole vendored bdwgc, `misc.c`, guarded by
   `if (NULL == GC_stackbottom)` -- which is never true once
   `Heap::initialize()` has run). Since it's dead code, gcconfig.h's stock
   NOSYS branch's existing `extern char __stack_base__[];` declaration can
   be satisfied by a symbol with **any** value -- your own platform glue
   just needs to *define* a symbol literally named `__stack_base__`
   somewhere (any content, e.g. `extern "C" char __stack_base__[1] = {0};`).
   No gcconfig.h edit needed -- the linker doesn't care which translation
   unit a symbol comes from.

2. **`DATASTART`/`DATAEND` genuinely are read at runtime**
   (`GC_register_data_segments()`, called unconditionally from
   `GC_init()`, no override exists for this one) -- BUT gcconfig.h's stock
   NOSYS branch already just wants two more symbols with the exact names
   `__data_start` and `end` (the latter via gcconfig.h's own *generic*
   `#ifndef DATAEND` fallback used by many platforms, not something
   NuttX-specific). If your port has real linker-script symbols with those
   names (like the FreeRTOS port), you're already done, no changes needed
   anywhere. If it doesn't (e.g. NuttX's app build has no such linker
   script), define stand-in symbols yourself -- **but see the correctness
   warning below before copying this.**

**Correctness warning, read before doing this:** `GC_register_data_segments()`
hard-`ABORT()`s at startup if `DATASTART > DATAEND`
(`os_dep.c`: `"Wrong DATASTART/END pair"`), and otherwise registers
*everything* in `[DATASTART, DATAEND)` as conservatively-scanned GC roots.
If your port genuinely keeps no GC-managed pointers in C/C++ static
storage duration variables (verified true for the NuttX port), the
*correct* stand-in is an **exactly zero-length** range -- not just "a
small range" -- and getting `DATASTART == DATAEND` exactly from two
independently-defined dummy symbols is NOT guaranteed by the C/C++
standard (the linker is free to place two separate objects anywhere
relative to each other). The safe way to guarantee true address equality
is a genuine linker alias, not two separate definitions:

```cpp
// One real definition...
extern "C" { int __data_start[1] = { 0 }; }
// ...and `end` (gcconfig.h's generic DATAEND fallback symbol) as a true
// alias of it -- guaranteed identical address, verified via `nm` on the
// compiled object (both resolve to the same offset in .bss).
extern "C" int end[1] __attribute__((alias("__data_start")));

// STACKBOTTOM's value is dead code (see point 1 above) -- any address is fine.
extern "C" char __stack_base__[1] = { 0 };
```

If your port DOES keep real GC-managed pointers in static storage, do NOT
use this zero-length trick -- point `__data_start`/`end` at your port's
real data+bss segment bounds instead (a normal linker script, like the
FreeRTOS port's, already does this correctly with zero extra code).

This was verified for real on the NuttX port: `gcconfig.h` is
byte-for-byte identical to its pre-porting-work state (confirmed via
`git diff` showing nothing), with these three dummy/aliased symbols added
to
`apps/interpreters/escargot/escargot_platform.cxx` instead, the port
still boots cleanly and passes all 15 exception regression cases plus
26/26 SunSpider 1.0.2 tests. `nm` on the final linked `nuttx` ELF confirms
`__data_start` and `end` resolve to the identical address.

**Net effect for a new RTOS port: `gcconfig.h` needs ZERO lines changed,
ever, for this.** Implement the four `PlatformRef` methods in this
section; if your build lacks real `__data_start`/`end`/`__stack_base__`
linker symbols, just define three small symbols with those exact names in
your own platform glue code (using the alias trick above for
`__data_start`/`end` if you need the zero-length-range guarantee) --
nothing in the shared, upstream-synced `third_party/GCutil` tree needs to
know your RTOS exists at all.

## 3. Reference skeleton (the ~5 things a new port implements)

```cpp
// my_rtos_platform.cpp -- adjust names/APIs to your RTOS

#include "EscargotPublic.h"

class MyRtosPlatform : public Escargot::PlatformRef {
public:
    // ... the usual embedder methods every PlatformRef needs
    // (markJSJobEnqueued, onLoadModule, didLoadModule,
    // hostImportModuleDynamically, ...) ...

    void* stackBase() override {
        // LOW end of the current task's stack.
        return my_rtos_current_task_stack_low_address();
    }

    void* stackTop() override {
        // HIGH/cold end of the current task's stack.
        return my_rtos_current_task_stack_high_address();
    }

    uint32_t tickMs() override {
        // Monotonic millisecond tick.
        return my_rtos_uptime_ms();
    }

    const char* tzName(int index) override {
        // Standard-time name (index 0) / DST name (index 1). If your
        // libc has a real, working ::tzset()/::tzname (verify this --
        // an actual undefined-reference link failure without it, don't
        // guess), just forward to it:
        //
        //   ::tzset();
        //   return ::tzname[index];
        //
        // If it doesn't, a UTC-only, no-DST-ever stub is a fine fallback
        // -- there is nothing to (re)compute, since it's a fixed constant:
        static const char* const utc[2] = { "UTC", "UTC" };
        return utc[index];
    }
};

static MyRtosPlatform g_platform;
Escargot::PlatformRef* escargot_platform() { return &g_platform; }

// Only needed if your build has no real __data_start/end/__stack_base__
// linker symbols -- see §2's "gcconfig.h needs ZERO changes" for the
// full explanation and correctness caveat.
extern "C" { int __data_start[1] = { 0 }; }
extern "C" int end[1] __attribute__((alias("__data_start")));
extern "C" char __stack_base__[1] = { 0 };

// sbrk(): raw pointer bump over a linker-reserved region. Same pattern on
// both existing ports -- see startup/startup_SSE300.c (FreeRTOS) or
// escargot_platform.cxx (NuttX) for the exact ~15-line implementation.
extern "C" void* sbrk(intptr_t increment) { /* ... */ }
```

Then at startup, before running any JS:

```cpp
Escargot::Globals::initialize(escargot_platform());
// ... create context, run scripts ...
// Do NOT call Escargot::Globals::finalize() -- see checklist above.
```

Build-time definitions every port needs. The engine-side ones
(`-DOS_BAREMETAL=1` and friends) come for free from
`-DESCARGOT_HOST=baremetal` (§0); the BDWGC ones come for free too once you
set `GCUTIL_NOSYS_BAREMETAL=ON` (§0, as a cache variable before your
top-level `add_subdirectory(<escargot repo root>)` call) -- `-DNOSYS`,
`-DGC_ATOMIC_UNCOLLECTABLE`, `-DENABLE_DISCLAIM` and the rest of that
mode's define set are baked into GCutil's own CMakeLists.txt, not
something your own project's CMakeLists.txt needs to hand-copy. The
one thing that IS still your own project's responsibility, if your libc
needs it, is remapping BDWGC's `_setjmp`/`_longjmp` calls via
`GCUTIL_CFLAGS_FROM_EXTERNAL` (NuttX's libc needs this; newlib, used by
the FreeRTOS sample, doesn't):

```
# GCUTIL_CFLAGS_FROM_EXTERNAL, your own project -- see §0 (only if your
# libc lacks newlib's _setjmp/_longjmp aliases)
-D_setjmp=setjmp -D_longjmp=longjmp
```

## 4. Shared code you get for free

- **`OSAllocator`** (yarr's `BumpPointerAllocator` backing store):
  `third_party/yarr/OSAllocatorBaremetal.cpp` is a generic
  `malloc()`/`free()`-backed implementation, self-guarded behind
  `OS_BAREMETAL` and picked up automatically by the engine's normal
  source globs, shared by both existing reference ports instead of being
  duplicated per-port -- **a new port does not need to write this file at
  all.**
- **`sbrk()` bump-pointer logic**: `src/heap/SbrkBaremetal.cpp`'s
  `escargotBaremetalSbrk()` -- see §1's "Heap supply" row. A new port
  provides its own linker symbols plus a 1-line forwarder, not the bump
  logic itself. (Lives under `src/heap/`, not `third_party/yarr/`, since
  it feeds BDWGC's heap growth via `Heap::initialize()`/`GC_init()` and
  has nothing to do with yarr's regex engine -- picked up automatically
  by every build's own recursive `src/*.cpp` glob, no build-file changes
  needed regardless of which location it lives in.)
- **`tzName()`**: unlike the two above, there is no shared fallback file
  for this one -- each port implements `PlatformRef::tzName()` directly
  (see §3's reference skeleton for a real-libc forward vs. a UTC-only
  stub). Timezone-name lookup is small and RTOS-specific enough (whether
  your libc has a real `::tzset()`/`::tzname` at all is itself a
  per-libc-config question, e.g. NuttX's `CONFIG_LIBC_LOCALTIME`/
  `CONFIG_LIBC_LOCALE`) that a shared file didn't pull its weight the way
  the sbrk/OSAllocator bump logic does -- an earlier version of this
  contract instead used a separate `ESCARGOT_NEED_TZSET_FALLBACK`
  opt-in macro and a shared, weak-symbol-based
  `third_party/yarr/TzsetBaremetal.cpp`; that approach is gone; routing
  through `PlatformRef::tzName()` like the other three hooks removes the
  entire class of fragility it had (see the git history of that file for
  the full story: a real NuttX `tzset()`/`tzname` got silently shadowed
  by the weak fallback due to static-archive symbol-resolution order).
