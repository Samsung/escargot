CMAKE_MINIMUM_REQUIRED (VERSION 2.8.12 FATAL_ERROR)

# Enable -fPIC for all targets when building shared libraries
# This is required for static libraries (GCutil, runtime_icu_binder, etc.)
# that will be linked into libescargot.so
IF (ESCARGOT_BUILD_SHARED_LIBS)
    SET (CMAKE_POSITION_INDEPENDENT_CODE ON)
ENDIF()

#######################################################
# CONFIGURATION
#######################################################
SET (ESCARGOT_LIBRARIES)
SET (ESCARGOT_INCDIRS)

# CONFIGURE ESCARGOT VERSION
# 1. Read release version from RELEASE_VERSION file (primary source)
IF (EXISTS "${PROJECT_SOURCE_DIR}/RELEASE_VERSION")
    FILE (STRINGS "${PROJECT_SOURCE_DIR}/RELEASE_VERSION" ESCARGOT_VERSION LIMIT_COUNT 1)
ENDIF()
IF ((NOT DEFINED ESCARGOT_VERSION) OR (ESCARGOT_VERSION STREQUAL ""))
    SET (ESCARGOT_VERSION "unknown")
ENDIF()

# 2. Read git short hash as build identifier (supplementary, optional)
FIND_PACKAGE(Git)
IF (GIT_FOUND)
    EXECUTE_PROCESS (
        COMMAND ${GIT_EXECUTABLE} rev-parse --short=8 HEAD
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        OUTPUT_VARIABLE ESCARGOT_BUILD
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE)
ENDIF()
IF ((NOT DEFINED ESCARGOT_BUILD) OR (ESCARGOT_BUILD STREQUAL ""))
    UNSET (ESCARGOT_BUILD)
ENDIF()

IF (DEFINED ESCARGOT_BUILD)
    MESSAGE(STATUS "Escargot Version: ${ESCARGOT_VERSION} (build: ${ESCARGOT_BUILD})")
ELSE()
    MESSAGE(STATUS "Escargot Version: ${ESCARGOT_VERSION}")
ENDIF()
CONFIGURE_FILE (${PROJECT_SOURCE_DIR}/src/EscargotInfo.h.in ${CMAKE_BINARY_DIR}/escargot_generated/EscargotInfo.h)
SET (ESCARGOT_INCDIRS ${ESCARGOT_INCDIRS} ${CMAKE_BINARY_DIR}/escargot_generated)

#######################################################
# PATH
#######################################################
SET (ESCARGOT_ROOT ${PROJECT_SOURCE_DIR})
SET (ESCARGOT_THIRD_PARTY_ROOT ${ESCARGOT_ROOT}/third_party)
SET (GCUTIL_ROOT ${ESCARGOT_THIRD_PARTY_ROOT}/GCutil)

#######################################################
# FLAGS FOR TARGET
#######################################################
INCLUDE (${ESCARGOT_ROOT}/build/target.cmake)

#######################################################
# FLAGS FOR COMMON
#######################################################
# ESCARGOT COMMON CXXFLAGS
SET (ESCARGOT_DEFINITIONS
    ${ESCARGOT_DEFINITIONS}
    -DESCARGOT
)

SET (CXXFLAGS_FROM_ENV $ENV{CXXFLAGS})
SEPARATE_ARGUMENTS(CXXFLAGS_FROM_ENV NATIVE_COMMAND)
SET (CFLAGS_FROM_ENV $ENV{CFLAGS})
SEPARATE_ARGUMENTS(CFLAGS_FROM_ENV NATIVE_COMMAND)
SET (LDFLAGS_FROM_ENV $ENV{LDFLAGS})
SEPARATE_ARGUMENTS(LDFLAGS_FROM_ENV NATIVE_COMMAND)

# these flags assigned from external should have the highest priority
SET (CXXFLAGS_FROM_ENV ${CXXFLAGS_FROM_ENV} ${ESCARGOT_CXXFLAGS_FROM_EXTERNAL})
SET (CFLAGS_FROM_ENV ${CFLAGS_FROM_ENV} ${ESCARGOT_CFLAGS_FROM_EXTERNAL})
SET (LDFLAGS_FROM_ENV ${LDFLAGS_FROM_ENV} ${ESCARGOT_LDFLAGS_FROM_EXTERNAL})

# ESCARGOT COMMON LDFLAGS
# Note: -fvisibility=hidden is set in target.cmake for the compiler
# Note: GC_DEBUG for the debug config is added via ESCARGOT_DEFINITIONS_DEBUG
# below (consumed through the ESCARGOT_CONFIG_DEFINITIONS generator
# expression in build/escargot.cmake). The android-shared "-shared" linker
# flag is added via TARGET_LINK_LIBRARIES in build/escargot.cmake instead
# (this used to be duplicated here, gated on the wrong variable too -- the
# un-namespaced global BUILD_SHARED_LIBS instead of ESCARGOT_BUILD_SHARED_LIBS).

IF (ESCARGOT_SMALL_CONFIG)
    SET (ESCARGOT_DEFINITIONS ${ESCARGOT_DEFINITIONS} -DESCARGOT_SMALL_CONFIG)
ENDIF()

IF (NOT DEFINED ESCARGOT_LIBICU_SUPPORT)
    SET (ESCARGOT_LIBICU_SUPPORT ON)
ENDIF()
option(ESCARGOT_LIBICU_SUPPORT "Include libicu library" ${ESCARGOT_LIBICU_SUPPORT})

# Temporal reached TC39 Stage 4 (see CMakeLists.txt), but it hard-requires ICU
# (see the "Temporal feature needs ICU" FATAL_ERROR below) -- only default it
# ON once ESCARGOT_LIBICU_SUPPORT's own default (just above) is resolved, so
# ICU-less configs (bare-metal, or an explicit -DESCARGOT_LIBICU_SUPPORT=OFF)
# don't hard-fail configure just from picking up this new default.
IF (NOT DEFINED ESCARGOT_TEMPORAL)
    SET (ESCARGOT_TEMPORAL ${ESCARGOT_LIBICU_SUPPORT})
ENDIF()
option(ESCARGOT_TEMPORAL "Enable Temporal support" ${ESCARGOT_TEMPORAL})

IF (NOT DEFINED ESCARGOT_LIBICU_SUPPORT_WITH_DLOPEN)
    SET (ESCARGOT_LIBICU_SUPPORT_WITH_DLOPEN ON)
ENDIF()
option(ESCARGOT_LIBICU_SUPPORT_WITH_DLOPEN "Load libicu at runtime via dlopen() instead of linking directly" ${ESCARGOT_LIBICU_SUPPORT_WITH_DLOPEN})

#######################################################
# FLAGS FOR ADDITIONAL FUNCTION
#######################################################
option(ESCARGOT_DEPLOY "Build for deployment (set up RPATH for a bundled ICU)" OFF)
IF (ESCARGOT_LIBICU_SUPPORT)
    IF (ESCARGOT_DEPLOY)
        # Build for deployment (include ICU library)
        SET (CMAKE_INSTALL_RPATH "$ORIGIN")
        SET (CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
        SET (CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
    ENDIF()

    IF (ESCARGOT_LIBICU_SUPPORT_WITH_DLOPEN)
        SET (ESCARGOT_DEFINITIONS ${ESCARGOT_DEFINITIONS} -DENABLE_ICU -DENABLE_INTL -DENABLE_RUNTIME_ICU_BINDER)
        SET (ESCARGOT_DEFINITIONS ${ESCARGOT_DEFINITIONS} -DENABLE_INTL_DISPLAYNAMES -DENABLE_INTL_NUMBERFORMAT -DENABLE_INTL_PLURALRULES)
        SET (ESCARGOT_DEFINITIONS ${ESCARGOT_DEFINITIONS} -DENABLE_INTL_RELATIVETIMEFORMAT -DENABLE_INTL_LISTFORMAT -DENABLE_INTL_DURATIONFORMAT)
        SET (ESCARGOT_DEFINITIONS ${ESCARGOT_DEFINITIONS} -DENABLE_INTL_SEGMENTER)
    ELSE()
        IF (NOT ${ESCARGOT_HOST} STREQUAL "windows")
            SET (ESCARGOT_DEFINITIONS ${ESCARGOT_DEFINITIONS} -DENABLE_INTL_DISPLAYNAMES -DENABLE_INTL_NUMBERFORMAT -DENABLE_INTL_PLURALRULES)
            SET (ESCARGOT_DEFINITIONS ${ESCARGOT_DEFINITIONS} -DENABLE_INTL_RELATIVETIMEFORMAT -DENABLE_INTL_LISTFORMAT -DENABLE_INTL_DURATIONFORMAT)
            SET (ESCARGOT_DEFINITIONS ${ESCARGOT_DEFINITIONS} -DENABLE_INTL_SEGMENTER)

            PKG_CHECK_MODULES(ICU REQUIRED icu-uc icu-i18n)
        ENDIF()

        MESSAGE(STATUS "ICU Libraries: ${ICU_LIBRARIES}")
        SET (ESCARGOT_LDFLAGS ${ESCARGOT_LDFLAGS} ${ICU_LDFLAGS})
        SET (ESCARGOT_DEFINITIONS ${ESCARGOT_DEFINITIONS} -DENABLE_ICU -DENABLE_INTL)
        SET (ESCARGOT_INCDIRS ${ESCARGOT_INCDIRS} ${ICU_INCLUDE_DIRS})
        SET (ESCARGOT_LIBRARIES ${ESCARGOT_LIBRARIES} ${ICU_LIBRARIES})
    ENDIF()
ENDIF()

# napi_define_class needs FunctionTemplateRef, which is gated behind
# ENABLE_EXTENDED_API; default it on for NAPI unless the caller already chose
IF (ESCARGOT_NAPI AND NOT DEFINED ESCARGOT_USE_EXTENDED_API)
    SET (ESCARGOT_USE_EXTENDED_API ON)
ENDIF()
option(ESCARGOT_USE_EXTENDED_API "Enable the extended C++ API (FunctionTemplateRef, etc.)" ${ESCARGOT_USE_EXTENDED_API})

IF (ESCARGOT_USE_EXTENDED_API)
    SET (ESCARGOT_DEFINITIONS ${ESCARGOT_DEFINITIONS} -DENABLE_EXTENDED_API)
ENDIF()

option(ESCARGOT_USE_CUSTOM_LOGGING "Use a custom logging backend instead of the host's native log (e.g. dlog on Tizen)" OFF)
IF (ESCARGOT_USE_CUSTOM_LOGGING)
    SET (ESCARGOT_DEFINITIONS ${ESCARGOT_DEFINITIONS} -DENABLE_CUSTOM_LOGGING)
ELSEIF (${ESCARGOT_HOST} STREQUAL "tizen" OR ${ESCARGOT_HOST} STREQUAL "tizen_obs")
    PKG_CHECK_MODULES (DLOG REQUIRED dlog)
    SET (ESCARGOT_LIBRARIES ${ESCARGOT_LIBRARIES} ${DLOG_LIBRARIES})
    SET (ESCARGOT_INCDIRS ${ESCARGOT_INCDIRS} ${DLOG_INCLUDE_DIRS})
    SET (ESCARGOT_CXXFLAGS ${ESCARGOT_CXXFLAGS} ${DLOG_CFLAGS_OTHER})
ENDIF()

SET (ESCARGOT_DEFINITIONS ${ESCARGOT_DEFINITIONS} -DENABLE_COMPRESSIBLE_STRING)
IF (ESCARGOT_SMALL_CONFIG)
    SET (ESCARGOT_DEFINITIONS ${ESCARGOT_DEFINITIONS} -DLZ4_MEMORY_USAGE=16 -DLZ4_HEAPMODE=1)
ENDIF()

SET (ESCARGOT_DEFINITIONS ${ESCARGOT_DEFINITIONS} -DENABLE_RELOADABLE_STRING)

IF (ESCARGOT_CODE_CACHE)
    SET (ESCARGOT_DEFINITIONS ${ESCARGOT_DEFINITIONS} -DENABLE_CODE_CACHE)
ENDIF()

IF (ESCARGOT_WASM)
    SET (ESCARGOT_DEFINITIONS ${ESCARGOT_DEFINITIONS} -DENABLE_WASM)
    # threading should be enabled for WASM (WASM threading feature). This used
    # to be an "IF (NOT DEFINED ESCARGOT_THREADING)" guard, but by the time
    # this file runs, CMakeLists.txt has already unconditionally resolved
    # ESCARGOT_THREADING to ON/OFF (it's always DEFINED here), so that guard
    # was dead code -- WASM builds on bare-metal (where threading defaults
    # OFF) silently lost this override. Force it on instead, and warn since
    # ESCARGOT_THREADING's TLS-access-strategy defaults in CMakeLists.txt have
    # already been computed against the old value by this point.
    IF (NOT ESCARGOT_THREADING)
        MESSAGE (WARNING "ESCARGOT_WASM requires ESCARGOT_THREADING -- forcing ESCARGOT_THREADING ON")
        SET (ESCARGOT_THREADING ON)
    ENDIF()
ENDIF()

IF (ESCARGOT_THREADING)
    SET (ESCARGOT_DEFINITIONS ${ESCARGOT_DEFINITIONS} -DENABLE_THREADING -DGC_THREAD_ISOLATE)
ENDIF()

IF (ESCARGOT_EXPORT_ALL)
    SET (ESCARGOT_CXXFLAGS ${ESCARGOT_CXXFLAGS} -fvisibility=default)
ENDIF()

option(ESCARGOT_TCO_DEBUG "Enable extra tail-call-optimization debug checks (debug builds only)" OFF)
IF (ESCARGOT_TCO)
    SET (ESCARGOT_DEFINITIONS ${ESCARGOT_DEFINITIONS} -DENABLE_TCO)
    IF (ESCARGOT_TCO_DEBUG)
        IF (NOT ${ESCARGOT_MODE} STREQUAL "debug")
            MESSAGE (FATAL_ERROR "ESCARGOT_TCO_DEBUG is enabled only for debug mode")
        ENDIF()
        SET (ESCARGOT_DEFINITIONS ${ESCARGOT_DEFINITIONS} -DENABLE_TCO_DEBUG)
    ENDIF()
ENDIF()

IF (ESCARGOT_TEMPORAL)
    SET (ESCARGOT_DEFINITIONS ${ESCARGOT_DEFINITIONS} -DENABLE_TEMPORAL)
    IF (NOT ESCARGOT_LIBICU_SUPPORT)
        MESSAGE (FATAL_ERROR "Temporal feature needs ICU")
    ENDIF()
ENDIF()

IF (ESCARGOT_SHADOWREALM)
    SET (ESCARGOT_DEFINITIONS ${ESCARGOT_DEFINITIONS} -DENABLE_SHADOWREALM)
ENDIF()

IF (ESCARGOT_NAPI)
    SET (ESCARGOT_DEFINITIONS ${ESCARGOT_DEFINITIONS} -DENABLE_NAPI)
    # Target Node-API version 10: unlocks the additive v9/v10 declarations
    # (node_api_create_syntax_error, property-key/external-string helpers, ...)
    # in the vendored headers, which otherwise default to NAPI_VERSION 8. Also
    # what napi_get_version reports.
    SET (ESCARGOT_DEFINITIONS ${ESCARGOT_DEFINITIONS} -DNAPI_VERSION=10)
    SET (ESCARGOT_INCDIRS ${ESCARGOT_INCDIRS} ${ESCARGOT_ROOT}/test/napi-tc/src)
    SET (ESCARGOT_LIBRARIES ${ESCARGOT_LIBRARIES} dl)
    # napi_* symbols live in this binary itself; addons are dlopen()'d with
    # unresolved napi_* references (same as real Node, which also needs
    # -rdynamic on its own executable for the same reason). Each napi_*
    # definition carries its own default-visibility attribute (see
    # ESCARGOT_NAPI_EXPORT in NapiTypes.h) instead of overriding
    # -fvisibility=hidden for the whole binary.
    SET (ESCARGOT_LDFLAGS ${ESCARGOT_LDFLAGS} -rdynamic)

    # async_work/threadsafe_function (NapiAsyncWork.cpp) and
    # napi_get_uv_event_loop (NapiRuntime.cpp) are backed by a real system
    # libuv event loop - link it via pkg-config, scoped to ESCARGOT_NAPI only
    # so non-napi builds stay unaffected.
    PKG_CHECK_MODULES (LIBUV REQUIRED libuv)
    SET (ESCARGOT_INCDIRS ${ESCARGOT_INCDIRS} ${LIBUV_INCLUDE_DIRS})
    SET (ESCARGOT_LIBRARIES ${ESCARGOT_LIBRARIES} ${LIBUV_LIBRARIES})
    SET (ESCARGOT_LDFLAGS ${ESCARGOT_LDFLAGS} ${LIBUV_LDFLAGS})
ENDIF()

IF (ESCARGOT_TLS_ACCESS_BY_ADDRESS)
    SET (ESCARGOT_DEFINITIONS ${ESCARGOT_DEFINITIONS} -DENABLE_TLS_ACCESS_BY_ADDRESS)
ENDIF()

IF (ESCARGOT_TLS_ACCESS_BY_PTHREAD_KEY)
    SET (ESCARGOT_DEFINITIONS ${ESCARGOT_DEFINITIONS} -DENABLE_TLS_ACCESS_BY_PTHREAD_KEY)
ENDIF()

IF (ESCARGOT_TEST)
    SET (ESCARGOT_DEFINITIONS ${ESCARGOT_DEFINITIONS} -DESCARGOT_ENABLE_TEST)
    IF (ESCARGOT_CODE_CACHE)
        SET (ESCARGOT_DEFINITIONS ${ESCARGOT_DEFINITIONS} -DCODE_CACHE_MIN_SOURCE_LENGTH=1024)
        SET (ESCARGOT_DEFINITIONS ${ESCARGOT_DEFINITIONS} -DCODE_CACHE_MAX_CACHE_COUNT=128)
        SET (ESCARGOT_DEFINITIONS ${ESCARGOT_DEFINITIONS} -DCODE_CACHE_SHOULD_LOAD_FUNCTIONS_ON_SCRIPT_LOADING=true)
    ENDIF()
ENDIF()

#######################################################
# FLAGS FOR $(MODE) : debug/release
#######################################################
# DEBUG FLAGS
# Bare macro names (no "-D" prefix): these are only ever consumed via
# TARGET_COMPILE_DEFINITIONS() through a $<CONFIG:...> generator expression
# (see ESCARGOT_CONFIG_DEFINITIONS in build/escargot.cmake) -- unlike a plain
# list, CMake does not dedupe an already-"-D"-prefixed entry once it's inside
# a generator expression, producing an invalid "-D-DFOO" flag. Let
# TARGET_COMPILE_DEFINITIONS add the "-D" itself.
SET (ESCARGOT_DEFINITIONS_DEBUG _GLIBCXX_DEBUG GC_DEBUG)

# RELEASE FLAGS
SET (ESCARGOT_DEFINITIONS_RELEASE NDEBUG)

# SHARED_LIB FLAGS
SET (ESCARGOT_CXXFLAGS_SHAREDLIB -fPIC)
SET (ESCARGOT_LDFLAGS_SHAREDLIB -ldl)

# STATIC_LIB FLAGS
IF (${ESCARGOT_HOST} STREQUAL "baremetal")
    # Bare-metal ports link a single flat ELF booted directly at fixed
    # link-time addresses (e.g. loaded straight via QEMU "-kernel" or
    # flashed) -- there is no PIE loader to process GOT/PLT relocations, so
    # -fPIC must NOT apply here despite building a static lib. (Confirmed
    # by direct testing: with -fPIC the FreeRTOS/QEMU sample compiles and
    # links cleanly but crashes into a FreeRTOS configASSERT very early at
    # boot; without it, it boots and runs correctly.)
    SET (ESCARGOT_CXXFLAGS_STATICLIB -DESCARGOT_EXPORT=)
ELSE()
    SET (ESCARGOT_CXXFLAGS_STATICLIB -fPIC -DESCARGOT_EXPORT=)
ENDIF()

# SHELL FLAGS
SET (ESCARGOT_CXXFLAGS_SHELL -DESCARGOT_EXPORT=)

#######################################################
# FLAGS FOR PROFILING
#######################################################
SET (PROFILER_FLAGS)

option(ESCARGOT_PROFILE_BDWGC "Enable bdwgc (Boehm GC) profiling" OFF)
IF (ESCARGOT_PROFILE_BDWGC)
    SET (PROFILER_FLAGS ${PROFILER_FLAGS} -DPROFILE_BDWGC)
ENDIF()

option(ESCARGOT_MEM_STATS "Enable memory usage statistics" OFF)
IF (ESCARGOT_MEM_STATS)
    SET (PROFILER_FLAGS ${PROFILER_FLAGS} -DESCARGOT_MEM_STATS)
ENDIF()

option(ESCARGOT_VALGRIND "Build with Valgrind annotations" OFF)
IF (ESCARGOT_VALGRIND)
    SET (PROFILER_FLAGS ${PROFILER_FLAGS} -DESCARGOT_VALGRIND)
ENDIF()

# Handle legacy/typo variable ESARGOT_GOOGLE_PERF and map to standard ESCARGOT_GOOGLE_PERF
IF (ESARGOT_GOOGLE_PERF)
    SET(ESCARGOT_GOOGLE_PERF ON)
ENDIF()
option(ESCARGOT_GOOGLE_PERF "Build with gperftools (Google Performance Tools) profiling" ${ESCARGOT_GOOGLE_PERF})

IF (ESCARGOT_GOOGLE_PERF)
    SET (PROFILER_FLAGS ${PROFILER_FLAGS} -DESCARGOT_GOOGLE_PERF)
    SET (ESCARGOT_LDFLAGS ${ESCARGOT_LDFLAGS} -lprofiler -lunwind -llzma)
ENDIF()

#######################################################
# FLAGS FOR DEBUGGER
#######################################################
IF (ESCARGOT_DEBUGGER)
    SET (ESCARGOT_DEFINITIONS ${ESCARGOT_DEFINITIONS} -DESCARGOT_DEBUGGER)
ENDIF()
