# This project lives at samples/rtos/nuttx/escargot-lib-cmake/ inside the
# main Escargot repo. It builds libescargot.a/libgc.a for NuttX as an
# out-of-tree CMake project (deliberately NOT folded into NuttX's own Make
# build -- see apps/interpreters/escargot/Makefile's ESCARGOT_PREBUILT,
# ../interpreters-escargot/Makefile in this same samples dir, and
# docs/porting/RTOS_PORTING_GUIDE.md for why: it reuses Escargot's own
# CMake infrastructure instead of re-expressing the whole engine as NuttX
# Make rules). NuttX itself is NOT vendored here -- point NUTTX_ROOT at
# your own NuttX kernel checkout (the "nuttx" directory, sibling of
# "apps"); there is no portable default.
cmake_minimum_required(VERSION 3.18)

if(NOT DEFINED NUTTX_ROOT OR NUTTX_ROOT STREQUAL "")
    message(FATAL_ERROR "NUTTX_ROOT is not set -- point it at your own NuttX "
                         "kernel checkout, e.g. "
                         "-DNUTTX_ROOT=/path/to/nuttx-workspace/nuttx")
endif()

# Cross-compiler/target must be set BEFORE project() -- that's what runs
# CMake's compiler detection, so setting these after it (as an earlier
# revision of this file did) silently falls back to the host compiler in
# a fresh build directory with no prior cache (only appeared to work
# before because a long-lived build directory's cached
# CMAKE_C_COMPILER/CMAKE_CXX_COMPILER from an earlier, differently-ordered
# configure masked it).
set(CMAKE_C_COMPILER   arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

project(escargot-nuttx-lib C CXX)

# Flags mirrored from NuttX's boards/arm/mps/mps3-an547:nsh Make.defs
# (CONFIG_ARCH_CORTEXM55 + CONFIG_ARM_HAVE_MVE + CONFIG_ARCH_FPU, hard-float ABI)
# NOTE: deliberately omitting +mve.fp from -march -- an experiment (from
# this project's original bring-up, tracked down to an unrelated cause)
# that was never reverted; likely fine to add back, not reverified.
set(ARCH_FLAGS
    -mthumb
    -mtune=cortex-m55
    -march=armv8.1-m.main+fp.dp
    -mfpu=fpv5-d16
    -mfloat-abi=hard
)

set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -isystem ${NUTTX_ROOT}/include -Os -fno-strict-aliasing -fomit-frame-pointer -fno-common -Wno-psabi")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -isystem ${NUTTX_ROOT}/include -Os -fno-strict-aliasing -fomit-frame-pointer -fno-common -Wno-psabi -std=gnu++17")
# No ESCARGOT_RTOS_PORT / gcconfig.h changes needed at all -- this port's
# own escargot_platform.cxx defines dummy __data_start/end/__stack_base__
# symbols directly (see that file), which is all gcconfig.h's stock NOSYS
# branch (unmodified) needs to link.

string(REPLACE ";" " " ARCH_FLAGS_STR "${ARCH_FLAGS}")
set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} ${ARCH_FLAGS_STR}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${ARCH_FLAGS_STR}")

# ─── Escargot ────────────────────────────────────────────────────────────────
# This CMakeLists.txt lives at samples/rtos/nuttx/escargot-lib-cmake inside
# the main Escargot repo -- one directory deeper than
# samples/rtos/freertos/CMakeLists.txt, so the engine root is four
# directories up here (that file's own ESCARGOT_ROOT uses ../../../ from
# one level shallower).
set(ESCARGOT_ROOT  ${CMAKE_SOURCE_DIR}/../../../..)
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

file(GLOB_RECURSE ESCARGOT_CPPSRC ${ESCARGOT_SRC}/*.cpp)
list(FILTER ESCARGOT_CPPSRC EXCLUDE REGEX ".*/shell/.*")
list(FILTER ESCARGOT_CPPSRC EXCLUDE REGEX ".*/wasm/.*")
list(FILTER ESCARGOT_CPPSRC EXCLUDE REGEX ".*/debugger/.*")
list(FILTER ESCARGOT_CPPSRC EXCLUDE REGEX ".*/codecache/.*")
list(FILTER ESCARGOT_CPPSRC EXCLUDE REGEX ".*/intl/.*")
list(APPEND ESCARGOT_CPPSRC ${ESCARGOT_SRC}/wasm/BuiltinWASM.cpp)

file(GLOB YARR_SRC ${THIRD}/yarr/*.cpp)
list(FILTER YARR_SRC EXCLUDE REGEX ".*OSAllocatorPosix.*")
list(FILTER YARR_SRC EXCLUDE REGEX ".*OSAllocatorWin.*")

set(YARR_GENERATED_DIR ${ESCARGOT_ROOT}/build/escargot_generated/yarr)
list(APPEND YARR_SRC
    ${YARR_GENERATED_DIR}/YarrCanonicalizeUCS2.cpp
    ${YARR_GENERATED_DIR}/YarrCanonicalizeUnicode.cpp
)

set(SIMDUTF_SRC ${THIRD}/simdutf/simdutf.cpp)
file(GLOB DC_SRC ${THIRD}/double_conversion/*.cc)
file(GLOB LZ4_SRC ${THIRD}/lz4/*.cpp)
set(LIBBF_SRC ${THIRD}/libbf/libbf.c ${THIRD}/libbf/cutils.c)
set(XSUM_SRC ${THIRD}/xsum/xsum.cpp)

set(PARSER_GENERATED_DIR ${ESCARGOT_ROOT}/build/escargot_generated/parser)
set(UNICODE_TABLES_SRC ${PARSER_GENERATED_DIR}/UnicodeIdentifierTables.cpp)

add_library(escargot STATIC
    ${ESCARGOT_CPPSRC}
    ${YARR_SRC}
    ${SIMDUTF_SRC}
    ${DC_SRC}
    ${LZ4_SRC}
    ${LIBBF_SRC}
    ${XSUM_SRC}
    ${UNICODE_TABLES_SRC}
)
target_compile_definitions(escargot PRIVATE ${ESCARGOT_DEFS})
target_include_directories(escargot PUBLIC ${ESCARGOT_INCS})
target_compile_options(escargot PRIVATE -fexceptions)

# ─── BDWGC (bare-metal NOSYS mode) ──────────────────────────────────────────
set(GCUTIL_SRC
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
    ${THIRD}/GCutil/headers.c
    ${THIRD}/GCutil/LeakChecker.cpp
    ${THIRD}/GCutil/mach_dep.c
    ${THIRD}/GCutil/malloc.c
    ${THIRD}/GCutil/mallocx.c
    ${THIRD}/GCutil/mark.c
    ${THIRD}/GCutil/mark_rts.c
    ${THIRD}/GCutil/misc.c
    ${THIRD}/GCutil/new_hblk.c
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
    "-DGC_INITIAL_HEAP_SIZE=(1*1024*1024)"
    -DGC_ATOMIC_UNCOLLECTABLE
    -DENABLE_DISCLAIM
    -D_setjmp=setjmp
    -D_longjmp=longjmp
)

add_library(gc STATIC ${GCUTIL_SRC})
target_compile_definitions(gc PRIVATE ${GCUTIL_DEFS} ${ESCARGOT_DEFS})
target_include_directories(gc PUBLIC
    ${THIRD}/GCutil/include
    ${THIRD}/GCutil/include/private
)
target_compile_options(gc PRIVATE -w)

install(TARGETS escargot gc ARCHIVE DESTINATION lib)
