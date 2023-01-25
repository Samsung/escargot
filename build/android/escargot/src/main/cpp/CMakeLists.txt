cmake_minimum_required(VERSION 3.18.1)

project(escargot-android)

SET (ESCARGOT_HOST "android")
SET (ESCARGOT_ARCH ${ANDROID_SYSROOT_ABI})
# STRING(TOLOWER ${CMAKE_BUILD_TYPE} ESCARGOT_MODE)
SET (ESCARGOT_MODE "release")
SET (ESCARGOT_OUTPUT "shared_lib")
SET (ESCARGOT_THREADING ON)
ADD_SUBDIRECTORY (${CMAKE_CURRENT_SOURCE_DIR}/../../../../../../ escargot)

ADD_LIBRARY (escargot-android SHARED escargot.cpp)
TARGET_LINK_LIBRARIES (escargot-android PRIVATE escargot)