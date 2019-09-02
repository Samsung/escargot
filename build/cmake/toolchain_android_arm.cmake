# Copyright (c) 2019-present Samsung Electronics Co., Ltd
#
#  This library is free software; you can redistribute it and/or
#  modify it under the terms of the GNU Lesser General Public
#  License as published by the Free Software Foundation; either
#  version 2 of the License, or (at your option) any later version.
#
#  This library is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#  Lesser General Public License for more details.
#
#  You should have received a copy of the GNU Lesser General Public
#  License along with this library; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
#  USA

IF (${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
    SET (COMPILER_PREFIX arm-linux-androideabi)
    SET (CMAKE_C_COMPILER ${ANDROID_NDK_STANDALONE}/bin/${COMPILER_PREFIX}-gcc)
    SET (CMAKE_CXX_COMPILER ${ANDROID_NDK_STANDALONE}/bin/${COMPILER_PREFIX}-g++)
    SET (LINK ${ANDROID_NDK_STANDALONE}/bin/${COMPILER_PREFIX}-g++)
    SET (LD ${ANDROID_NDK_STANDALONE}/bin/${COMPILER_PREFIX}-ld)
    SET (AR ${ANDROID_NDK_STANDALONE}/bin/${COMPILER_PREFIX}-ar)
    SET (GC_CONFIGURE_HOST --host=arm-linux-androideabi)
ELSE()
    SET (GC_CONFIGURE_HOST)
ENDIF()

SET (ESCARGOT_CXXFLAGS -fPIE -mthumb -march=armv7-a -mfloat-abi=softfp -mfpu=neon)

SET (ESCARGOT_CXXFLAGS ${ESCARGOT_CXXFLAGS} --sysroot=${ANDROID_NDK_STANDALONE}/sysroot)

SET (ESCARGOT_DEFINITIONS -DANDROID=1 -DESCARGOT_32=1)

SET (ESCARGOT_CXXFLAGS_DEBUG)
SET (ESCARGOT_CXXFLAGS_RELEASE)
SET (ESCARGOT_HOST android)
SET (ESCARGOT_ARCH arm)
SET (ESCARGOT_LDFLAGS -fPIE -pie -march=armv7-a -Wl,--fix-cortex-a8 -llog -Wl,--gc-sections)

SET (ESCARGOT_LDFLAGS ${ESCARGOT_LDFLAGS} --sysroot=${ANDROID_NDK_STANDALONE}/sysroot)

SET (ESCARGOT_LIBRARIES)
SET (ESCARGOT_INCDIRS)

SET (GC_CFLAGS_ARCH -march=armv7-a -mthumb -finline-limit=64)
SET (GC_LDFLAGS_ARCH)
