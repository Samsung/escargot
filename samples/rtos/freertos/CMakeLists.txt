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
# so the engine root is three directories up.
set(ESCARGOT_ROOT  ${CMAKE_SOURCE_DIR}/../../..)
set(ESCARGOT_SRC   ${ESCARGOT_ROOT}/src)
set(THIRD          ${ESCARGOT_ROOT}/third_party)

set(ESCARGOT_DEFS
    -DOS_BAREMETAL=1
    -DESCARGOT_SMALL_CONFIG
    -DNDEBUG
    -DGC_NEW_ABORTS_ON_OOM
    -DENABLE_COMPRESSIBLE_STRING
    -DENABLE_RELOADABLE_STRING
    -DESCARGOT
    -DESCARGOT_32=1
    -DSTACK_FREESPACE_FROM_LIMIT=4096
    -DSTACK_USAGE_LIMIT=131072
)

set(ESCARGOT_INCS
    ${ESCARGOT_SRC}
    ${ESCARGOT_ROOT}/build/escargot_generated
    ${THIRD}
    ${THIRD}/GCutil/include
    ${THIRD}/libbf
    ${THIRD}/robin_map/include
    ${THIRD}/yarr
    ${THIRD}/double_conversion
    ${THIRD}/rapidjson/include
    ${THIRD}/runtime_icu_binder
    ${THIRD}/xsum
    ${THIRD}/simdutf
    ${THIRD}/checked_arithmetic
    ${THIRD}/lz4
)

# Collect Escargot sources (exclude shell, wasm, debugger, codecache, intl)
file(GLOB_RECURSE ESCARGOT_CPPSRC ${ESCARGOT_SRC}/*.cpp)
list(FILTER ESCARGOT_CPPSRC EXCLUDE REGEX ".*/shell/.*")
list(FILTER ESCARGOT_CPPSRC EXCLUDE REGEX ".*/wasm/.*")
list(FILTER ESCARGOT_CPPSRC EXCLUDE REGEX ".*/debugger/.*")
list(FILTER ESCARGOT_CPPSRC EXCLUDE REGEX ".*/codecache/.*")
list(FILTER ESCARGOT_CPPSRC EXCLUDE REGEX ".*/intl/.*")
# BuiltinWASM.cpp provides a no-op initializeWebAssembly stub when ENABLE_WASM
# is not defined — required because GlobalObject always declares this method.
list(APPEND ESCARGOT_CPPSRC ${ESCARGOT_SRC}/wasm/BuiltinWASM.cpp)

# Yarr (exclude platform allocators — we provide our own stub)
file(GLOB YARR_SRC ${THIRD}/yarr/*.cpp)
list(FILTER YARR_SRC EXCLUDE REGEX ".*OSAllocatorPosix.*")
list(FILTER YARR_SRC EXCLUDE REGEX ".*OSAllocatorWin.*")

# Generated canonicalization tables (built once during normal host build)
set(YARR_GENERATED_DIR ${ESCARGOT_ROOT}/build/escargot_generated/yarr)
list(APPEND YARR_SRC
    ${YARR_GENERATED_DIR}/YarrCanonicalizeUCS2.cpp
    ${YARR_GENERATED_DIR}/YarrCanonicalizeUnicode.cpp
)

# simdutf (single-file build)
set(SIMDUTF_SRC ${THIRD}/simdutf/simdutf.cpp)

# double_conversion
file(GLOB DC_SRC ${THIRD}/double_conversion/*.cc)

# lz4
file(GLOB LZ4_SRC ${THIRD}/lz4/*.cpp)

# libbf (C sources — libbf.c uses dbuf_* from cutils.c)
set(LIBBF_SRC
    ${THIRD}/libbf/libbf.c
    ${THIRD}/libbf/cutils.c
)

# xsum (Math.sumPrecise)
set(XSUM_SRC ${THIRD}/xsum/xsum.cpp)

# Generated Unicode identifier tables (built once during normal host build)
set(PARSER_GENERATED_DIR ${ESCARGOT_ROOT}/build/escargot_generated/parser)
set(UNICODE_TABLES_SRC ${PARSER_GENERATED_DIR}/UnicodeIdentifierTables.cpp)

add_library(escargot-lib STATIC
    ${ESCARGOT_CPPSRC}
    ${YARR_SRC}
    ${SIMDUTF_SRC}
    ${DC_SRC}
    ${LZ4_SRC}
    ${LIBBF_SRC}
    ${XSUM_SRC}
    ${UNICODE_TABLES_SRC}
)
target_compile_definitions(escargot-lib PRIVATE ${ESCARGOT_DEFS})
target_include_directories(escargot-lib PUBLIC ${ESCARGOT_INCS})
# Escargot requires exceptions
target_compile_options(escargot-lib PRIVATE -fexceptions)

# ─── BDWGC (bare-metal NOSYS mode) ──────────────────────────────────────────
# GCutil bundles a modified BDWGC; build the subset needed for NOSYS ARM32
file(GLOB GCUTIL_SRC
    ${THIRD}/GCutil/allchblk.c
    ${THIRD}/GCutil/Allocator.cpp
    ${THIRD}/GCutil/alloc.c
    ${THIRD}/GCutil/backgraph.c
    ${THIRD}/GCutil/blacklst.c
    ${THIRD}/GCutil/checksums.c
    ${THIRD}/GCutil/dbg_mlc.c
    ${THIRD}/GCutil/dyn_load.c
    ${THIRD}/GCutil/finalize.c
    ${THIRD}/GCutil/fnlz_mlc.c
    ${THIRD}/GCutil/gc_badalc.cpp
    ${THIRD}/GCutil/gc_cpp.cpp
    ${THIRD}/GCutil/headers.c
    ${THIRD}/GCutil/LeakChecker.cpp
    ${THIRD}/GCutil/mach_dep.c
    ${THIRD}/GCutil/malloc.c
    ${THIRD}/GCutil/mallocx.c
    ${THIRD}/GCutil/mark.c
    ${THIRD}/GCutil/mark_rts.c
    ${THIRD}/GCutil/misc.c
    ${THIRD}/GCutil/new_hblk.c
    ${THIRD}/GCutil/obj_map.c
    ${THIRD}/GCutil/os_dep.c
    ${THIRD}/GCutil/ptr_chck.c
    ${THIRD}/GCutil/reclaim.c
    ${THIRD}/GCutil/typd_mlc.c
)

set(GCUTIL_DEFS
    -DNOSYS
    -DSMALL_CONFIG
    -DGC_NOT_DLL
    -DGC_NO_SIGHANDLER
    -DNO_GETENV
    -DNO_CLOCK
    -DGC_NEW_ABORTS_ON_OOM
    -DGC_DISABLE_INCREMENTAL
    -DIGNORE_DYNAMIC_LOADING
    -DDONT_USE_MMAP
    -DGC_NO_DLOPEN
    # Large initial heap: SMALL_CONFIG default is 16 KB which causes hundreds of
    # GC collections during built-in initialization on the ~1 MIPS FVP simulator.
    # 4 MB covers all of initializeBuiltins with at most a handful of collections.
    "-DGC_INITIAL_HEAP_SIZE=(4*1024*1024)"
    # Do NOT define GC_THREADS — would auto-enable GC_PTHREADS
    -DGC_ATOMIC_UNCOLLECTABLE
    # Required by newer typed-GC kinds (BackingStore/ByteCodeBlock disclaim-based
    # finalization) — matches third_party/GCutil/CMakeLists.txt upstream flags.
    -DENABLE_DISCLAIM
)

add_library(gc-lib STATIC ${GCUTIL_SRC})
target_compile_definitions(gc-lib PRIVATE ${GCUTIL_DEFS} ${ESCARGOT_DEFS})
target_include_directories(gc-lib PUBLIC
    ${THIRD}/GCutil/include
    ${THIRD}/GCutil/include/private
)
# Allow C89-style in GC sources
target_compile_options(gc-lib PRIVATE -w)

# ─── Main application ────────────────────────────────────────────────────────
# OSAllocator implementation comes from third_party/yarr/OSAllocatorBaremetal.cpp
# (picked up automatically by escargot-lib's YARR_SRC glob above, self-guarded
# behind OS_BAREMETAL) -- no port-local copy needed here anymore.
add_executable(escargot-rtos
    startup/startup_SSE300.c
    src/main.cpp
    src/escargot_platform.cpp
)
target_include_directories(escargot-rtos PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${ESCARGOT_SRC}/api
    ${ESCARGOT_INCS}
)
target_compile_definitions(escargot-rtos PRIVATE ${ESCARGOT_DEFS})
target_compile_options(escargot-rtos PRIVATE -fexceptions)
target_link_libraries(escargot-rtos escargot-lib gc-lib freertos -lc -lstdc++)

set_target_properties(escargot-rtos PROPERTIES SUFFIX ".elf")
