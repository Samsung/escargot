cmake_minimum_required(VERSION 3.18.1)

project(escargot-jni)

option(UNDER_NDK "Build under the Android NDK" ON)
option(ENABLE_SHELL "Enable shell" OFF)

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fexceptions")

if (NOT UNDER_NDK)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libstdc++ -g3")
endif ()

SET (ESCARGOT_ARCH ${ANDROID_SYSROOT_ABI})
# STRING(TOLOWER ${CMAKE_BUILD_TYPE} ESCARGOT_MODE)
SET (ESCARGOT_MODE "release")
SET (ESCARGOT_THREADING ON)
if (ENABLE_SHELL)
    ADD_DEFINITIONS(-DESCARGOT_ENABLE_TEST)
endif ()
ADD_SUBDIRECTORY (${CMAKE_CURRENT_SOURCE_DIR}/../../../../../../ escargot)

ADD_LIBRARY (escargot-jni SHARED escargot.cpp)
SET(LOG_LIBRARY "")
if (NOT UNDER_NDK)
    ADD_DEFINITIONS(-DNDEBUG)
endif ()

if (UNDER_NDK)
    FIND_LIBRARY(LOG_LIBRARY log)
else (UNDER_NDK)
    FIND_PACKAGE(JNI REQUIRED)
    INCLUDE_DIRECTORIES(${JNI_INCLUDE_DIRS})
endif ()

TARGET_LINK_LIBRARIES (escargot-jni PRIVATE escargot ${LOG_LIBRARY})

if (ENABLE_SHELL)
    ADD_EXECUTABLE(escargot-shell ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../../src/shell/Shell.cpp)
    TARGET_INCLUDE_DIRECTORIES(escargot-shell PRIVATE ADD_SUBDIRECTORY
        (${CMAKE_CURRENT_SOURCE_DIR}/../../../../../../third_party/GCutil/)
        (${CMAKE_CURRENT_SOURCE_DIR}/../../../../../../third_party/GCutil/bdwgc/include)
        (${CMAKE_CURRENT_SOURCE_DIR}/../../../../../../src))
    TARGET_LINK_LIBRARIES (escargot-shell PRIVATE escargot ${LOG_LIBRARY})

    ADD_EXECUTABLE(test262-runner ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../../tools/test/test262/test-data-runner.cpp)
    TARGET_COMPILE_OPTIONS(test262-runner PRIVATE -std=c++11)
endif ()