PROJECT(runtime-icu-binder)
CMAKE_MINIMUM_REQUIRED(VERSION 2.8.12 FATAL_ERROR)

FILE(GLOB RIB_SRC *.cpp)

IF (${CMAKE_CXX_COMPILER_ID} MATCHES "GNU|Clang|AppleClang")
    # Base flags
    SET (RIB_CFLAGS_INTERNAL -fdata-sections -ffunction-sections -fvisibility=hidden)
    
    # Debug Info & Optimization flags based on CMAKE_BUILD_TYPE
    STRING(TOLOWER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE_LOWER)
    IF (CMAKE_BUILD_TYPE_LOWER MATCHES "debug")
        SET (RIB_CFLAGS_INTERNAL ${RIB_CFLAGS_INTERNAL} -g3 -O0)
    ELSEIF (CMAKE_BUILD_TYPE_LOWER STREQUAL "relwithdebinfo")
        SET (RIB_CFLAGS_INTERNAL ${RIB_CFLAGS_INTERNAL} -g3 -O2)
    ELSEIF (CMAKE_BUILD_TYPE_LOWER STREQUAL "minsizerel")
        SET (RIB_CFLAGS_INTERNAL ${RIB_CFLAGS_INTERNAL} -Os)
    ELSE()
        SET (RIB_CFLAGS_INTERNAL ${RIB_CFLAGS_INTERNAL} -O2)
    ENDIF()

    # Use local variable instead of modifying global CMAKE_CXX_FLAGS
    SET (RIB_CXX_FLAGS -std=c++11 -fno-rtti)
ELSEIF(${CMAKE_CXX_COMPILER_ID} STREQUAL "MSVC")
    # Base flags
    SET (RIB_CFLAGS_INTERNAL -fdata-sections -ffunction-sections)
    
    # Debug Info & Optimization flags based on CMAKE_BUILD_TYPE
    STRING(TOLOWER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE_LOWER)
    IF (CMAKE_BUILD_TYPE_LOWER MATCHES "debug")
        SET (RIB_CFLAGS_INTERNAL ${RIB_CFLAGS_INTERNAL} /Zi /Od)
    ELSEIF (CMAKE_BUILD_TYPE_LOWER STREQUAL "relwithdebinfo")
        SET (RIB_CFLAGS_INTERNAL ${RIB_CFLAGS_INTERNAL} /Zi /O2)
    ELSEIF (CMAKE_BUILD_TYPE_LOWER STREQUAL "minsizerel")
        SET (RIB_CFLAGS_INTERNAL ${RIB_CFLAGS_INTERNAL} /O1)
    ELSE()
        SET (RIB_CFLAGS_INTERNAL ${RIB_CFLAGS_INTERNAL} /O2)
    ENDIF()
    # Use local variable instead of modifying global CMAKE_CXX_FLAGS
    SET (RIB_CXX_FLAGS /std:c++17 /fp:strict /Zc:__cplusplus /EHs /source-charset:utf-8 /MP /D_CRT_SECURE_NO_WARNINGS /D_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING /wd4244 /wd4267 /wd4805 /wd4018 /wd4172 /wd4146)
ELSE()
    MESSAGE (FATAL_ERROR "unsupported target")
ENDIF()

ADD_LIBRARY(runtime-icu-binder-static STATIC ${RIB_SRC})
SET_TARGET_PROPERTIES(runtime-icu-binder-static PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Get CFLAGS from environment and convert to CMake list (like walrus)
SET (RIB_CFLAGS_FROM_ENV $ENV{CFLAGS})
SEPARATE_ARGUMENTS(RIB_CFLAGS_FROM_ENV NATIVE_COMMAND)

# Build compile flags as a proper CMake list
SET (RIB_ALL_COMPILE_FLAGS ${RIB_CFLAGS_INTERNAL} ${RIB_CXX_FLAGS} ${RIB_CFLAGS_FROM_ENV})
IF (RIB_CFLAGS_FROM_EXTERNAL)
    IF (NOT RIB_CFLAGS_FROM_EXTERNAL MATCHES ";")
        SEPARATE_ARGUMENTS(RIB_CFLAGS_FROM_EXTERNAL NATIVE_COMMAND)
    ENDIF()
    LIST(APPEND RIB_ALL_COMPILE_FLAGS ${RIB_CFLAGS_FROM_EXTERNAL})
ENDIF()

# Use target_compile_options for proper flag handling (like walrus)
TARGET_COMPILE_OPTIONS(runtime-icu-binder-static PRIVATE ${RIB_ALL_COMPILE_FLAGS})

# Propagate dl library dependency to dependent targets (e.g. escargot target)
# using Modern CMake INTERFACE linking. This replaces the problematic global link_libraries(-ldl).
IF (${CMAKE_CXX_COMPILER_ID} MATCHES "GNU|Clang|AppleClang" AND CMAKE_DL_LIBS)
    TARGET_LINK_LIBRARIES(runtime-icu-binder-static INTERFACE ${CMAKE_DL_LIBS})
ENDIF()
