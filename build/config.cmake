CMAKE_MINIMUM_REQUIRED (VERSION 2.8)

# CONFIGURATION
SET (ESCARGOT_CXXFLAGS_CONFIG)
SET (ESCARGOT_LDFLAGS_CONFIG)

IF (${HOST} STREQUAL "linux")
    SET (CMAKE_C_COMPILER gcc)
    SET (CMAKE_CXX_COMPILER g++)
    SET (ARFLAGS)

ELSEIF (${HOST} STREQUAL "android")
    SET (COMPILER_PREFIX "arm-linux-androideabi")
    SET (CMAKE_C_COMPILER ${ANDROID_NDK_STANDALONE}/bin/${COMPILER_PREFIX}-gcc)
    SET (CMAKE_CXX_COMPILER ${ANDROID_NDK_STANDALONE}/bin/${COMPILER_PREFIX}-g++)
    SET (LINK ${ANDROID_NDK_STANDALONE}/bin/${COMPILER_PREFIX}-g++)
    SET (LD ${ANDROID_NDK_STANDALONE}/bin/${COMPILER_PREFIX}-ld)
    SET (AR ${ANDROID_NDK_STANDALONE}/bin/${COMPILER_PREFIX}-ar)

    SET (ESCARGOT_CXXFLAGS_CONFIG "--sysroot=${ANDROID_NDK_STANDALONE}/sysroot")
    SET (ESCARGOT_LDFLAGS_CONFIG  "--sysroot=${ANDROID_NDK_STANDALONE}/sysroot")
    SET (ARFLAGS)

ELSEIF (${HOST} STREQUAL "tizen_obs")
    SET (CMAKE_C_COMPILER gcc)
    SET (CMAKE_CXX_COMPILER g++)
    IF ("${LTO}" EQUAL 1)
#TODO
        SET (ARFLAGS "--plugin=/usr/lib/bfd-plugins/liblto_plugin.so")
    ELSE()
        SET (ARFLAGS)
    ENDIF()

ELSEIF (${HOST} MATCHES "tizen")
    IF (NOT DEFINED ${TIZEN_SDK_HOME})
        MESSAGE (FATAL_ERROR "TIZEN_SDK_HOME must be set")
    ENDIF()

    IF (${HOST} MATCHES "mobile")
        IF (${ARCH} STREQUAL "arm")
            SET (TIZEN_SYSROOT "${TIZEN_SDK_HOME}/platforms/tizen-${VERSION}/mobile/rootstraps/mobile-${VERSION}-device.core")
        ELSEIF (${ARCH} STREQUAL "i386")
            SET (TIZEN_SYSROOT "${TIZEN_SDK_HOME}/platforms/tizen-${VERSION}/mobile/rootstraps/mobile-${VERSION}-emulator.core")
        ENDIF()
    ELSEIF (${HOST} MATCHES "wearable")
        IF (${ARCH} STREQUAL "arm")
            SET (TIZEN_SYSROOT "${TIZEN_SDK_HOME}/platforms/tizen-${VERSION}/wearable/rootstraps/wearable-${VERSION}-device.core")
        ELSEIF (${ARCH} STREQUAL "i386")
            SET (TIZEN_SYSROOT "${TIZEN_SDK_HOME}/platforms/tizen-${VERSION}/wearable/rootstraps/wearable-${VERSION}-emulator.core")
        ENDIF()
    ENDIF()

    SET (COMPILER_PREFIX "${ARCH}-linux-gnueabi")
    SET (CMAKE_C_COMPILER ${TIZEN_SDK_HOME}/tools/${COMPILER_PREFIX}-gcc-4.6/bin/${COMPILER_PREFIX}-gcc)
    SET (CMAKE_CXX_COMPILER ${TIZEN_SDK_HOME}/tools/${COMPILER_PREFIX}-gcc-4.6/bin/${COMPILER_PREFIX}-g++)
    SET (LINK ${TIZEN_SDK_HOME}/tools/${COMPILER_PREFIX}-gcc-4.6/bin/${COMPILER_PREFIX}-g++)
    SET (LD ${TIZEN_SDK_HOME}/tools/${COMPILER_PREFIX}-gcc-4.6/bin/${COMPILER_PREFIX}-ld)
    IF (${LTO} EQUAL 1)
      SET (AR ${TIZEN_SDK_HOME}/tools/${COMPILER_PREFIX}-gcc-4.6/bin/${COMPILER_PREFIX}-gcc-ar)
#TODO
      SET (ARFLAGS "--plugin=${TIZEN_SDK_HOME}/tools/${COMPILER_PREFIX}-gcc-4.6/libexec/gcc/${COMPILER_PREFIX}/4.6.4/liblto_plugin.so")
    ELSE()
      SET (AR ${TIZEN_SDK_HOME}/tools/${COMPILER_PREFIX}-gcc-4.6/bin/${COMPILER_PREFIX}-ar)
      SET (ARFLAGS)
    ENDIF()

    SET (ESCARGOT_CXXFLAGS_CONFIG "--sysroot=${TIZEN_SYSROOT}")
    SET (ESCARGOT_LDFLAGS_CONFIG "--sysroot=${TIZEN_SYSROOT}")
ENDIF()


# PATH
SET (ESCARGOT_ROOT ${CMAKE_SOURCE_DIR})
SET (THIRD_PARTY_ROOT ${ESCARGOT_ROOT}/third_party)
SET (GCUTIL_ROOT ${THIRD_PARTY_ROOT}/GCutil)

SET (OUTDIR ${ESCARGOT_ROOT}/out/${HOST}/${ARCH}/${TYPE}/${MODE})

# TARGET NAME
SET (ESCARGOT_TARGET escargot)
