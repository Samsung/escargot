# default set of each flag
SET (ESCARGOT_CXXFLAGS)
SET (ESCARGOT_CXXFLAGS_DEBUG)
SET (ESCARGOT_CXXFLAGS_RELEASE)
SET (ESCARGOT_LDFLAGS)
SET (ESCARGOT_DEFINITIONS)
SET (ESCARGOT_THIRDPARTY_CFLAGS)

SET (ESCARGOT_BUILD_32BIT OFF)
SET (ESCARGOT_BUILD_64BIT OFF)
SET (ESCARGOT_BUILD_64BIT_LARGE OFF)

# clang-cl defines ${CMAKE_CXX_COMPILER_ID} "Clang" and ${CMAKE_CXX_COMPILER_FRONTEND_VARIANT} "MSVC"
SET (COMPILER_CLANG_CL OFF)
IF (${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
    IF (DEFINED CMAKE_CXX_COMPILER_FRONTEND_VARIANT)
        IF ("${CMAKE_CXX_COMPILER_FRONTEND_VARIANT}" STREQUAL "MSVC")
            SET (COMPILER_CLANG_CL ON)
        ENDIF()
    ENDIF()
ENDIF()

# Default options per compiler
IF (${CMAKE_CXX_COMPILER_ID} STREQUAL "MSVC" OR ${COMPILER_CLANG_CL})
    SET (ESCARGOT_CXXFLAGS /std:c++17 /fp:strict /Zc:__cplusplus /EHs /source-charset:utf-8 /MP /D_CRT_SECURE_NO_WARNINGS /DGC_NOT_DLL /D_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING /wd4244 /wd4267 /wd4805 /wd4018 /wd4172)
    SET (ESCARGOT_CXXFLAGS_RELEASE /O2 /Oy-)
    SET (ESCARGOT_THIRDPARTY_CFLAGS /D_CRT_SECURE_NO_WARNINGS /DGC_NOT_DLL /Oy- /wd4146 /EHs)
    IF (${COMPILER_CLANG_CL})
        SET (ESCARGOT_CXXFLAGS ${ESCARGOT_CXXFLAGS} /EHs -Wno-invalid-offsetof -Wno-inline-new-delete -fintegrated-cc1)
    ENDIF()
ELSEIF (${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
    SET (ESCARGOT_CXXFLAGS
        ${ESCARGOT_CXXFLAGS}
        -std=c++11 -g3
        -fno-rtti
        -fno-math-errno
        -fdata-sections -ffunction-sections
        -fno-omit-frame-pointer
        -fvisibility=hidden
        -frounding-math -fsignaling-nans
        -Wno-unused-parameter
        -Wno-type-limits -Wno-unused-result -Wno-unused-variable -Wno-invalid-offsetof
        -Wno-unused-but-set-variable -Wno-unused-but-set-parameter
        -Wno-deprecated-declarations -Wno-unused-function
    )
    IF (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 9)
        SET (ESCARGOT_CXXFLAGS ${ESCARGOT_CXXFLAGS} -Wno-attributes -Wno-class-memaccess -Wno-deprecated-copy -Wno-cast-function-type -Wno-stringop-truncation -Wno-pessimizing-move -Wno-mismatched-new-delete -Wno-overloaded-virtual -Wno-dangling-pointer)
    endif()
    SET (ESCARGOT_CXXFLAGS_DEBUG -O0 -Wall -Wextra -Werror)
    SET (ESCARGOT_CXXFLAGS_RELEASE -O2 -fno-stack-protector -fno-omit-frame-pointer)
    SET (ESCARGOT_THIRDPARTY_CFLAGS -w -g3 -fdata-sections -ffunction-sections -fno-omit-frame-pointer -fvisibility=hidden)
ELSEIF (${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
    SET (ESCARGOT_CXXFLAGS
        ${ESCARGOT_CXXFLAGS}
        -std=c++11 -g3
        -fno-rtti
        -fno-math-errno
        -fdata-sections -ffunction-sections
        -fno-omit-frame-pointer
        -fvisibility=hidden
        -fno-fast-math -fno-unsafe-math-optimizations -fdenormal-fp-math=ieee
        -Wno-type-limits -Wno-unused-result -Wno-unused-variable -Wno-invalid-offsetof -Wno-unused-function
        -Wno-deprecated-declarations -Wno-unsupported-floating-point-opt -Wno-parentheses-equality -Wno-dynamic-class-memaccess -Wno-deprecated-register
        -Wno-expansion-to-defined -Wno-return-type -Wno-overloaded-virtual -Wno-unused-private-field -Wno-deprecated-copy -Wno-atomic-alignment
        -Wno-ambiguous-reversed-operator -Wno-deprecated-enum-enum-conversion -Wno-deprecated-enum-float-conversion -Wno-braced-scalar-init -Wno-unused-parameter
    )
    SET (ESCARGOT_CXXFLAGS_DEBUG -O0 -Wall -Wextra -Werror)
    SET (ESCARGOT_CXXFLAGS_RELEASE -O2 -fno-stack-protector -fno-omit-frame-pointer)
    SET (ESCARGOT_THIRDPARTY_CFLAGS -w -g3 -fdata-sections -ffunction-sections -fno-omit-frame-pointer -fvisibility=hidden)
ELSE()
    MESSAGE (FATAL_ERROR ${CMAKE_CXX_COMPILER_ID} " is Unsupported Compiler")
ENDIF()

# Default options per host
IF (${ESCARGOT_HOST} STREQUAL "linux")
    FIND_PACKAGE (PkgConfig REQUIRED)
    IF (ESCARGOT_THREADING)
        SET (ESCARGOT_LIBRARIES ${ESCARGOT_LIBRARIES} atomic)
    ENDIF()
    # default set of LDFLAGS
    SET (ESCARGOT_LDFLAGS -lpthread -lrt -Wl,--gc-sections)
    IF ((${ESCARGOT_ARCH} STREQUAL "x64") OR (${ESCARGOT_ARCH} STREQUAL "x86_64"))
        SET (ESCARGOT_BUILD_64BIT ON)
    ELSEIF ((${ESCARGOT_ARCH} STREQUAL "x86") OR (${ESCARGOT_ARCH} STREQUAL "i686"))
        SET (ESCARGOT_BUILD_32BIT ON)
        SET (ESCARGOT_CXXFLAGS ${ESCARGOT_CXXFLAGS} -m32 -mfpmath=sse -msse -msse2)
        SET (ESCARGOT_LDFLAGS ${ESCARGOT_LDFLAGS} -m32)
        SET (ESCARGOT_THIRDPARTY_CFLAGS ${ESCARGOT_THIRDPARTY_CFLAGS} -m32)
    ELSEIF (${ESCARGOT_ARCH} STREQUAL "arm")
        SET (ESCARGOT_BUILD_32BIT ON)
    ELSEIF (${ESCARGOT_ARCH} STREQUAL "aarch64")
        SET (ESCARGOT_BUILD_64BIT ON)
    ELSEIF (${ESCARGOT_ARCH} STREQUAL "riscv64")
        SET (ESCARGOT_BUILD_64BIT ON)
        SET (ESCARGOT_BUILD_64BIT_LARGE ON)
    ELSE()
        MESSAGE (FATAL_ERROR ${ESCARGOT_ARCH} " is unsupported")
    ENDIF()
ELSEIF (${ESCARGOT_HOST} STREQUAL "tizen" OR ${ESCARGOT_HOST} STREQUAL "tizen_obs")
    FIND_PACKAGE (PkgConfig REQUIRED)
    # default set of LDFLAGS
    SET (ESCARGOT_LDFLAGS -lpthread -lrt -Wl,--gc-sections)
    IF (ESCARGOT_THREADING)
        SET (ESCARGOT_LIBRARIES ${ESCARGOT_LIBRARIES} atomic)
    ENDIF()
    SET (ESCARGOT_DEFINITIONS -DESCARGOT_TIZEN)
    IF ((${ESCARGOT_ARCH} STREQUAL "x64") OR (${ESCARGOT_ARCH} STREQUAL "x86_64"))
        SET (ESCARGOT_BUILD_64BIT ON)
    ELSEIF ((${ESCARGOT_ARCH} STREQUAL "x86") OR (${ESCARGOT_ARCH} STREQUAL "i686"))
        SET (ESCARGOT_BUILD_32BIT ON)
        SET (ESCARGOT_CXXFLAGS ${ESCARGOT_CXXFLAGS} -m32 -mfpmath=sse -msse -msse2)
        SET (ESCARGOT_LDFLAGS ${ESCARGOT_LDFLAGS} -m32)
        SET (ESCARGOT_THIRDPARTY_CFLAGS ${ESCARGOT_THIRDPARTY_CFLAGS} -m32)
    ELSEIF (${ESCARGOT_ARCH} STREQUAL "arm")
        SET (ESCARGOT_BUILD_32BIT ON)
        SET (ESCARGOT_CXXFLAGS_DEBUG -O1)
        SET (ESCARGOT_CXXFLAGS_RELEASE -O2)
    ELSEIF (${ESCARGOT_ARCH} STREQUAL "aarch64")
        SET (ESCARGOT_BUILD_64BIT ON)
        SET (ESCARGOT_BUILD_64BIT_LARGE ON)
    ELSEIF (${ESCARGOT_ARCH} STREQUAL "riscv64")
        SET (ESCARGOT_BUILD_64BIT ON)
        SET (ESCARGOT_BUILD_64BIT_LARGE ON)
    ELSE()
        MESSAGE (FATAL_ERROR ${ESCARGOT_ARCH} " is unsupported")
    ENDIF()
ELSEIF (${ESCARGOT_HOST} STREQUAL "android")
    FIND_PACKAGE (PkgConfig REQUIRED)
    IF (ESCARGOT_THREADING)
        SET (ESCARGOT_LIBRARIES ${ESCARGOT_LIBRARIES} atomic)
    ENDIF()
    SET (ESCARGOT_DEFINITIONS -DANDROID=1 -DESCARGOT_ANDROID=1)
    SET (ESCARGOT_THIRDPARTY_CFLAGS ${ESCARGOT_THIRDPARTY_CFLAGS} -mstackrealign)
    SET (ESCARGOT_CXXFLAGS ${ESCARGOT_CXXFLAGS} -mstackrealign)
    IF (${ESCARGOT_ARCH} STREQUAL "arm")
        SET (ESCARGOT_BUILD_32BIT ON)
        SET (ESCARGOT_LDFLAGS -fPIE -pie -march=armv7-a -Wl,--fix-cortex-a8 -llog -Wl,--gc-sections)
    ELSEIF ((${ESCARGOT_ARCH} STREQUAL "arm64") OR (${ESCARGOT_ARCH} STREQUAL "aarch64"))
        SET (ESCARGOT_BUILD_64BIT ON)
        SET (ESCARGOT_LDFLAGS -fPIE -pie -llog -Wl,--gc-sections)
    ELSEIF (${ESCARGOT_ARCH} STREQUAL "x86")
        SET (ESCARGOT_BUILD_32BIT ON)
        SET (ESCARGOT_CXXFLAGS ${ESCARGOT_CXXFLAGS} -m32 -mfpmath=sse -msse -msse2  -mstackrealign)
        SET (ESCARGOT_LDFLAGS -fPIE -pie -llog -Wl,--gc-sections -m32)
    ELSEIF (${ESCARGOT_ARCH} STREQUAL "x86_64" OR ${ESCARGOT_ARCH} STREQUAL "x64")
        SET (ESCARGOT_BUILD_64BIT ON)
        SET (ESCARGOT_LDFLAGS -fPIE -pie -llog -Wl,--gc-sections)
        # bdwgc android amd64 cannot support keeping back ptrs
        SET (ESCARGOT_THIRDPARTY_CFLAGS ${ESCARGOT_THIRDPARTY_CFLAGS} -UKEEP_BACK_PTRS -USAVE_CALL_COUNT -UDBG_HDRS_ALL)
    ENDIF()
ELSEIF (${ESCARGOT_HOST} STREQUAL "darwin" AND ${ESCARGOT_ARCH} STREQUAL "x64")
    FIND_PACKAGE (PkgConfig REQUIRED)
    SET (ESCARGOT_LDFLAGS -lpthread -Wl,-dead_strip)
    SET (ESCARGOT_BUILD_64BIT_LARGE ON)
    # bdwgc mac cannot support pthread_getattr_np
    SET (ESCARGOT_THIRDPARTY_CFLAGS ${ESCARGOT_THIRDPARTY_CFLAGS} -UHAVE_PTHREAD_GETATTR_NP)
    # dlopen version not working correctly on mac
    IF (NOT DEFINED ESCARGOT_LIBICU_SUPPORT_WITH_DLOPEN)
        SET (ESCARGOT_LIBICU_SUPPORT_WITH_DLOPEN OFF)
    ENDIF()
    # add pkg_config_path icu install path of brew
    SET (ENV{PKG_CONFIG_PATH} "/usr/local/opt/icu4c/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")
ELSEIF (${ESCARGOT_HOST} STREQUAL "windows")
    SET (ESCARGOT_LDFLAGS ${ESCARGOT_LDFLAGS} icu.lib)
    IF ((${ESCARGOT_ARCH} STREQUAL "x64") OR (${ESCARGOT_ARCH} STREQUAL "x86_64"))
        SET (ESCARGOT_BUILD_64BIT ON)
        SET (ESCARGOT_BUILD_64BIT_LARGE ON)
    ELSEIF ((${ESCARGOT_ARCH} STREQUAL "x86") OR (${ESCARGOT_ARCH} STREQUAL "i686"))
        SET (ESCARGOT_BUILD_32BIT ON)
    ELSEIF (${ESCARGOT_ARCH} STREQUAL "arm")
        SET (ESCARGOT_BUILD_32BIT ON)
    ELSEIF (${ESCARGOT_ARCH} STREQUAL "aarch64" OR (${ESCARGOT_ARCH} STREQUAL "arm64"))
        SET (ESCARGOT_BUILD_64BIT ON)
        SET (ESCARGOT_BUILD_64BIT_LARGE ON)
    ELSE()
        MESSAGE (FATAL_ERROR ${ESCARGOT_ARCH} " is unsupported")
    ENDIF()

ELSE()
    MESSAGE (FATAL_ERROR ${ESCARGOT_HOST} " with " ${ESCARGOT_ARCH} " is unsupported")
ENDIF()

IF (ESCARGOT_BUILD_32BIT)
    # 32bit build
    SET (ESCARGOT_DEFINITIONS ${ESCARGOT_DEFINITIONS} -DESCARGOT_32=1)
ELSEIF ((ESCARGOT_BUILD_64BIT_LARGE) OR (ESCARGOT_BUILD_64BIT AND ESCARGOT_BUILD_64BIT_FORCE_LARGE))
    # 64bit build(large)
    SET (ESCARGOT_DEFINITIONS ${ESCARGOT_DEFINITIONS} -DESCARGOT_64=1)
ELSEIF (ESCARGOT_BUILD_64BIT)
    # 64bit build
    SET (ESCARGOT_DEFINITIONS ${ESCARGOT_DEFINITIONS} -DESCARGOT_64=1 -DESCARGOT_USE_32BIT_IN_64BIT)
    SET (ESCARGOT_THIRDPARTY_CFLAGS ${ESCARGOT_THIRDPARTY_CFLAGS} -DESCARGOT_USE_32BIT_IN_64BIT)
ELSE()
    MESSAGE (FATAL_ERROR "unsupported mode")
ENDIF()
