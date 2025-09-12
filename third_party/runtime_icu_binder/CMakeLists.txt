PROJECT(runtime-icu-binder)
CMAKE_MINIMUM_REQUIRED(VERSION 2.8.12 FATAL_ERROR)

FILE(GLOB RIB_SRC *.cpp)

IF (${CMAKE_CXX_COMPILER_ID} MATCHES "GNU|Clang|AppleClang") #include GNU, Clang and AppleClang all
    SET (RIB_CFLAGS_INTERNAL -g3 -fdata-sections -ffunction-sections -fvisibility=hidden)
    IF (${RIB_MODE} STREQUAL "debug")
        SET (RIB_CFLAGS_INTERNAL ${RIB_CFLAGS_INTERNAL} -O0)
    ELSEIF (${RIB_MODE} STREQUAL "release")
        SET (RIB_CFLAGS_INTERNAL ${RIB_CFLAGS_INTERNAL} -O2)
    ENDIF()

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fno-rtti")

    link_libraries(-ldl)
ELSEIF(${CMAKE_CXX_COMPILER_ID} STREQUAL "MSVC")
    IF (${RIB_MODE} STREQUAL "debug")
        SET (RIB_CFLAGS_INTERNAL ${RIB_CFLAGS_INTERNAL} /O0)
    ELSEIF (${RIB_MODE} STREQUAL "release")
        SET (RIB_CFLAGS_INTERNAL ${RIB_CFLAGS_INTERNAL} /O2)
    ENDIF()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++17 /fp:strict /Zc:__cplusplus /EHs /source-charset:utf-8 /MP /D_CRT_SECURE_NO_WARNINGS /D_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING /wd4244 /wd4267 /wd4805 /wd4018 /wd4172 /wd4146")
ELSE()
    MESSAGE (FATAL_ERROR "unsupported target")
ENDIF()

add_compile_options(${RIB_CFLAGS_INTERNAL})
add_compile_options(${RIB_CFLAGS})
SET (RIB_CFLAGS_FROM_ENV $ENV{CFLAGS})
SEPARATE_ARGUMENTS(RIB_CFLAGS_FROM_ENV)
# add ${RIB_CFLAGS_FROM_EXTERNAL} at the end
add_compile_options(${RIB_CFLAGS_FROM_ENV} ${RIB_CFLAGS_FROM_EXTERNAL})

ADD_LIBRARY(runtime-icu-binder-static STATIC ${RIB_SRC})
