# This project lives at samples/rtos/nuttx/escargot-lib-cmake/ inside the
# main Escargot repo. It builds libescargot.a/libgc-lib.a for NuttX as an
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
#
# Instead of hand-duplicating the engine's own source-file globs/defines
# here, reuse the main repo's own top-level CMakeLists.txt (build/config.cmake
# + build/escargot.cmake) as a subdirectory, with ESCARGOT_HOST=baremetal
# (see build/target.cmake) doing the ENGINE-side (compile defs/flags/-std
# override) work. That top-level project deliberately does NOT build
# runtime_icu_binder for ESCARGOT_HOST=baremetal (see the guards in
# build/escargot.cmake) -- it unconditionally needs <dlfcn.h>, which doesn't
# exist on this newlib/nosys target, so it stays out of scope entirely.
# BDWGC (third_party/GCutil) IS built as part of this same add_subdirectory()
# -- its own CMakeLists.txt has a NOSYS/bare-metal mode
# (GCUTIL_NOSYS_BAREMETAL) that build/escargot.cmake add_subdirectory()s
# unconditionally; this port only needs to set the GCUTIL_* cache variables
# below before calling add_subdirectory() so GCutil's own CMakeLists.txt
# picks them up.
set(ESCARGOT_ROOT  ${CMAKE_SOURCE_DIR}/../../../..)
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
# NOSYS source list + define set over its normal hosted ones.
# GCUTIL_CFLAGS_FROM_EXTERNAL supplies this port's own extra requirement:
# NuttX's libc doesn't provide the underscore-prefixed _setjmp/_longjmp
# aliases newlib does, and mach_dep.c calls them directly, so remap them
# here rather than baking a NuttX-specific remap into GCutil's own shared
# NOSYS define set. All three must be set BEFORE add_subdirectory() below,
# since that's what actually pulls in third_party/GCutil (build/escargot.cmake
# add_subdirectory()s it as part of the same call, no separate one needed
# here). Do NOT set GCUTIL_ENABLE_THREADING -- no thread runtime on this
# target.
#
# Note: this produces a target named "gc-lib" (GCutil's own, fixed target
# name), not the "gc" this project used to hand-roll itself -- see
# ../interpreters-escargot/Makefile (libgc-lib.a, not libgc.a) and install()
# below.
set(GCUTIL_NOSYS_BAREMETAL ON CACHE BOOL "" FORCE)
set(GCUTIL_INITIAL_HEAP_SIZE "(1*1024*1024)" CACHE STRING "" FORCE)
set(GCUTIL_CFLAGS_FROM_EXTERNAL -D_setjmp=setjmp -D_longjmp=longjmp CACHE STRING "" FORCE)

# Force ALL static-library outputs -- the engine subdirectory's own nested
# libescargot.a, and GCutil's own nested libgc-lib.a -- to land flatly at
# this project's own build-directory root instead of under their respective
# add_subdirectory() binary dirs. ../interpreters-escargot/Makefile's
# `context::` rule (the external NuttX-side consumer of this project's
# output) copies both by exactly that flat path
# ($(ESCARGOT_PREBUILT)/libescargot.a, $(ESCARGOT_PREBUILT)/libgc-lib.a) into
# NuttX's apps/staging/, since it's a separate `make` invocation over
# prebuilt archives, not a CMake target_link_libraries() consumer that could
# resolve a nested path on its own.
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

add_subdirectory(${ESCARGOT_ROOT} ${CMAKE_BINARY_DIR}/escargot-engine)

# Reuse the exact same preprocessor defines the engine itself was compiled
# with for any translation unit here that includes EscargotPublic.h (e.g.
# OS_BAREMETAL, ESCARGOT_EXPORT=) instead of hand-copying ESCARGOT_DEFS again.
get_target_property(ESCARGOT_ENGINE_DEFS escargot COMPILE_DEFINITIONS)

install(TARGETS escargot gc-lib ARCHIVE DESTINATION lib)
