CMAKE_MINIMUM_REQUIRED (VERSION 2.8.12 FATAL_ERROR)

SET (ESCARGOT_INCDIRS
    ${ESCARGOT_INCDIRS}
    ${ESCARGOT_ROOT}/src/
    ${ESCARGOT_THIRD_PARTY_ROOT}
    ${ESCARGOT_THIRD_PARTY_ROOT}/checked_arithmetic/
    ${ESCARGOT_THIRD_PARTY_ROOT}/double_conversion/
    ${ESCARGOT_THIRD_PARTY_ROOT}/lz4/
    ${ESCARGOT_THIRD_PARTY_ROOT}/rapidjson/include/
    ${ESCARGOT_THIRD_PARTY_ROOT}/yarr/
    ${ESCARGOT_THIRD_PARTY_ROOT}/runtime_icu_binder/
    ${ESCARGOT_THIRD_PARTY_ROOT}/robin_map/include/
    ${ESCARGOT_THIRD_PARTY_ROOT}/xsum/
)

# Convert CMAKE_BUILD_TYPE to lowercase for comparison (standard CMake practice)
# Support both single-config (Ninja, Makefile) and multi-config (Visual Studio, Ninja Multi-Config)
STRING(TOLOWER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE_LOWER)

IF (CMAKE_CONFIGURATION_TYPES)
    # Multi-configuration generators (MSVC, Xcode): CMAKE_BUILD_TYPE is empty
    # Debug/Release is selected at build time via --config
    # Use generator expressions for per-configuration flags
    SET (ESCARGOT_CXXFLAGS $<$<CONFIG:Debug>:${ESCARGOT_CXXFLAGS_DEBUG}>$<$<CONFIG:Release>:${ESCARGOT_CXXFLAGS_RELEASE}> ${ESCARGOT_CXXFLAGS})
    SET (ESCARGOT_LDFLAGS $<$<CONFIG:Debug>:${ESCARGOT_LDFLAGS_DEBUG}>$<$<CONFIG:Release>:${ESCARGOT_LDFLAGS_RELEASE}> ${ESCARGOT_LDFLAGS})
    SET (ESCARGOT_DEFINITIONS $<$<CONFIG:Debug>:${ESCARGOT_DEFINITIONS_DEBUG}>$<$<CONFIG:Release>:${ESCARGOT_DEFINITIONS_RELEASE}> ${ESCARGOT_DEFINITIONS})
ELSEIF (CMAKE_BUILD_TYPE_LOWER STREQUAL "debug")
    SET (ESCARGOT_CXXFLAGS ${ESCARGOT_CXXFLAGS_DEBUG} ${ESCARGOT_CXXFLAGS})
    SET (ESCARGOT_LDFLAGS ${ESCARGOT_LDFLAGS_DEBUG} ${ESCARGOT_LDFLAGS})
    SET (ESCARGOT_DEFINITIONS ${ESCARGOT_DEFINITIONS} ${ESCARGOT_DEFINITIONS_DEBUG})
ELSEIF (CMAKE_BUILD_TYPE_LOWER STREQUAL "release")
    SET (ESCARGOT_CXXFLAGS ${ESCARGOT_CXXFLAGS_RELEASE} ${ESCARGOT_CXXFLAGS})
    SET (ESCARGOT_LDFLAGS ${ESCARGOT_LDFLAGS_RELEASE} ${ESCARGOT_LDFLAGS})
    SET (ESCARGOT_DEFINITIONS ${ESCARGOT_DEFINITIONS} ${ESCARGOT_DEFINITIONS_RELEASE})
ENDIF()

IF (${ESCARGOT_OUTPUT} STREQUAL "shell")
    SET (ESCARGOT_CXXFLAGS ${ESCARGOT_CXXFLAGS} ${ESCARGOT_CXXFLAGS_SHELL})
    SET (ESCARGOT_LDFLAGS ${ESCARGOT_LDFLAGS} ${ESCARGOT_LDFLAGS_SHELL})
    SET (ESCARGOT_DEFINITIONS ${ESCARGOT_DEFINITIONS} ${ESCARGOT_DEFINITIONS_SHELL})
ELSEIF (${ESCARGOT_OUTPUT} STREQUAL "shared_lib")
    SET (ESCARGOT_CXXFLAGS ${ESCARGOT_CXXFLAGS} ${ESCARGOT_CXXFLAGS_SHAREDLIB})
    SET (ESCARGOT_LDFLAGS ${ESCARGOT_LDFLAGS} ${ESCARGOT_LDFLAGS_SHAREDLIB})
    SET (ESCARGOT_DEFINITIONS ${ESCARGOT_DEFINITIONS} ${ESCARGOT_DEFINITIONS_SHAREDLIB})
ELSEIF (${ESCARGOT_OUTPUT} STREQUAL "static_lib")
    SET (ESCARGOT_CXXFLAGS ${ESCARGOT_CXXFLAGS} ${ESCARGOT_CXXFLAGS_STATICLIB})
    SET (ESCARGOT_LDFLAGS ${ESCARGOT_LDFLAGS} ${ESCARGOT_LDFLAGS_STATICLIB})
    SET (ESCARGOT_DEFINITIONS ${ESCARGOT_DEFINITIONS} ${ESCARGOT_DEFINITIONS_STATICLIB})
ENDIF()

IF (ESCARGOT_ASAN)
    SET (ESCARGOT_CXXFLAGS ${ESCARGOT_CXXFLAGS} -fsanitize=address)
    SET (ESCARGOT_LDFLAGS ${ESCARGOT_LDFLAGS} -lasan)
ENDIF()

# Code coverage test with gcovr and Codecov
IF (ESCARGOT_COVERAGE)
    SET (ESCARGOT_CXXFLAGS ${ESCARGOT_CXXFLAGS} -O0 -fprofile-arcs -ftest-coverage)
    SET (ESCARGOT_LDFLAGS ${ESCARGOT_LDFLAGS} --coverage)
    SET (ESCARGOT_LIBRARIES ${ESCARGOT_LIBRARIES} gcov)
ENDIF()

# SOURCE FILES
FILE (GLOB_RECURSE ESCARGOT_SRC ${ESCARGOT_ROOT}/src/*.cpp)
FILE (GLOB YARR_SRC ${ESCARGOT_THIRD_PARTY_ROOT}/yarr/*.cpp)
FILE (GLOB DOUBLE_CONVERSION_SRC ${ESCARGOT_THIRD_PARTY_ROOT}/double_conversion/*.cc)
FILE (GLOB LZ4_SRC ${ESCARGOT_THIRD_PARTY_ROOT}/lz4/*.cpp)
FILE (GLOB XSUM_SRC ${ESCARGOT_THIRD_PARTY_ROOT}/xsum/*.cpp)

IF (NOT ${ESCARGOT_OUTPUT} MATCHES "shell")
    LIST (REMOVE_ITEM ESCARGOT_SRC ${ESCARGOT_ROOT}/src/shell/Shell.cpp)
ENDIF()

# Always add googletest subdirectory when BUILD_TESTING is enabled
# This prevents CMake from finding system gtest
IF (BUILD_TESTING)
    SET (BUILD_GMOCK OFF CACHE BOOL "")
    SET (INSTALL_GTEST OFF CACHE BOOL "")
    ADD_COMPILE_OPTIONS(${ESCARGOT_THIRDPARTY_CFLAGS})
    ADD_SUBDIRECTORY (third_party/googletest)
ENDIF()

IF (${ESCARGOT_OUTPUT} STREQUAL "cctest")
    FILE (GLOB CCTEST_SRC ${ESCARGOT_ROOT}/test/cctest/testapi.cpp)
ENDIF()

# CCTEST_SRC is only for cctest target, not for escargot library
SET (ESCARGOT_SRC_LIST
    ${ESCARGOT_SRC}
    ${YARR_SRC}
    ${DOUBLE_CONVERSION_SRC}
    ${LZ4_SRC}
    ${XSUM_SRC}
)

#######################################################
# GCUTIL
#######################################################
IF (${ESCARGOT_OUTPUT} STREQUAL "shared_lib")
    SET (ESCARGOT_THIRDPARTY_CFLAGS ${ESCARGOT_THIRDPARTY_CFLAGS} ${ESCARGOT_CXXFLAGS_SHAREDLIB})
ELSEIF (${ESCARGOT_OUTPUT} STREQUAL "static_lib")
    SET (ESCARGOT_THIRDPARTY_CFLAGS ${ESCARGOT_THIRDPARTY_CFLAGS} ${ESCARGOT_CXXFLAGS_STATICLIB})
ENDIF()

SET (GCUTIL_CFLAGS ${ESCARGOT_THIRDPARTY_CFLAGS} ${PROFILER_FLAGS})
# Append, don't overwrite: a bare-metal port may have already pre-set this as
# a CACHE variable before add_subdirectory()ing this whole project (e.g.
# NuttX's -D_setjmp=setjmp/-D_longjmp=longjmp remap) -- ESCARGOT_CFLAGS_FROM_EXTERNAL
# (the toolchain/arch flags every other target here also gets) still needs to
# reach GCutil too, since it's compiled for the exact same target.
SET (GCUTIL_CFLAGS_FROM_EXTERNAL ${GCUTIL_CFLAGS_FROM_EXTERNAL} ${ESCARGOT_CFLAGS_FROM_EXTERNAL} ${ESCARGOT_THIRDPARTY_CFLAGS})

IF (ESCARGOT_SMALL_CONFIG)
    SET (GCUTIL_CFLAGS ${GCUTIL_CFLAGS} -DSMALL_CONFIG)
ENDIF()
IF (ESCARGOT_THREADING)
    SET (GCUTIL_ENABLE_THREADING ON)
ENDIF()
IF (ESCARGOT_TLS_ACCESS_BY_ADDRESS)
    SET (GCUTIL_ENABLE_TLS_ACCESS_BY_ADDRESS ON)
ENDIF()
IF (ESCARGOT_TLS_ACCESS_BY_PTHREAD_KEY)
    SET (GCUTIL_ENABLE_TLS_ACCESS_BY_PTHREAD_KEY ON)
ENDIF()

# Legacy compatibility: GCUTIL_MODE is deprecated, use CMAKE_BUILD_TYPE
SET (GCUTIL_MODE ${CMAKE_BUILD_TYPE})

# Bare-metal/RTOS ports set GCUTIL_NOSYS_BAREMETAL/GCUTIL_INITIAL_HEAP_SIZE/
# GCUTIL_CFLAGS_FROM_EXTERNAL as CACHE variables before add_subdirectory()ing
# this whole project (see samples/rtos/*/CMakeLists.txt) -- GCutil's own
# CMakeLists.txt picks its NOSYS source list/defines over the hosted ones
# when that option is ON, so one add_subdirectory() here covers both hosted
# and bare-metal builds. gc-lib's PUBLIC include dir propagates automatically
# via the TARGET_LINK_LIBRARIES() calls below, so no separate INCDIRS entry
# is needed for it.
ADD_SUBDIRECTORY (third_party/GCutil)
SET (ESCARGOT_LIBRARIES ${ESCARGOT_LIBRARIES} gc-lib)

# escargot_lib is added after all dependencies are collected

#######################################################
# SIMDUTF
#######################################################
ADD_LIBRARY (libsimdutf STATIC
    ${ESCARGOT_THIRD_PARTY_ROOT}/simdutf/simdutf.cpp)
TARGET_INCLUDE_DIRECTORIES (libsimdutf PUBLIC ${ESCARGOT_THIRD_PARTY_ROOT}/simdutf)
SET (LIBSIMDUTF_CXXFLAGS
    ${ESCARGOT_CXXFLAGS}
    ${ESCARGOT_THIRDPARTY_CFLAGS}
    ${CXXFLAGS_FROM_ENV}
# disable simd optimization
    -DSIMDUTF_IMPLEMENTATION_ARM64=0
    -DSIMDUTF_IMPLEMENTATION_ICELAKE=0
    -DSIMDUTF_IMPLEMENTATION_HASWELL=0
    -DSIMDUTF_IMPLEMENTATION_WESTMERE=0
    -DSIMDUTF_IMPLEMENTATION_PPC64=0
    -DSIMDUTF_IMPLEMENTATION_RVV=0
    -DSIMDUTF_IMPLEMENTATION_LSX=0
    -DSIMDUTF_IMPLEMENTATION_FALLBACK=1
# ${ESCARGOT_CXXFLAGS_FROM_EXTERNAL} already included in ${CXXFLAGS_FROM_ENV}
    )

IF (CMAKE_BUILD_TYPE_LOWER STREQUAL "debug")
    SET (LIBSIMDUTF_CXXFLAGS ${ESCARGOT_CXXFLAGS_DEBUG} ${LIBSIMDUTF_CXXFLAGS})
ELSEIF (CMAKE_BUILD_TYPE_LOWER STREQUAL "release")
    SET (LIBSIMDUTF_CXXFLAGS ${ESCARGOT_CXXFLAGS_RELEASE} ${LIBSIMDUTF_CXXFLAGS})
ENDIF()

TARGET_COMPILE_OPTIONS (libsimdutf PRIVATE ${LIBSIMDUTF_CXXFLAGS})

SET (ESCARGOT_LIBRARIES ${ESCARGOT_LIBRARIES} libsimdutf)

#######################################################
# LIBBF
#######################################################
ADD_LIBRARY (libbf STATIC
    ${ESCARGOT_THIRD_PARTY_ROOT}/libbf/libbf.c
    ${ESCARGOT_THIRD_PARTY_ROOT}/libbf/cutils.c)
TARGET_INCLUDE_DIRECTORIES (libbf PUBLIC ${ESCARGOT_THIRD_PARTY_ROOT}/libbf)
SET (LIBBF_CFLAGS
	${ESCARGOT_THIRDPARTY_CFLAGS}
	${CFLAGS_FROM_ENV}
# ${ESCARGOT_CFLAGS_FROM_EXTERNAL} already included in ${CFLAGS_FROM_ENV}
    )

IF (CMAKE_BUILD_TYPE_LOWER STREQUAL "debug")
    SET (LIBBF_CFLAGS ${ESCARGOT_CXXFLAGS_DEBUG} ${LIBBF_CFLAGS})
ELSEIF (CMAKE_BUILD_TYPE_LOWER STREQUAL "release")
    SET (LIBBF_CFLAGS ${ESCARGOT_CXXFLAGS_RELEASE} ${LIBBF_CFLAGS})
ENDIF()

TARGET_COMPILE_OPTIONS (libbf PRIVATE ${LIBBF_CFLAGS})

SET (ESCARGOT_LIBRARIES ${ESCARGOT_LIBRARIES} libbf)

#######################################################
# RUNTIME ICU BINDER
#######################################################
SET (RIB_CFLAGS ${ESCARGOT_THIRDPARTY_CFLAGS})
# Legacy compatibility: RIB_MODE is deprecated, use CMAKE_BUILD_TYPE
SET (RIB_MODE ${CMAKE_BUILD_TYPE})
SET (RIB_CFLAGS_FROM_EXTERNAL ${ESCARGOT_CFLAGS_FROM_EXTERNAL})
# runtime_icu_binder unconditionally includes <dlfcn.h> (RuntimeICUBinder.cpp)
# to dlopen() the system ICU at runtime -- there is no ICU and no dlopen() on
# bare-metal/RTOS targets (newlib/nosys has no dlfcn.h at all), so this
# subdirectory must not even be configured/compiled for ESCARGOT_HOST=baremetal,
# not just left unlinked.
IF (NOT ${ESCARGOT_HOST} STREQUAL "baremetal")
    ADD_SUBDIRECTORY (third_party/runtime_icu_binder)

    IF (ESCARGOT_LIBICU_SUPPORT_WITH_DLOPEN)
        SET (ESCARGOT_LIBRARIES ${ESCARGOT_LIBRARIES} runtime-icu-binder-static)
    ENDIF()
ENDIF()

#######################################################
# WebAssembly (walrus)
#######################################################
IF (ESCARGOT_WASM)
    SET (WALRUS_CXXFLAGS
        ${ESCARGOT_THIRDPARTY_CFLAGS}
        -g3)
    SET (WASM_ARCH ${ESCARGOT_ARCH})
    SET (WALRUS_HOST ${ESCARGOT_HOST})
    SET (WALRUS_ARCH ${ESCARGOT_ARCH})
    # Legacy compatibility: WALRUS_MODE is deprecated, use CMAKE_BUILD_TYPE
SET (WALRUS_MODE ${CMAKE_BUILD_TYPE})
    SET (WALRUS_OUTPUT "shared_lib")
    SET (WALRUS_WASI OFF) # WASI should be OFF
    SET (WALRUS_EXTENDED_FEATURES ON) # enable extended features

    IF (CMAKE_BUILD_TYPE_LOWER STREQUAL "release")
        SET (WALRUS_CXXFLAGS ${WALRUS_CXXFLAGS} ${ESCARGOT_CXXFLAGS_RELEASE})
    ENDIF()
    SET (WALRUS_CXXFLAGS_FROM_EXTERNAL ${ESCARGOT_CXXFLAGS_FROM_EXTERNAL})
    SET (WALRUS_LDFLAGS_FROM_EXTERNAL ${ESCARGOT_LDFLAGS_FROM_EXTERNAL})

    # IMPORTANT: Pass architecture flags to walrus submodules (wabt, uvwasi)
    # wabt uses CMAKE_C_FLAGS/CMAKE_CXX_FLAGS directly, not WALRUS_CXXFLAGS
    # Convert CMake list (semicolon-separated) to space-separated string
    STRING (REPLACE ";" " " ESCARGOT_THIRDPARTY_CFLAGS_STR "${ESCARGOT_THIRDPARTY_CFLAGS}")
    
    # Preserve and restore CMAKE_C_FLAGS/CMAKE_CXX_FLAGS to avoid side effects
    SET (WALRUS_BACKUP_C_FLAGS ${CMAKE_C_FLAGS})
    SET (WALRUS_BACKUP_CXX_FLAGS ${CMAKE_CXX_FLAGS})
    
    # Append flags for wabt inheritance (space-separated, not semicolon)
    SET (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${ESCARGOT_THIRDPARTY_CFLAGS_STR}")
    SET (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${ESCARGOT_THIRDPARTY_CFLAGS_STR}")
    
    ADD_SUBDIRECTORY (third_party/walrus)
    SET (ESCARGOT_LIBRARIES ${ESCARGOT_LIBRARIES} walrus)
    
    # Restore original CMAKE_C_FLAGS and CMAKE_CXX_FLAGS
    SET (CMAKE_C_FLAGS ${WALRUS_BACKUP_C_FLAGS})
    SET (CMAKE_CXX_FLAGS ${WALRUS_BACKUP_CXX_FLAGS})
ENDIF()

MAKE_DIRECTORY(${CMAKE_BINARY_DIR}/escargot_generated/tmp)

# yarr/UnicodePatternTables.h
EXECUTE_PROCESS(
    COMMAND python3 ${PROJECT_SOURCE_DIR}/tools/code_generators/generateYarrUnicodePropertyTables.py ${PROJECT_SOURCE_DIR}/tools/unicode_data ${CMAKE_BINARY_DIR}/escargot_generated/tmp/UnicodePatternTables.h
    RESULT_VARIABLE GENERATE_RESULT
    OUTPUT_VARIABLE GENERATE_OUTPUT
    ERROR_VARIABLE GENERATE_ERROR
)

IF (NOT GENERATE_RESULT EQUAL 0)
    MESSAGE(STATUS "Output:\n${GENERATE_OUTPUT}")
    MESSAGE(FATAL_ERROR "${GENERATE_ERROR}")
ENDIF()

EXECUTE_PROCESS (COMMAND ${CMAKE_COMMAND} -E compare_files ${CMAKE_BINARY_DIR}/escargot_generated/tmp/UnicodePatternTables.h ${CMAKE_BINARY_DIR}/escargot_generated/yarr/UnicodePatternTables.h
                RESULT_VARIABLE COMPARE_RESULT
                OUTPUT_VARIABLE COMPARE_OUTPUT
                ERROR_VARIABLE COMPARE_ERROR
)

IF (NOT ${COMPARE_RESULT} EQUAL 0)
    FILE (COPY ${CMAKE_BINARY_DIR}/escargot_generated/tmp/UnicodePatternTables.h DESTINATION ${CMAKE_BINARY_DIR}/escargot_generated/yarr/)
ENDIF()

# yarr/SimpleCaseFoldingTable.h
EXECUTE_PROCESS(
    COMMAND python3 ${PROJECT_SOURCE_DIR}/tools/code_generators/generateSimpleCaseFoldingTable.py ${PROJECT_SOURCE_DIR}/tools/unicode_data ${CMAKE_BINARY_DIR}/escargot_generated/tmp/SimpleCaseFoldingTable.h
    RESULT_VARIABLE GENERATE_RESULT
    OUTPUT_VARIABLE GENERATE_OUTPUT
    ERROR_VARIABLE GENERATE_ERROR
)

IF (NOT GENERATE_RESULT EQUAL 0)
    MESSAGE(STATUS "Output:\n${GENERATE_OUTPUT}")
    MESSAGE(FATAL_ERROR "${GENERATE_ERROR}")
ENDIF()

EXECUTE_PROCESS (COMMAND ${CMAKE_COMMAND} -E compare_files ${CMAKE_BINARY_DIR}/escargot_generated/tmp/SimpleCaseFoldingTable.h ${CMAKE_BINARY_DIR}/escargot_generated/yarr/SimpleCaseFoldingTable.h
                RESULT_VARIABLE COMPARE_RESULT
                OUTPUT_VARIABLE COMPARE_OUTPUT
                ERROR_VARIABLE COMPARE_ERROR
)

IF (NOT ${COMPARE_RESULT} EQUAL 0)
    FILE (COPY ${CMAKE_BINARY_DIR}/escargot_generated/tmp/SimpleCaseFoldingTable.h DESTINATION ${CMAKE_BINARY_DIR}/escargot_generated/yarr/)
ENDIF()

SET (ESCARGOT_INCDIRS
    ${ESCARGOT_INCDIRS}
    ${CMAKE_BINARY_DIR}/escargot_generated/yarr/
)

# =============================================================================
# BUILD TARGETS
# =============================================================================

# Always build the library (STATIC or SHARED based on BUILD_SHARED_LIBS)
# Library target name: escargot (outputs libescargot.so/a)
IF (BUILD_SHARED_LIBS)
    ADD_LIBRARY (escargot SHARED ${ESCARGOT_SRC_LIST})
    TARGET_LINK_LIBRARIES (escargot PUBLIC ${ESCARGOT_LIBRARIES})
    TARGET_INCLUDE_DIRECTORIES (escargot PRIVATE ${ESCARGOT_INCDIRS})
    TARGET_INCLUDE_DIRECTORIES (escargot PUBLIC ${ESCARGOT_ROOT}/src/api)
    TARGET_COMPILE_DEFINITIONS (escargot PRIVATE ${ESCARGOT_DEFINITIONS})
    TARGET_COMPILE_OPTIONS (escargot PRIVATE ${ESCARGOT_CXXFLAGS} ${CXXFLAGS_FROM_ENV})
    # Linker flags: avoid TARGET_LINK_OPTIONS on Windows (causes /machine:X86 to be treated as filename)
    IF (NOT ${ESCARGOT_HOST} STREQUAL "windows")
        IF (ESCARGOT_LDFLAGS OR LDFLAGS_FROM_ENV)
            TARGET_LINK_OPTIONS (escargot PUBLIC ${ESCARGOT_LDFLAGS} ${LDFLAGS_FROM_ENV})
        ENDIF()
    ENDIF()
ELSE()
    ADD_LIBRARY (escargot STATIC ${ESCARGOT_SRC_LIST})
    TARGET_LINK_LIBRARIES (escargot PUBLIC ${ESCARGOT_LIBRARIES})
    TARGET_INCLUDE_DIRECTORIES (escargot PRIVATE ${ESCARGOT_INCDIRS})
    TARGET_INCLUDE_DIRECTORIES (escargot PUBLIC ${ESCARGOT_ROOT}/src/api)
    TARGET_COMPILE_DEFINITIONS (escargot PRIVATE ${ESCARGOT_DEFINITIONS})
    TARGET_COMPILE_OPTIONS (escargot PRIVATE ${ESCARGOT_CXXFLAGS} ${CXXFLAGS_FROM_ENV})
    # Linker flags: avoid TARGET_LINK_OPTIONS on Windows (causes /machine:X86 to be treated as filename)
    IF (NOT ${ESCARGOT_HOST} STREQUAL "windows")
        IF (ESCARGOT_LDFLAGS OR LDFLAGS_FROM_ENV)
            TARGET_LINK_OPTIONS (escargot PUBLIC ${ESCARGOT_LDFLAGS} ${LDFLAGS_FROM_ENV})
        ENDIF()
    ENDIF()
ENDIF()

# Build shell executable only when ESCARGOT_SHELL=ON
# Shell target name: escargot_shell, OUTPUT_NAME: escargot (for test runner compatibility)
# UWP (Windows Store/Phone) does not support standalone shell executables - skip shell target
IF (ESCARGOT_SHELL AND NOT CMAKE_SYSTEM_NAME MATCHES "^Windows")
    ADD_EXECUTABLE (escargot_shell ${ESCARGOT_ROOT}/src/shell/Shell.cpp)
    SET_TARGET_PROPERTIES (escargot_shell PROPERTIES OUTPUT_NAME "escargot")
    
    TARGET_LINK_LIBRARIES (escargot_shell PRIVATE escargot)
    TARGET_INCLUDE_DIRECTORIES (escargot_shell PRIVATE ${ESCARGOT_INCDIRS})
    TARGET_COMPILE_DEFINITIONS (escargot_shell PRIVATE ${ESCARGOT_DEFINITIONS})
    TARGET_COMPILE_OPTIONS (escargot_shell PRIVATE ${ESCARGOT_CXXFLAGS} ${CXXFLAGS_FROM_ENV} ${PROFILER_FLAGS})
    # Linker flags: avoid TARGET_LINK_OPTIONS on Windows (causes /machine:X86 to be treated as filename)
    IF (NOT ${ESCARGOT_HOST} STREQUAL "windows")
        IF (ESCARGOT_LDFLAGS OR LDFLAGS_FROM_ENV)
            TARGET_LINK_OPTIONS (escargot_shell PRIVATE ${ESCARGOT_LDFLAGS} ${LDFLAGS_FROM_ENV})
        ENDIF()
    ENDIF()
ELSEIF (ESCARGOT_SHELL AND CMAKE_SYSTEM_NAME MATCHES "^Windows")
    # For UWP/WinRT, build escargot as static library only (no shell executable)
    # UWP packaging requires proper app manifest and signing, which is beyond scope of this build
    MESSAGE (STATUS "UWP/Windows detected: skipping shell executable (library only)")
ENDIF()

# Build cctest when BUILD_TESTING=ON
IF (BUILD_TESTING AND ${ESCARGOT_OUTPUT} STREQUAL "cctest")
    ADD_EXECUTABLE (${ESCARGOT_CCTEST_TARGET} ${ESCARGOT_SRC_LIST} ${CCTEST_SRC})

    TARGET_LINK_LIBRARIES (${ESCARGOT_CCTEST_TARGET} PRIVATE escargot gtest)
    # googletest include MUST be first to prevent system gtest from being found
    TARGET_INCLUDE_DIRECTORIES (${ESCARGOT_CCTEST_TARGET} PRIVATE 
        ${ESCARGOT_ROOT}/third_party/googletest/googletest/include
        ${ESCARGOT_ROOT}/third_party/googletest/googlemock/include)
    TARGET_INCLUDE_DIRECTORIES (${ESCARGOT_CCTEST_TARGET} PRIVATE
        ${ESCARGOT_INCDIRS})
    TARGET_COMPILE_DEFINITIONS (${ESCARGOT_CCTEST_TARGET} PRIVATE ${ESCARGOT_DEFINITIONS})
    # cctest requires C++14 for googletest
    TARGET_COMPILE_OPTIONS (${ESCARGOT_CCTEST_TARGET} PRIVATE ${ESCARGOT_CXXFLAGS} ${CXXFLAGS_FROM_ENV} -std=c++14)
    # Linker flags: avoid TARGET_LINK_OPTIONS on Windows (causes /machine:X86 to be treated as filename)
    IF (NOT ${ESCARGOT_HOST} STREQUAL "windows")
        IF (ESCARGOT_LDFLAGS OR LDFLAGS_FROM_ENV)
            TARGET_LINK_OPTIONS (${ESCARGOT_CCTEST_TARGET} PRIVATE ${ESCARGOT_LDFLAGS} ${LDFLAGS_FROM_ENV})
        ENDIF()
    ENDIF()
ENDIF()
