cmake_minimum_required(VERSION 3.18)

project(escargot-rtos C CXX ASM)

set(CMAKE_EXE_LINKER_FLAGS
    "-T ${CMAKE_SOURCE_DIR}/startup/an552_ns.ld \
    -Wl,--gc-sections -Wl,-Map=${CMAKE_BINARY_DIR}/escargot-rtos.map \
    --specs=nosys.specs"
    CACHE STRING "" FORCE)

# ─── FreeRTOS ────────────────────────────────────────────────────────────────
set(FREERTOS_DIR ${CMAKE_SOURCE_DIR}/FreeRTOS-Kernel)
set(FREERTOS_PORT_DIR ${FREERTOS_DIR}/portable/GCC/ARM_CM4F)

add_library(freertos STATIC
    ${FREERTOS_DIR}/tasks.c
    ${FREERTOS_DIR}/queue.c
    ${FREERTOS_DIR}/list.c
    ${FREERTOS_DIR}/timers.c
    ${FREERTOS_DIR}/portable/MemMang/heap_3.c
    ${FREERTOS_PORT_DIR}/port.c
)
target_include_directories(freertos PUBLIC
    ${FREERTOS_DIR}/include
    ${FREERTOS_PORT_DIR}
    ${CMAKE_SOURCE_DIR}
)

# ─── Escargot ────────────────────────────────────────────────────────────────
# This sample lives at samples/rtos/freertos inside the main Escargot repo,
# so the engine root is three directories up. Instead of hand-duplicating the
# engine's own source-file globs/defines here, reuse the main repo's own
# top-level CMakeLists.txt (build/config.cmake + build/escargot.cmake) as a
# subdirectory, with ESCARGOT_HOST=baremetal (see build/target.cmake) doing
# the ENGINE-side (compile defs/flags/-std override) work. That top-level
# project deliberately does NOT build runtime_icu_binder for
# ESCARGOT_HOST=baremetal (see the guards in build/escargot.cmake) --
# runtime_icu_binder unconditionally needs <dlfcn.h>, which doesn't exist on
# this newlib/nosys target, so that one stays out of scope entirely. BDWGC
# (third_party/GCutil) IS built as part of this same add_subdirectory() --
# its own CMakeLists.txt has a NOSYS/bare-metal mode (GCUTIL_NOSYS_BAREMETAL)
# that build/escargot.cmake add_subdirectory()s unconditionally; this port
# only needs to set the GCUTIL_* cache variables below before calling
# add_subdirectory() so GCutil's own CMakeLists.txt picks them up.
set(ESCARGOT_ROOT  ${CMAKE_SOURCE_DIR}/../../..)
set(THIRD          ${ESCARGOT_ROOT}/third_party)

set(ESCARGOT_HOST   baremetal CACHE STRING "" FORCE)
set(ESCARGOT_ARCH   arm       CACHE STRING "" FORCE)
set(ESCARGOT_OUTPUT static_lib CACHE STRING "" FORCE)
set(ESCARGOT_MODE   release   CACHE STRING "" FORCE)
set(ESCARGOT_SMALL_CONFIG ON  CACHE BOOL   "" FORCE)
# Extra bare-metal-only tuning the main build doesn't model as first-class
# options: this port's ~128KB task stack (default STACK_USAGE_LIMIT is sized
# for a hosted OS thread, far too large here) and abort-on-OOM instead of the
# default throw (no exception-safe recovery story for GC allocation failure
# on this target).
set(ESCARGOT_CXXFLAGS_FROM_EXTERNAL
    -DGC_NEW_ABORTS_ON_OOM
    -DSTACK_FREESPACE_FROM_LIMIT=4096
    -DSTACK_USAGE_LIMIT=131072
    CACHE STRING "" FORCE)

# BDWGC (bare-metal NOSYS mode): GCUTIL_NOSYS_BAREMETAL picks GCutil's own
# NOSYS source list + define set over its normal hosted (Linux/Darwin/
# Windows/Android/Tizen) ones; GCUTIL_INITIAL_HEAP_SIZE is this port's own
# tuning knob (large initial heap: SMALL_CONFIG's default 16 KB causes
# hundreds of GC collections during built-in initialization on the ~1 MIPS
# FVP simulator -- 4 MB covers all of initializeBuiltins with at most a
# handful of collections). Both must be set BEFORE add_subdirectory() below,
# since that's what actually pulls in third_party/GCutil (build/escargot.cmake
# add_subdirectory()s it as part of the same call, no separate one needed
# here). Do NOT set GCUTIL_ENABLE_THREADING -- no thread runtime on this
# target.
set(GCUTIL_NOSYS_BAREMETAL ON CACHE BOOL "" FORCE)
set(GCUTIL_INITIAL_HEAP_SIZE "(4*1024*1024)" CACHE STRING "" FORCE)

add_subdirectory(${ESCARGOT_ROOT} ${CMAKE_BINARY_DIR}/escargot-engine)

# Reuse the exact same preprocessor defines the engine itself was compiled
# with for any translation unit here that includes EscargotPublic.h (e.g.
# OS_BAREMETAL, ESCARGOT_EXPORT=) instead of hand-copying ESCARGOT_DEFS again.
get_target_property(ESCARGOT_ENGINE_DEFS escargot COMPILE_DEFINITIONS)

# ─── Main application ────────────────────────────────────────────────────────
# OSAllocator implementation comes from third_party/yarr/OSAllocatorBaremetal.cpp
# (picked up automatically by the reused "escargot" engine target's own
# source list, self-guarded behind OS_BAREMETAL) -- no port-local copy needed
# here anymore.
add_executable(escargot-rtos
    startup/startup_SSE300.c
    src/main.cpp
    src/escargot_platform.cpp
)
target_include_directories(escargot-rtos PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${THIRD}/GCutil/include
)
target_compile_definitions(escargot-rtos PRIVATE ${ESCARGOT_ENGINE_DEFS})
target_compile_options(escargot-rtos PRIVATE -fexceptions)
target_link_libraries(escargot-rtos escargot gc-lib freertos -lc -lstdc++)

set_target_properties(escargot-rtos PROPERTIES SUFFIX ".elf")
