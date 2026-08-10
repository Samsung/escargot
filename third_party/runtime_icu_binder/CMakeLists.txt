PROJECT(runtime-icu-binder)
# The "...4.0" range keeps this buildable on CMake 4.x, which hard-errors
# when the effective policy version is below 3.5; pre-3.12 CMake doesn't
# understand the range syntax and just uses 2.8.12 as before (see
# `cmake --help-command cmake_minimum_required`).
CMAKE_MINIMUM_REQUIRED(VERSION 2.8.12...4.0 FATAL_ERROR)

FILE(GLOB RIB_SRC *.cpp)

# Debug Info & Optimization flags per CMake configuration. Computed unconditionally
# (instead of branching on CMAKE_BUILD_TYPE here) and applied below through
# $<CONFIG:...> generator expressions, so multi-config generators (Visual Studio,
# Xcode, Ninja Multi-Config) pick the right flags at build time -- CMAKE_BUILD_TYPE
# isn't set at configure time for those.
IF (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    # Base flags
    SET (RIB_CFLAGS_INTERNAL -fdata-sections -ffunction-sections -fvisibility=hidden)

    SET (RIB_CFLAGS_DEBUG -g3 -O0)
    SET (RIB_CFLAGS_RELWITHDEBINFO -g3 -O2)
    SET (RIB_CFLAGS_MINSIZEREL -Os)
    SET (RIB_CFLAGS_RELEASE -O2)

    # Use local variable instead of modifying global CMAKE_CXX_FLAGS
    SET (RIB_CXX_FLAGS -std=c++11 -fno-rtti)
ELSEIF(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    # Base flags
    SET (RIB_CFLAGS_INTERNAL -fdata-sections -ffunction-sections)

    SET (RIB_CFLAGS_DEBUG /Zi /Od)
    SET (RIB_CFLAGS_RELWITHDEBINFO /Zi /O2)
    SET (RIB_CFLAGS_MINSIZEREL /O1)
    SET (RIB_CFLAGS_RELEASE /O2)

    # Use local variable instead of modifying global CMAKE_CXX_FLAGS
    SET (RIB_CXX_FLAGS /std:c++17 /fp:strict /Zc:__cplusplus /EHs /source-charset:utf-8 /MP /D_CRT_SECURE_NO_WARNINGS /D_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING /wd4244 /wd4267 /wd4805 /wd4018 /wd4172 /wd4146)
ELSE()
    MESSAGE (FATAL_ERROR "unsupported target")
ENDIF()

# "Release-type" here also covers an empty/unset CMAKE_BUILD_TYPE (single-config
# generator with no -DCMAKE_BUILD_TYPE), matching the previous textual ELSE() default.
SET (RIB_CONFIG_COMPILE_FLAGS
    $<$<CONFIG:Debug>:${RIB_CFLAGS_DEBUG}>
    $<$<CONFIG:RelWithDebInfo>:${RIB_CFLAGS_RELWITHDEBINFO}>
    $<$<CONFIG:MinSizeRel>:${RIB_CFLAGS_MINSIZEREL}>
    $<$<AND:$<NOT:$<CONFIG:Debug>>,$<NOT:$<CONFIG:RelWithDebInfo>>,$<NOT:$<CONFIG:MinSizeRel>>>:${RIB_CFLAGS_RELEASE}>
)

ADD_LIBRARY(runtime-icu-binder-static STATIC ${RIB_SRC})
SET_TARGET_PROPERTIES(runtime-icu-binder-static PROPERTIES POSITION_INDEPENDENT_CODE ON)

# SEPARATE_ARGUMENTS()'s NATIVE_COMMAND mode needs CMake 3.9+ -- pick
# WINDOWS_COMMAND/UNIX_COMMAND explicitly instead (that's all NATIVE_COMMAND
# does internally anyway), which has worked since CMake's earliest 2.x
# releases and keeps this compatible with the 2.8.12 floor declared above.
MACRO (SEPARATE_ARGUMENTS_NATIVE_COMPAT _var)
    IF (WIN32)
        SEPARATE_ARGUMENTS (${_var} WINDOWS_COMMAND)
    ELSE()
        SEPARATE_ARGUMENTS (${_var} UNIX_COMMAND)
    ENDIF()
ENDMACRO()

# Get CFLAGS from environment and convert to CMake list (like walrus)
SET (RIB_CFLAGS_FROM_ENV $ENV{CFLAGS})
SEPARATE_ARGUMENTS_NATIVE_COMPAT(RIB_CFLAGS_FROM_ENV)

# Build compile flags as a proper CMake list
SET (RIB_ALL_COMPILE_FLAGS ${RIB_CFLAGS_INTERNAL} ${RIB_CXX_FLAGS} ${RIB_CFLAGS_FROM_ENV})
IF (RIB_CFLAGS_FROM_EXTERNAL)
    IF (NOT RIB_CFLAGS_FROM_EXTERNAL MATCHES ";")
        SEPARATE_ARGUMENTS_NATIVE_COMPAT(RIB_CFLAGS_FROM_EXTERNAL)
    ENDIF()
    LIST(APPEND RIB_ALL_COMPILE_FLAGS ${RIB_CFLAGS_FROM_EXTERNAL})
ENDIF()

# Use target_compile_options for proper flag handling (like walrus)
TARGET_COMPILE_OPTIONS(runtime-icu-binder-static PRIVATE ${RIB_ALL_COMPILE_FLAGS} ${RIB_CONFIG_COMPILE_FLAGS})

# Propagate dl library dependency to dependent targets (e.g. escargot target)
# using Modern CMake INTERFACE linking. This replaces the problematic global link_libraries(-ldl).
IF (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang" AND CMAKE_DL_LIBS)
    TARGET_LINK_LIBRARIES(runtime-icu-binder-static INTERFACE ${CMAKE_DL_LIBS})
ENDIF()
