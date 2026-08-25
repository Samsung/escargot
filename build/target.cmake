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
IF (NOT DEFINED ESCARGOT_BUILD_64BIT_FORCE_LARGE)
    SET (ESCARGOT_BUILD_64BIT_FORCE_LARGE ON)
ENDIF()
option(ESCARGOT_BUILD_64BIT_FORCE_LARGE "On 64-bit targets, force full 64-bit pointers instead of 32-bit-in-64-bit compression" ${ESCARGOT_BUILD_64BIT_FORCE_LARGE})
IF (ESCARGOT_ASAN)
    SET (ESCARGOT_BUILD_64BIT_LARGE ON)
ENDIF()

# clang-cl defines ${CMAKE_CXX_COMPILER_ID} "Clang" and ${CMAKE_CXX_COMPILER_FRONTEND_VARIANT} "MSVC"
SET (COMPILER_CLANG_CL OFF)
IF (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    IF (DEFINED CMAKE_CXX_COMPILER_FRONTEND_VARIANT)
        IF ("${CMAKE_CXX_COMPILER_FRONTEND_VARIANT}" STREQUAL "MSVC")
            SET (COMPILER_CLANG_CL ON)
        ENDIF()
    ENDIF()
ENDIF()

# Default options per compiler
IF (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" OR COMPILER_CLANG_CL)
    SET (ESCARGOT_COMPILER_ID "MSVC")
    SET (ESCARGOT_CXXFLAGS /std:c++17 /fp:strict /Zc:__cplusplus /EHs /source-charset:utf-8 /MP /D_CRT_SECURE_NO_WARNINGS /DGC_NOT_DLL /D_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING /wd4244 /wd4267 /wd4805 /wd4018 /wd4172 /wd4146)
    SET (ESCARGOT_CXXFLAGS_RELEASE /O2 /Oy-)
    SET (ESCARGOT_THIRDPARTY_CFLAGS /D_CRT_SECURE_NO_WARNINGS /DGC_NOT_DLL /Oy- /wd4146 /EHs)
    IF (COMPILER_CLANG_CL)
        SET (ESCARGOT_CXXFLAGS ${ESCARGOT_CXXFLAGS} /EHs -Wno-invalid-offsetof -Wno-inline-new-delete -fintegrated-cc1)
    ENDIF()
    IF (ESCARGOT_SMALL_CONFIG)
        SET (ESCARGOT_CXXFLAGS_RELEASE ${ESCARGOT_CXXFLAGS_RELEASE} /Os)
    ENDIF()
ELSEIF (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    SET (ESCARGOT_COMPILER_ID "GCC")
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

    IF (NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 16)
        SET (ESCARGOT_CXXFLAGS ${ESCARGOT_CXXFLAGS} -Wno-maybe-uninitialized)
    ENDIF()

    IF (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 9)
        SET (ESCARGOT_CXXFLAGS ${ESCARGOT_CXXFLAGS} -Wno-attributes -Wno-class-memaccess -Wno-deprecated-copy -Wno-cast-function-type -Wno-stringop-truncation -Wno-pessimizing-move -Wno-mismatched-new-delete -Wno-overloaded-virtual -Wno-dangling-pointer)
    endif()
    SET (ESCARGOT_CXXFLAGS_DEBUG -O0 -Wall -Wextra -Werror)
    SET (ESCARGOT_CXXFLAGS_RELEASE -O2 -fno-stack-protector -fno-omit-frame-pointer)
    IF (ESCARGOT_SMALL_CONFIG)
        IF (NOT CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 9)
            # BUG?) -Os option has unknown memory conflicts (might be related with gcc version)
            # enable this option only for old gcc version
            SET (ESCARGOT_CXXFLAGS_RELEASE ${ESCARGOT_CXXFLAGS_RELEASE} -Os)
        ENDIF()
    ENDIF()
    SET (ESCARGOT_THIRDPARTY_CFLAGS -w -g3 -fdata-sections -ffunction-sections -fno-omit-frame-pointer -fvisibility=hidden)
ELSEIF (CMAKE_CXX_COMPILER_ID MATCHES  "Clang") #include Clang and AppleClang both
    SET (ESCARGOT_COMPILER_ID "CLANG")
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
        -Wno-deprecated-declarations -Wno-parentheses-equality -Wno-dynamic-class-memaccess -Wno-deprecated-register
        -Wno-expansion-to-defined -Wno-return-type -Wno-overloaded-virtual -Wno-unused-private-field -Wno-deprecated-copy -Wno-atomic-alignment
        -Wno-ambiguous-reversed-operator -Wno-deprecated-enum-enum-conversion -Wno-deprecated-enum-float-conversion -Wno-braced-scalar-init -Wno-unused-parameter -Wno-deprecated-literal-operator -Wno-cast-function-type-mismatch
        -Wno-unknown-warning-option
    )
    IF (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 10)
        # this feature supported after clang version 11
        SET (ESCARGOT_CXXFLAGS ${ESCARGOT_CXXFLAGS} -Wno-unsupported-floating-point-opt)
    endif()
    IF (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 19)
        # this feature supported after clang version 20
        SET (ESCARGOT_CXXFLAGS ${ESCARGOT_CXXFLAGS} -Wno-invalid-specialization)
    endif()
    IF (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 20)
        # this feature supported after clang version 21
        SET (ESCARGOT_CXXFLAGS ${ESCARGOT_CXXFLAGS} -Wno-character-conversion)
    endif()
    SET (ESCARGOT_CXXFLAGS_DEBUG -O0 -Wall -Wextra -Werror)
    SET (ESCARGOT_CXXFLAGS_RELEASE -O2 -fno-stack-protector -fno-omit-frame-pointer)
    IF (ESCARGOT_SMALL_CONFIG)
        SET (ESCARGOT_CXXFLAGS_RELEASE ${ESCARGOT_CXXFLAGS_RELEASE} -Os)
    ENDIF()
    SET (ESCARGOT_THIRDPARTY_CFLAGS -w -g3 -fdata-sections -ffunction-sections -fno-omit-frame-pointer -fvisibility=hidden)
ELSE()
    MESSAGE (FATAL_ERROR ${CMAKE_CXX_COMPILER_ID} " is Unsupported Compiler")
ENDIF()

# Default options per host
IF (ESCARGOT_HOST STREQUAL "linux")
    FIND_PACKAGE (PkgConfig REQUIRED)
    IF (ESCARGOT_THREADING)
        SET (ESCARGOT_LIBRARIES ${ESCARGOT_LIBRARIES} atomic)
    ENDIF()
    # default set of LDFLAGS
    SET (ESCARGOT_LDFLAGS -lpthread -lrt -Wl,--gc-sections)
    IF ((ESCARGOT_ARCH STREQUAL "x64") OR (ESCARGOT_ARCH STREQUAL "x86_64"))
        SET (ESCARGOT_BUILD_64BIT ON)
    ELSEIF ((ESCARGOT_ARCH STREQUAL "x86") OR (ESCARGOT_ARCH STREQUAL "i686"))
        SET (ESCARGOT_BUILD_32BIT ON)
        SET (ESCARGOT_CXXFLAGS ${ESCARGOT_CXXFLAGS} -m32 -mfpmath=sse -msse -msse2)
        SET (ESCARGOT_LDFLAGS ${ESCARGOT_LDFLAGS} -m32)
        SET (ESCARGOT_THIRDPARTY_CFLAGS ${ESCARGOT_THIRDPARTY_CFLAGS} -m32)
    ELSEIF (ESCARGOT_ARCH STREQUAL "arm")
        SET (ESCARGOT_BUILD_32BIT ON)
    ELSEIF (ESCARGOT_ARCH STREQUAL "aarch64")
        SET (ESCARGOT_BUILD_64BIT ON)
    ELSEIF (ESCARGOT_ARCH STREQUAL "riscv64")
        SET (ESCARGOT_BUILD_64BIT ON)
        SET (ESCARGOT_BUILD_64BIT_LARGE ON)
    ELSE()
        MESSAGE (FATAL_ERROR ${ESCARGOT_ARCH} " is unsupported")
    ENDIF()
ELSEIF (ESCARGOT_HOST STREQUAL "tizen" OR ESCARGOT_HOST STREQUAL "tizen_obs")
    FIND_PACKAGE (PkgConfig REQUIRED)
    # default set of LDFLAGS
    SET (ESCARGOT_LDFLAGS -lpthread -lrt -Wl,--gc-sections)
    IF (ESCARGOT_THREADING)
        SET (ESCARGOT_LIBRARIES ${ESCARGOT_LIBRARIES} atomic)
    ENDIF()
    SET (ESCARGOT_DEFINITIONS -DESCARGOT_TIZEN)
    IF ((ESCARGOT_ARCH STREQUAL "x64") OR (ESCARGOT_ARCH STREQUAL "x86_64"))
        SET (ESCARGOT_BUILD_64BIT ON)
    ELSEIF ((ESCARGOT_ARCH STREQUAL "x86") OR (ESCARGOT_ARCH STREQUAL "i686"))
        SET (ESCARGOT_BUILD_32BIT ON)
        SET (ESCARGOT_CXXFLAGS ${ESCARGOT_CXXFLAGS} -m32 -mfpmath=sse -msse -msse2)
        SET (ESCARGOT_LDFLAGS ${ESCARGOT_LDFLAGS} -m32)
        SET (ESCARGOT_THIRDPARTY_CFLAGS ${ESCARGOT_THIRDPARTY_CFLAGS} -m32)
    ELSEIF (ESCARGOT_ARCH STREQUAL "arm")
        SET (ESCARGOT_BUILD_32BIT ON)
        SET (ESCARGOT_CXXFLAGS_DEBUG -O1)
        SET (ESCARGOT_CXXFLAGS_RELEASE -O2)
    ELSEIF (ESCARGOT_ARCH STREQUAL "aarch64")
        SET (ESCARGOT_BUILD_64BIT ON)
        SET (ESCARGOT_BUILD_64BIT_LARGE ON)
    ELSEIF (ESCARGOT_ARCH STREQUAL "riscv64")
        SET (ESCARGOT_BUILD_64BIT ON)
        SET (ESCARGOT_BUILD_64BIT_LARGE ON)
    ELSE()
        MESSAGE (FATAL_ERROR ${ESCARGOT_ARCH} " is unsupported")
    ENDIF()
ELSEIF (ESCARGOT_HOST STREQUAL "android")
    FIND_PACKAGE (PkgConfig REQUIRED)
    IF (ESCARGOT_THREADING)
        SET (ESCARGOT_LIBRARIES ${ESCARGOT_LIBRARIES} atomic)
    ENDIF()
    SET (ESCARGOT_DEFINITIONS -DANDROID=1 -DESCARGOT_ANDROID=1)
    SET (ESCARGOT_THIRDPARTY_CFLAGS ${ESCARGOT_THIRDPARTY_CFLAGS} -mstackrealign)
    SET (ESCARGOT_CXXFLAGS ${ESCARGOT_CXXFLAGS} -mstackrealign)
    IF (ESCARGOT_ARCH STREQUAL "arm")
        SET (ESCARGOT_BUILD_32BIT ON)
        SET (ESCARGOT_LDFLAGS -fPIE -pie -march=armv7-a -Wl,--fix-cortex-a8 -llog -Wl,--gc-sections)
    ELSEIF ((ESCARGOT_ARCH STREQUAL "arm64") OR (ESCARGOT_ARCH STREQUAL "aarch64"))
        SET (ESCARGOT_BUILD_64BIT ON)
        SET (ESCARGOT_LDFLAGS -fPIE -pie -llog -Wl,--gc-sections)
    ELSEIF (ESCARGOT_ARCH STREQUAL "x86")
        SET (ESCARGOT_BUILD_32BIT ON)
        SET (ESCARGOT_CXXFLAGS ${ESCARGOT_CXXFLAGS} -m32 -mfpmath=sse -msse -msse2  -mstackrealign)
        SET (ESCARGOT_LDFLAGS -fPIE -pie -llog -Wl,--gc-sections -m32)
    ELSEIF (ESCARGOT_ARCH STREQUAL "x86_64" OR ESCARGOT_ARCH STREQUAL "x64")
        SET (ESCARGOT_BUILD_64BIT ON)
        SET (ESCARGOT_LDFLAGS -fPIE -pie -llog -Wl,--gc-sections)
        # bdwgc android amd64 cannot support keeping back ptrs
        SET (ESCARGOT_THIRDPARTY_CFLAGS ${ESCARGOT_THIRDPARTY_CFLAGS} -UKEEP_BACK_PTRS -USAVE_CALL_COUNT -UDBG_HDRS_ALL)
    ENDIF()
ELSEIF (ESCARGOT_HOST STREQUAL "darwin")
    IF (NOT ESCARGOT_LIBICU_SUPPORT_VENDORED)
        # Only needed for the non-vendored, pkg-config-located system/Homebrew
        # ICU path (build/config.cmake's PKG_CHECK_MODULES(ICU ...) call) --
        # the vendored path (default now, see build/VendoredICU.cmake) builds
        # its own static ICU from the third_party/icu submodule instead.
        FIND_PACKAGE (PkgConfig REQUIRED)
    ENDIF()
    IF ((NOT ESCARGOT_ARCH STREQUAL "x64") AND (NOT ESCARGOT_ARCH STREQUAL "aarch64"))
        MESSAGE (FATAL_ERROR ${ESCARGOT_ARCH} " is unsupported")
    ENDIF()
    # recent macOS supports only ICU version 75 that requires c++17 or above
    SET (ESCARGOT_CXXFLAGS ${ESCARGOT_CXXFLAGS} -std=c++17)
    SET (ESCARGOT_LDFLAGS -lpthread -Wl,-dead_strip)
    SET (ESCARGOT_BUILD_64BIT_LARGE ON)
    # bdwgc mac cannot support pthread_getattr_np
    SET (ESCARGOT_THIRDPARTY_CFLAGS ${ESCARGOT_THIRDPARTY_CFLAGS} -UHAVE_PTHREAD_GETATTR_NP)
    # dlopen version not working correctly on mac
    IF (NOT DEFINED ESCARGOT_LIBICU_SUPPORT_WITH_DLOPEN)
        SET (ESCARGOT_LIBICU_SUPPORT_WITH_DLOPEN OFF)
    ENDIF()
ELSEIF (ESCARGOT_HOST STREQUAL "ios")
    # ESCARGOT_HOST=ios targets arm64 iOS, either SDK:
    #   -DCMAKE_OSX_SYSROOT=iphonesimulator -> iOS Simulator build (what the
    #     build-test-on-ios-simulator-arm64 CI job builds *and runs*, so it
    #     is the configuration with actual runtime coverage)
    #   -DCMAKE_OSX_SYSROOT=iphoneos        -> real-device build (built and
    #     Mach-O-verified by the build-on-ios-device-arm64 CI job, but never
    #     executed anywhere in CI -- running a device binary needs signed,
    #     provisioned hardware. The two SDKs share one set of headers/API
    #     availability annotations and differ only in sysroot + target
    #     triple, which is why this is a sysroot switch and not a separate
    #     port; see README.md's "iOS" section for what the embedder still
    #     has to do -- app bundle, code signing, sandboxed paths.)
    # iPadOS ships the same "iOS" SDK/platform identifier and arm64
    # sysroot/triple as iPhone at the CMake/toolchain level (Apple does not
    # build a separate iPadOS SDK), so this one host value covers both;
    # there is no separate "ipados" host.
    #
    # Unlike the android branch above (which relies on the caller passing
    # the NDK's own -DCMAKE_TOOLCHAIN_FILE=.../android.toolchain.cmake to
    # set up CMAKE_SYSTEM_NAME/sysroot/target-triple before this file ever
    # runs), Apple's CMake generators have first-class iOS cross-compilation
    # support built in via plain cache variables: the caller passes
    # -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphonesimulator|iphoneos
    # -DCMAKE_OSX_ARCHITECTURES=arm64 [-DCMAKE_OSX_DEPLOYMENT_TARGET=<ver>]
    # at the *initial* `cmake -B...` invocation (see README.md's "iOS"
    # section and the two ios CI jobs) -- CMake's own Clang compiler module
    # then derives the right
    # "-target arm64-apple-ios<ver>[-simulator] -isysroot <SDK>" flags for
    # every compile/link step automatically. This branch, like every other
    # host branch here, only adds Escargot's own engine-specific flags on
    # top; it cannot retroactively fix up the cross-compilation setup at
    # this point in the configure run (CMake's own compiler/ABI detection
    # already ran before build/config.cmake -- let alone this file -- was
    # ever INCLUDEd), hence the FATAL_ERRORs below instead of trying to set
    # CMAKE_SYSTEM_NAME/CMAKE_OSX_SYSROOT here.
    IF (NOT ESCARGOT_ARCH STREQUAL "aarch64")
        MESSAGE (FATAL_ERROR ${ESCARGOT_ARCH} " is unsupported (ESCARGOT_HOST=ios only targets arm64 -- the only architecture both current iOS devices and the iOS Simulator on an Apple Silicon build host use)")
    ENDIF()
    IF (NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
        MESSAGE (FATAL_ERROR "ESCARGOT_HOST=ios requires -DCMAKE_SYSTEM_NAME=iOS to be passed at the initial cmake invocation -- it cannot be set afterward. See README.md's \"iOS\" section.")
    ENDIF()
    # CMake resolves the short SDK name passed at the command line
    # (-DCMAKE_OSX_SYSROOT=iphonesimulator|iphoneos) into the full SDK path
    # by this point (e.g. ".../SDKs/iPhoneSimulator18.4.sdk") -- mixed-case,
    # unlike the lowercase short name -- so match case-insensitively.
    # ESCARGOT_IOS_PLATFORM is what the rest of the build keys off (see
    # build/VendoredICU.cmake, which needs the matching SDK name and target
    # triple for its own cross build of ICU).
    STRING (TOLOWER "${CMAKE_OSX_SYSROOT}" ESCARGOT_IOS_SYSROOT_LOWER)
    IF (ESCARGOT_IOS_SYSROOT_LOWER MATCHES "iphonesimulator")
        SET (ESCARGOT_IOS_PLATFORM "simulator")
    ELSEIF (ESCARGOT_IOS_SYSROOT_LOWER MATCHES "iphoneos")
        SET (ESCARGOT_IOS_PLATFORM "device")
        # Not a defect, just scope: nothing in this project has ever run on
        # physical iOS hardware, and CI can't (a device binary needs signed,
        # provisioned hardware to execute at all). Say so at configure time
        # rather than letting the absence of a warning imply device runtime
        # coverage that doesn't exist.
        MESSAGE (STATUS "ESCARGOT_HOST=ios: building against the iphoneos (device) SDK -- compile/link-verified only, never executed on physical hardware in this project's CI. Code signing, app-bundle packaging and sandbox-legal file paths are the embedder's responsibility; see README.md's \"iOS\" section.")
    ELSE()
        MESSAGE (FATAL_ERROR "ESCARGOT_HOST=ios needs an iOS SDK -- pass -DCMAKE_OSX_SYSROOT=iphonesimulator (Simulator) or -DCMAKE_OSX_SYSROOT=iphoneos (device) (found: '${CMAKE_OSX_SYSROOT}')")
    ENDIF()
    SET (ESCARGOT_BUILD_64BIT_LARGE ON)
    # recent ICU (see the darwin branch above) and this engine's own vendored
    # ICU build (build/VendoredICU.cmake) both need c++17
    SET (ESCARGOT_CXXFLAGS ${ESCARGOT_CXXFLAGS} -std=c++17)
    SET (ESCARGOT_LDFLAGS -Wl,-dead_strip)
    # bdwgc mac/iOS cannot support pthread_getattr_np (same as darwin above)
    SET (ESCARGOT_THIRDPARTY_CFLAGS ${ESCARGOT_THIRDPARTY_CFLAGS} -UHAVE_PTHREAD_GETATTR_NP)
    # No FIND_PACKAGE(PkgConfig) call here (unlike every other host branch
    # above) -- there is no pkg-config'able system ICU on iOS at all.
    # ESCARGOT_LIBICU_SUPPORT_VENDORED is the only supported ICU path (see
    # build/config.cmake, which both defaults it ON for this host and
    # FATAL_ERRORs if ESCARGOT_LIBICU_SUPPORT_WITH_DLOPEN or the plain
    # pkg-config path would otherwise be selected -- that enforcement lives
    # there and not here because ESCARGOT_LIBICU_SUPPORT/_WITH_DLOPEN aren't
    # resolved to their final value until after this file returns).
    IF (NOT DEFINED ESCARGOT_LIBICU_SUPPORT_WITH_DLOPEN)
        SET (ESCARGOT_LIBICU_SUPPORT_WITH_DLOPEN OFF)
    ENDIF()
ELSEIF (ESCARGOT_HOST STREQUAL "windows")
    IF (NOT ESCARGOT_LIBICU_SUPPORT_VENDORED)
        # Vendored (default): linked via the vcpkg-sourced import libs found
        # in build/VendoredICU.cmake instead -- the OS-provided icu.lib (from
        # the Windows 10 SDK's built-in ICU) is only wanted when explicitly
        # opted out of vendoring.
        SET (ESCARGOT_LDFLAGS ${ESCARGOT_LDFLAGS} icu.lib)
    ENDIF()
    IF ((ESCARGOT_ARCH STREQUAL "x64") OR (ESCARGOT_ARCH STREQUAL "x86_64"))
        SET (ESCARGOT_BUILD_64BIT ON)
        SET (ESCARGOT_BUILD_64BIT_LARGE ON)
    ELSEIF ((ESCARGOT_ARCH STREQUAL "x86") OR (ESCARGOT_ARCH STREQUAL "i686"))
        SET (ESCARGOT_BUILD_32BIT ON)
    ELSEIF (ESCARGOT_ARCH STREQUAL "arm")
        SET (ESCARGOT_BUILD_32BIT ON)
    ELSEIF (ESCARGOT_ARCH STREQUAL "aarch64" OR (ESCARGOT_ARCH STREQUAL "arm64"))
        SET (ESCARGOT_BUILD_64BIT ON)
        SET (ESCARGOT_BUILD_64BIT_LARGE ON)
    ELSE()
        MESSAGE (FATAL_ERROR ${ESCARGOT_ARCH} " is unsupported")
    ENDIF()

ELSEIF (ESCARGOT_HOST STREQUAL "baremetal")
    # Bare-metal/RTOS embedders (FreeRTOS, NuttX, ... -- see
    # docs/porting/RTOS_PORTING_GUIDE.md). This host value captures the
    # engine's own compile definitions/flags; BDWGC (third_party/GCutil) is
    # built by the normal build/escargot.cmake ADD_SUBDIRECTORY() alongside
    # it, in its own GCUTIL_NOSYS_BAREMETAL mode -- the calling RTOS port
    # just needs to set GCUTIL_NOSYS_BAREMETAL/GCUTIL_INITIAL_HEAP_SIZE/
    # GCUTIL_CFLAGS_FROM_EXTERNAL as CACHE variables before
    # add_subdirectory()ing this whole project (see e.g. the two proven
    # reference ports' own CMakeLists.txt for the full picture). No
    # PkgConfig requirement (no ICU on these targets -- ESCARGOT_LIBICU_SUPPORT
    # already defaults OFF for this host, see root CMakeLists.txt), no
    # -lpthread/-lrt (no thread runtime).
    SET (ESCARGOT_DEFINITIONS ${ESCARGOT_DEFINITIONS} -DOS_BAREMETAL=1)
    SET (ESCARGOT_LDFLAGS -Wl,--gc-sections)
    IF (ESCARGOT_SMALL_CONFIG)
        # Bare-metal/RTOS targets prioritize code size over the general
        # GCC-version gate above (that gate's "-Os has unknown memory
        # conflicts" concern was about a hosted platform -- not
        # re-litigated here, just not applied to this host). Force -Os
        # unconditionally for this host when ESCARGOT_SMALL_CONFIG is on.
        SET (ESCARGOT_CXXFLAGS_RELEASE ${ESCARGOT_CXXFLAGS_RELEASE} -Os)
    ENDIF()
    # newlib's stdlib.h only declares alloca() outside of strict-ISO mode --
    # the generic "-std=c++11" set above (GNU/Clang branch) defines
    # __STRICT_ANSI__, which hides it and breaks the parser's ALLOCA() macro
    # (see src/Escargot.h). Override to the GNU dialect, same as arm-none-eabi-g++'s
    # own default (-std=gnu++17) that each RTOS port's standalone CMakeLists.txt
    # relied on implicitly by never setting -std= itself.
    IF (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        SET (ESCARGOT_CXXFLAGS ${ESCARGOT_CXXFLAGS} -std=gnu++11)
    ENDIF()
    # 64-bit bare-metal/RTOS targets always use full, uncompressed 64-bit
    # addressing (ESCARGOT_BUILD_64BIT_LARGE), never the 32-bit-in-64-bit
    # pointer-compression mode -- that mode assumes addresses fit in 32
    # bits, which a hosted OS process's loader/allocator typically keeps
    # true, but a bare-metal/RTOS target's custom linker script can place
    # code/GC-heap/stack anywhere in the full 64-bit address space with no
    # such guarantee. Silently violating that assumption would truncate
    # pointers instead of failing loudly, so this is forced for every
    # 64-bit arch here, not just riscv64.
    IF ((ESCARGOT_ARCH STREQUAL "x64") OR (ESCARGOT_ARCH STREQUAL "x86_64"))
        SET (ESCARGOT_BUILD_64BIT ON)
        SET (ESCARGOT_BUILD_64BIT_LARGE ON)
    ELSEIF ((ESCARGOT_ARCH STREQUAL "x86") OR (ESCARGOT_ARCH STREQUAL "i686"))
        SET (ESCARGOT_BUILD_32BIT ON)
    ELSEIF (ESCARGOT_ARCH STREQUAL "arm")
        SET (ESCARGOT_BUILD_32BIT ON)
    ELSEIF (ESCARGOT_ARCH STREQUAL "aarch64")
        SET (ESCARGOT_BUILD_64BIT ON)
        SET (ESCARGOT_BUILD_64BIT_LARGE ON)
    ELSEIF (ESCARGOT_ARCH STREQUAL "riscv64")
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
