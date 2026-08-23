# <max> (the "...4.0" part) keeps this buildable on CMake 4.x: CMake 4.0
# hard-errors when a project's effective policy version is below 3.5 (only
# <min> below 2.8.12 was deprecated-but-tolerated before that). CMake older
# than 3.12 doesn't understand the "..." range syntax at all and just sees
# extra version-component dots, so it parses this as <min>=2.8.12 and
# ignores the rest -- see `cmake --help-command cmake_minimum_required`.
CMAKE_MINIMUM_REQUIRED (VERSION 2.8.12...4.0 FATAL_ERROR)

PROJECT (ESCARGOT)

# NOTE: many ESCARGOT_* options below are declared via option(VAR "help" ${VAR})
# after a normal variable VAR already holds a computed default (host/arch/
# threading-dependent defaults, legacy-variable remaps, etc). This intentionally
# relies on CMP0077's OLD behavior, where option() still creates/updates the
# CACHE entry (so the option shows up in `cmake -L`/cmake-gui with its
# help text) even though the pre-existing normal variable keeps shadowing it
# for reads in this scope. Setting CMP0077=NEW would make option() a no-op
# whenever a normal variable already exists, silently dropping the CACHE
# entry entirely (tested and confirmed: ENABLE_SHELL/BUILD_SHARED_LIBS/
# TLS_ACCESS_BY_ADDRESS vanish from `cmake -L`) -- do NOT set that policy here.
# The resulting "CMP0077 ambiguity" dev warnings on every such option() call
# are expected noise from this pattern, not a real problem.

# CONFIGURATION
SET (CMAKE_VERBOSE_MAKEFILE true)
SET (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

MESSAGE(STATUS "CMAKE_SYSTEM_NAME: " ${CMAKE_SYSTEM_NAME})
MESSAGE(STATUS "CMAKE_SYSTEM_PROCESSOR: " ${CMAKE_SYSTEM_PROCESSOR})

# If compiling for Intel x86 32-bit but CMake host/environment hides it under x86_64 (e.g. CI multilib build with -m32),
# forcefully override CMAKE_SYSTEM_PROCESSOR to i386 to unify the whole project tree under 32-bit target.
IF ("${CMAKE_C_FLAGS}" MATCHES "-m32" OR "${CMAKE_CXX_FLAGS}" MATCHES "-m32" OR
    "$ENV{CFLAGS}" MATCHES "-m32" OR "$ENV{CXXFLAGS}" MATCHES "-m32" OR
    "${LDFLAGS_FROM_ENV}" MATCHES "-m32" OR "$ENV{LDFLAGS}" MATCHES "-m32")
    IF ("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "x86_64" OR "${CMAKE_SYSTEM_PROCESSOR}" MATCHES "AMD64" OR "${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "")
        SET(CMAKE_SYSTEM_PROCESSOR "i386" CACHE STRING "Forced 32-bit processor" FORCE)
        # Re-print synchronized processor name
        MESSAGE(STATUS "CMAKE_SYSTEM_PROCESSOR (Overridden to 32-bit): " ${CMAKE_SYSTEM_PROCESSOR})
    ENDIF()
ENDIF()

IF (NOT DEFINED ESCARGOT_ARCH)
    IF ("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "X86" OR "${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86"
        OR "${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "i386" OR "${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "i686")
        SET(ESCARGOT_ARCH "x86")
    ELSEIF ("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "AMD64" OR "${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64"
            OR "${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x64" OR "${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "EM64T")
        SET(ESCARGOT_ARCH "x64")
    ELSEIF ("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "arm" OR "${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "arm32"
            OR "${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "armv7l")
        SET(ESCARGOT_ARCH "arm")
    ELSEIF ("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "arm64" OR "${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "ARM64"
            OR "${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "aarch64" OR "${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "AARCH64")
        SET(ESCARGOT_ARCH "aarch64")
    ELSEIF ("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "riscv64")
        SET(ESCARGOT_ARCH "riscv64")
    ENDIF()
    IF (NOT ESCARGOT_ARCH)
        SET(ESCARGOT_ARCH "x64") # Fallback to standard x64 if processor is unknown or empty
    ENDIF()
ENDIF()



IF (CMAKE_SYSTEM_NAME STREQUAL "Generic" OR
    CMAKE_SYSTEM_NAME STREQUAL "BareMetal" OR
    CMAKE_SYSTEM_NAME STREQUAL "FreeRTOS" OR
    CMAKE_SYSTEM_NAME STREQUAL "NuttX" OR
    CMAKE_SYSTEM_NAME STREQUAL "Zephyr" OR
    CMAKE_SYSTEM_NAME STREQUAL "RIOT" OR
    CMAKE_SYSTEM_NAME STREQUAL "MbedOS" OR
    CMAKE_SYSTEM_NAME STREQUAL "Mynewt" OR
    CMAKE_SYSTEM_NAME STREQUAL "Emscripten" OR
    CMAKE_SYSTEM_NAME STREQUAL "WASI" OR
    CMAKE_C_COMPILER_TARGET MATCHES "-none-eabi$" OR
    CMAKE_C_COMPILER_TARGET MATCHES "-elf$")
    SET(ESCARGOT_IS_BAREMETAL ON)
ELSE()
    SET(ESCARGOT_IS_BAREMETAL OFF)
ENDIF()

# Map CMAKE_SYSTEM_NAME to internal ESCARGOT_HOST for backwards compatibility
# with legacy build scripts and internal configuration files.
IF (NOT DEFINED ESCARGOT_HOST)
    IF (CMAKE_SYSTEM_NAME STREQUAL "Windows" OR CMAKE_SYSTEM_NAME STREQUAL "WindowsStore")
        SET(ESCARGOT_HOST "windows")
    ELSEIF (CMAKE_SYSTEM_NAME STREQUAL "Android")
        SET(ESCARGOT_HOST "android")
    ELSEIF (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        SET(ESCARGOT_HOST "darwin")
    ELSEIF (ESCARGOT_IS_BAREMETAL)
        SET(ESCARGOT_HOST "baremetal")
    ELSEIF (CMAKE_SYSTEM_NAME STREQUAL "Tizen")
        SET(ESCARGOT_HOST "tizen")
    ELSE()
        SET(ESCARGOT_HOST "linux")
    ENDIF()
ENDIF()

# Map CMAKE_BUILD_TYPE to the internal legacy ESCARGOT_MODE variable, to avoid
# breaking internal configuration scripts that still key off it (e.g.
# ESCARGOT_TCO_DEBUG's debug-only check below). ESCARGOT_OUTPUT itself no
# longer exists -- it was split into the independent ESCARGOT_ENABLE_SHELL /
# ESCARGOT_BUILD_SHARED_LIBS / ESCARGOT_BUILD_CCTEST booleans below.
#
# On multi-config generators (Visual Studio, Xcode, Ninja Multi-Config),
# CMAKE_BUILD_TYPE is always empty at configure time -- the config is only
# selected at build time via --config -- so ESCARGOT_MODE always resolves to
# "release" there regardless of what will actually be built. Record whether
# we're on such a generator so configure-time-only consumers of ESCARGOT_MODE
# (see ESCARGOT_TCO_DEBUG below) can skip checks they can't correctly perform.
GET_PROPERTY (ESCARGOT_IS_MULTI_CONFIG_GENERATOR GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)

IF (NOT DEFINED ESCARGOT_MODE)
    STRING(TOLOWER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE_LOWER)
    IF (CMAKE_BUILD_TYPE_LOWER MATCHES "debug")
        SET (ESCARGOT_MODE "debug")
    ELSE()
        SET (ESCARGOT_MODE "release")
    ENDIF()
ENDIF()

# Bare-metal/RTOS ports (see docs/porting/RTOS_PORTING_GUIDE.md) have no
# thread runtime and no ICU on the target -- default both off unless the
# caller explicitly overrides. Must run before the generic ESCARGOT_THREADING
# default below, and before build/config.cmake's ESCARGOT_LIBICU_SUPPORT
# default, so those generic "ON" defaults don't clobber this first.
IF (ESCARGOT_IS_BAREMETAL)
    IF (NOT DEFINED ESCARGOT_THREADING)
        SET (ESCARGOT_THREADING OFF)
    ENDIF()
    IF (NOT DEFINED ESCARGOT_LIBICU_SUPPORT)
        SET (ESCARGOT_LIBICU_SUPPORT OFF)
    ENDIF()
    # CompressibleString trades flash (the LZ4 codec + bookkeeping) for RAM
    # (compressing idle long-lived strings). On flash-constrained bare-metal
    # single-script targets the code cost is the scarcer resource, so default
    # it off; it can still be forced on with -DESCARGOT_COMPRESSIBLE_STRING=ON.
    IF (NOT DEFINED ESCARGOT_COMPRESSIBLE_STRING)
        SET (ESCARGOT_COMPRESSIBLE_STRING OFF)
    ENDIF()
ENDIF()


# Safe inheritance of standard CMake variables to ESCARGOT_ prefix options (Zero global namespace pollution!)
IF (NOT DEFINED ESCARGOT_BUILD_SHARED_LIBS)
    IF (DEFINED BUILD_SHARED_LIBS)
        SET(ESCARGOT_BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS})
    ELSE()
        SET(ESCARGOT_BUILD_SHARED_LIBS OFF)
    ENDIF()
ENDIF()

IF (NOT DEFINED ESCARGOT_ENABLE_SHELL)
    IF (DEFINED ESCARGOT_SHELL)
        SET(ESCARGOT_ENABLE_SHELL ${ESCARGOT_SHELL})
    ELSEIF (DEFINED ENABLE_SHELL)
        SET(ESCARGOT_ENABLE_SHELL ${ENABLE_SHELL})
    ELSEIF (ESCARGOT_NAPI)
        # Node-API hides all proprietary public APIs for binary size
        # optimization, so the standard C++ shell target cannot be built
        # alongside it (see build/escargot.cmake's FATAL_ERROR guard below).
        # Default shell off instead of forcing every NAPI caller to pass
        # -DESCARGOT_ENABLE_SHELL=OFF explicitly.
        SET(ESCARGOT_ENABLE_SHELL OFF)
    ELSE()
        SET(ESCARGOT_ENABLE_SHELL ON)
    ENDIF()
ENDIF()

# Map legacy DEBUGGER input to standard ESCARGOT_DEBUGGER variable
IF (DEFINED DEBUGGER)
    SET(ESCARGOT_DEBUGGER ${DEBUGGER})
ENDIF()

option(ESCARGOT_BUILD_SHARED_LIBS "Build shared libraries" ${ESCARGOT_BUILD_SHARED_LIBS})
option(ESCARGOT_BUILD_GC_SHARED_LIBS "Build GCutil as a shared library" OFF)
option(ESCARGOT_ENABLE_SHELL "Build the Escargot shell" ${ESCARGOT_ENABLE_SHELL})
option(ESCARGOT_BUILD_CCTEST "Build the C++ tests" OFF)
option(ESCARGOT_TEST "Enable additional features used only for testing" OFF)
option(ESCARGOT_DEBUGGER "Enable debugger" ${ESCARGOT_DEBUGGER})
# Temporal reached TC39 Stage 4 in March 2026 (ships in ES2026), so it's a
# finished part of the standard -- default it ON, EXCEPT Temporal requires ICU
# (see build/config.cmake's "Temporal feature needs ICU" FATAL_ERROR) and
# ESCARGOT_LIBICU_SUPPORT isn't resolved to its final value until
# build/config.cmake runs, so ESCARGOT_TEMPORAL's default is computed there
# instead, right after ESCARGOT_LIBICU_SUPPORT's own default. ShadowRealm is
# still at Stage 2.7 as of this writing (not yet even Stage 3), so it stays
# opt-in regardless of ICU.
option(ESCARGOT_SHADOWREALM "Enable ShadowRealm support" OFF)
option(ESCARGOT_WASM "Enable WebAssembly support" OFF)
option(ESCARGOT_CODE_CACHE "Enable code cache" OFF)
option(ESCARGOT_TCO "Enable tail call optimization" OFF)
option(ESCARGOT_NAPI "Enable Node-API (N-API) support and C-style hosting APIs" OFF)
option(ESCARGOT_SMALL_CONFIG "Enable aggressive memory optimizations for tiny devices" OFF)
option(ESCARGOT_EXPORT_ALL "Export all symbols instead of the default curated public API" OFF)
IF (NOT DEFINED ESCARGOT_THREADING)
    SET (ESCARGOT_THREADING ON)
ENDIF()
option(ESCARGOT_THREADING "Enable threading features (e.g. Atomics, SharedArrayBuffer)" ${ESCARGOT_THREADING})

# =============================================================================
# THREAD LOCAL STORAGE (TLS) ACCESS OPTIMIZATION STRATEGIES
# =============================================================================
# Two types of fast TLS access optimizations are implemented in this project:
#
# 1. Raw Address TLS Optimization (ESCARGOT_TLS_ACCESS_BY_ADDRESS)
#    - Bypasses traditional TLS lookups by reading the Thread Pointer directly
#      from hardware registers (e.g. %fs, %gs, tpidr_el0, or tp) via inline asm,
#      and applying a static offset calculated at startup.
#    - RISK: Assumes a strictly static memory layout. Under dynamic linking (.so),
#      lazy TLS allocation, or NPTL variances, offsets may diverge per-thread,
#      causing dynamic-sharing crashes or RELEASE_ASSERT failures.
#    - Darwin (macOS) Warning: Known to fail completely. Mach-O thread-local 
#      variables (TLV) are lazily allocated and do not hold a fixed-offset guarantee.
#
# 2. Pthread Key Offset Search TLS Optimization (ESCARGOT_TLS_ACCESS_BY_PTHREAD_KEY)
#    - Bypasses pthread_getspecific() call overhead. During startup, it writes a
#      known signature using pthread_setspecific(), scans the TCB (Thread Control
#      Block) memory space to discover the exact slot offset, and uses pointer
#      arithmetic (tlsBase + offset) for subsequent accesses.
#    - RISK: Lower-level memory bypass hack. Bears potential for signature collisions
#      in other TCB fields, future libc layout changes, or memory violations.
#      It may fail or crash on certain environments with unique TCB structures or
#      hardened memory restrictions.
#    - Darwin (macOS) Warning: Known to fail on Darwin. Darwin's TCB internal layout
#      differs significantly from glibc/Bionic, making memory-scanning offsets 
#      unreliable and causing unexpected crashes.
#
# * RECOMMENDATION FOR PERFORMANCE:
#   For maximum performance on stable GNU libc (glibc) or Bionic (Android) based 
#   environments, it is highly recommended to manually opt into ONE of these 
#   optimizations depending on your platform requirements. Passing either flag can
#   drastically improve TLS access speeds.
#
# * DEFAULT BEHAVIOR (SAFETY-FIRST):
#   Because memory layout and thread environments can vary wildly, BOTH optimizations
#   are disabled by default (OFF) to guarantee absolute runtime stability.
#
#   To enable them, pass either of the following flags to your CMake configuration:
#   -DESCARGOT_TLS_ACCESS_BY_PTHREAD_KEY=ON  (or -DESCARGOT_TLS_ACCESS_BY_ADDRESS=ON)
# =============================================================================

# Detect Sanitizers (ASAN/TSAN) and force-disable TLS optimizations.
# Manual memory scans and address bypass hacks violate safety invariants 
# monitored by sanitizers and will produce false-positive memory access traps.
SET (ESCARGOT_SANITIZER_ACTIVE OFF)
IF (ESCARGOT_ASAN)
    SET (ESCARGOT_SANITIZER_ACTIVE ON)
ENDIF()
IF ("${CMAKE_C_FLAGS}" MATCHES "-fsanitize=address" OR "${CMAKE_CXX_FLAGS}" MATCHES "-fsanitize=address" OR
    "${CMAKE_C_FLAGS}" MATCHES "-fsanitize=thread" OR "${CMAKE_CXX_FLAGS}" MATCHES "-fsanitize=thread" OR
    "$ENV{CFLAGS}" MATCHES "-fsanitize=address" OR "$ENV{CXXFLAGS}" MATCHES "-fsanitize=address" OR
    "$ENV{CFLAGS}" MATCHES "-fsanitize=thread" OR "$ENV{CXXFLAGS}" MATCHES "-fsanitize=thread")
    SET (ESCARGOT_SANITIZER_ACTIVE ON)
ENDIF()

IF (ESCARGOT_SANITIZER_ACTIVE)
    MESSAGE (STATUS "Sanitizer (ASAN/TSAN) detected. Forcing fast TLS access optimizations OFF for safety.")
    SET (ESCARGOT_TLS_ACCESS_BY_ADDRESS OFF CACHE BOOLEAN "Enable thread local storage access optimization (offset)" FORCE)
    SET (ESCARGOT_TLS_ACCESS_BY_PTHREAD_KEY OFF CACHE BOOLEAN "Enable thread local storage access optimization (pthread_key)" FORCE)
ENDIF()

# Default to OFF for safety-first policy across most environments.
# Exception: Android is a highly controlled environment with consistent Bionic libc
# and TCB structure where TLS Pthread Key optimization is proven to be extremely stable.
# Therefore, it is enabled by default for Android host targets.
IF (ESCARGOT_THREADING AND NOT DEFINED ESCARGOT_TLS_ACCESS_BY_ADDRESS AND NOT DEFINED ESCARGOT_TLS_ACCESS_BY_PTHREAD_KEY)
    IF (ESCARGOT_HOST STREQUAL "android")
        SET (ESCARGOT_TLS_ACCESS_BY_ADDRESS OFF)
        SET (ESCARGOT_TLS_ACCESS_BY_PTHREAD_KEY ON)
    ELSE()
        SET (ESCARGOT_TLS_ACCESS_BY_ADDRESS OFF)
        SET (ESCARGOT_TLS_ACCESS_BY_PTHREAD_KEY OFF)
    ENDIF()
ENDIF()
option(ESCARGOT_TLS_ACCESS_BY_ADDRESS "Enable thread local storage access optimization (offset)" ${ESCARGOT_TLS_ACCESS_BY_ADDRESS})
option(ESCARGOT_TLS_ACCESS_BY_PTHREAD_KEY "Enable thread local storage access optimization (pthread_key)" ${ESCARGOT_TLS_ACCESS_BY_PTHREAD_KEY})

IF (ESCARGOT_THREADING AND NOT ESCARGOT_TLS_ACCESS_BY_ADDRESS AND NOT ESCARGOT_TLS_ACCESS_BY_PTHREAD_KEY)
    MESSAGE(STATUS "--------------------------------------------------------------------------------")
    MESSAGE(STATUS " TLS OPTIMIZATION NOTICE:")
    MESSAGE(STATUS "   Fast TLS access optimizations are currently DISABLED by default for maximum safety.")
    MESSAGE(STATUS "   On stable glibc or Android Bionic platforms, manually enabling one of these can")
    MESSAGE(STATUS "   significantly improve TLS access speeds. Consider passing either:")
    MESSAGE(STATUS "     -DESCARGOT_TLS_ACCESS_BY_PTHREAD_KEY=ON  (Recommended)")
    MESSAGE(STATUS "     -DESCARGOT_TLS_ACCESS_BY_ADDRESS=ON")
    MESSAGE(STATUS "--------------------------------------------------------------------------------")
ENDIF()

SET (ESCARGOT_TARGET escargot)
SET (ESCARGOT_CCTEST_TARGET cctest)

INCLUDE (ProcessorCount)
PROCESSORCOUNT (NPROCS)

# INCLUDE CMAKE FILES
INCLUDE (${PROJECT_SOURCE_DIR}/build/config.cmake)
INCLUDE (${PROJECT_SOURCE_DIR}/build/escargot.cmake)

# Pkgconfig
CONFIGURE_FILE(${PROJECT_SOURCE_DIR}/packaging/escargot.pc.in ${CMAKE_BINARY_DIR}/escargot.pc @ONLY)

IF (ESCARGOT_TLS_ACCESS_BY_PTHREAD_KEY AND ESCARGOT_TLS_ACCESS_BY_ADDRESS)
    MESSAGE(FATAL_ERROR "You cannot enable ESCARGOT_TLS_ACCESS_BY_PTHREAD_KEY and ESCARGOT_TLS_ACCESS_BY_ADDRESS at same time")
ENDIF()

MESSAGE(STATUS "Escargot Arch: " ${ESCARGOT_ARCH})



MESSAGE(STATUS "--------------------------------------------------------------------------------")
# MESSAGE() concatenates its arguments with NO separator (it has never
# space-joined them -- that's a common misreading of its behavior), so
# `MESSAGE(STATUS "prefix: " ${LIST})` where LIST expands to several unquoted
# words prints them all jammed together with no delimiter at all (e.g.
# "-std=c++11-g3-fno-rtti-..."), which is unreadable and was mistaken for a
# missing optimization flag when someone was actually just staring at this
# printout. Join with real spaces first. (Also: this print only ever showed
# the base ESCARGOT_CXXFLAGS, never the per-config ESCARGOT_CONFIG_CXXFLAGS
# -DO2/-O0/etc. added via generator expressions in build/escargot.cmake --
# those aren't resolved until build time, so there's no way to print their
# final value at configure time. The actual optimization flags ARE present
# in the real compile command; verified via `-O3 ... -O2 ...` in a Release
# build's ninja rule.)
STRING (REPLACE ";" " " ESCARGOT_DEFINITIONS_PRINT "${ESCARGOT_DEFINITIONS}")
STRING (REPLACE ";" " " ESCARGOT_CXXFLAGS_PRINT "${ESCARGOT_CXXFLAGS}")
STRING (REPLACE ";" " " ESCARGOT_LDFLAGS_PRINT "${ESCARGOT_LDFLAGS}")
STRING (REPLACE ";" " " ESCARGOT_INCDIRS_PRINT "${ESCARGOT_INCDIRS}")
STRING (REPLACE ";" " " ESCARGOT_LIBRARIES_PRINT "${ESCARGOT_LIBRARIES}")
MESSAGE(STATUS "ESCARGOT_DEFINITIONS: ${ESCARGOT_DEFINITIONS_PRINT}")
MESSAGE(STATUS "ESCARGOT_CXXFLAGS: ${ESCARGOT_CXXFLAGS_PRINT}")
MESSAGE(STATUS "ESCARGOT_LDFLAGS: ${ESCARGOT_LDFLAGS_PRINT}")
MESSAGE(STATUS "ESCARGOT_INCDIRS: ${ESCARGOT_INCDIRS_PRINT}")
MESSAGE(STATUS "ESCARGOT_LIBRARIES: ${ESCARGOT_LIBRARIES_PRINT}")
MESSAGE(STATUS "ESCARGOT_LIBICU_SUPPORT: " ${ESCARGOT_LIBICU_SUPPORT})
MESSAGE(STATUS "ESCARGOT_LIBICU_SUPPORT_WITH_DLOPEN: " ${ESCARGOT_LIBICU_SUPPORT_WITH_DLOPEN})
MESSAGE(STATUS "ESCARGOT_SMALL_CONFIG: " ${ESCARGOT_SMALL_CONFIG})
MESSAGE(STATUS "ESCARGOT_CODE_CACHE: " ${ESCARGOT_CODE_CACHE})
MESSAGE(STATUS "ESCARGOT_WASM: " ${ESCARGOT_WASM})
MESSAGE(STATUS "ESCARGOT_THREADING: " ${ESCARGOT_THREADING})
MESSAGE(STATUS "ESCARGOT_TLS_ACCESS_BY_ADDRESS: " ${ESCARGOT_TLS_ACCESS_BY_ADDRESS})
MESSAGE(STATUS "ESCARGOT_TLS_ACCESS_BY_PTHREAD_KEY: " ${ESCARGOT_TLS_ACCESS_BY_PTHREAD_KEY})
MESSAGE(STATUS "ESCARGOT_EXPORT_ALL: " ${ESCARGOT_EXPORT_ALL})
MESSAGE(STATUS "ESCARGOT_TCO: " ${ESCARGOT_TCO})
MESSAGE(STATUS "ESCARGOT_TEMPORAL: " ${ESCARGOT_TEMPORAL})
MESSAGE(STATUS "ESCARGOT_SHADOWREALM: " ${ESCARGOT_SHADOWREALM})
MESSAGE(STATUS "ESCARGOT_NAPI: " ${ESCARGOT_NAPI})
MESSAGE(STATUS "ESCARGOT_TEST: " ${ESCARGOT_TEST})
