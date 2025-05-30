CMAKE_MINIMUM_REQUIRED (VERSION 2.8.12 FATAL_ERROR)

PROJECT (ESCARGOT)

# CONFIGURATION
SET (CMAKE_VERBOSE_MAKEFILE true)
SET (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

MESSAGE(STATUS "CMAKE_SYSTEM_NAME: " ${CMAKE_SYSTEM_NAME})
MESSAGE(STATUS "CMAKE_SYSTEM_PROCESSOR: " ${CMAKE_SYSTEM_PROCESSOR})

IF (NOT DEFINED ESCARGOT_ARCH)
    IF (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "X86" OR ${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86"
        OR ${CMAKE_SYSTEM_PROCESSOR} STREQUAL "i386" OR ${CMAKE_SYSTEM_PROCESSOR} STREQUAL "i686")
        SET(ESCARGOT_ARCH "x86")
    ELSEIF (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "AMD64" OR ${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86_64"
            OR ${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x64" OR ${CMAKE_SYSTEM_PROCESSOR} STREQUAL "EM64T")
        SET(ESCARGOT_ARCH "x64")
    ELSEIF (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "arm" OR ${CMAKE_SYSTEM_PROCESSOR} STREQUAL "arm32"
            OR ${CMAKE_SYSTEM_PROCESSOR} STREQUAL "armv7l")
        SET(ESCARGOT_ARCH "arm")
    ELSEIF (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "arm64" OR ${CMAKE_SYSTEM_PROCESSOR} STREQUAL "aarch64")
        SET(ESCARGOT_ARCH "aarch64")
    ELSEIF (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "riscv64")
        SET(ESCARGOT_ARCH "riscv64")
    ENDIF()
ENDIF()

IF (NOT DEFINED ESCARGOT_HOST)
    IF (${CMAKE_SYSTEM_NAME} STREQUAL "Windows" OR ${CMAKE_SYSTEM_NAME} STREQUAL "WindowsStore")
        SET(ESCARGOT_HOST "windows")
    ELSEIF (${CMAKE_SYSTEM_NAME} STREQUAL "Android")
        SET(ESCARGOT_HOST "android")
    ELSEIF (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
        SET(ESCARGOT_HOST "darwin")
    ELSE()
        SET(ESCARGOT_HOST "linux")
    ENDIF()
ENDIF()

IF (NOT DEFINED ESCARGOT_MODE)
    SET (ESCARGOT_MODE "release")
ENDIF()
IF (NOT DEFINED ESCARGOT_OUTPUT)
    SET (ESCARGOT_OUTPUT "shell")
ENDIF()
IF (NOT DEFINED ESCARGOT_THREADING)
    SET (ESCARGOT_THREADING ON)
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
MESSAGE(STATUS "Escargot Host: " ${ESCARGOT_HOST})
MESSAGE(STATUS "Escargot Mode: " ${ESCARGOT_MODE})
MESSAGE(STATUS "Escargot Threading: " ${ESCARGOT_THREADING})
MESSAGE(STATUS "Escargot TLS optimize(offset): " ${ESCARGOT_TLS_ACCESS_BY_ADDRESS})
MESSAGE(STATUS "Escargot TLS optimize(pthread_key): " ${ESCARGOT_TLS_ACCESS_BY_PTHREAD_KEY})
MESSAGE(STATUS "Escargot Output: " ${ESCARGOT_OUTPUT})