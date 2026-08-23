# build/target.cmake even runs). Mechanism differs per host:
#   - linux: builds the vendored third_party/icu submodule (pinned to tag
#     release-78.1, matching CI's Linux ICU pin) with its data trimmed via
#     build/icu-filters/escargot.json, and links it statically.
#   - macOS: same idea as linux -- ICU's build system has a real MacOSX
#     configure host (unlike windows, see below), so this also builds the
#     third_party/icu submodule from source via `runConfigureICU MacOSX
#     --enable-static` and links the resulting static archives directly. No
#     -Wl,--start-group/--end-group here even though the three archives are
#     mutually referential like on linux: Apple's ld64 doesn't understand
#     GNU ld's --start-group/--end-group syntax at all (errors with "unknown
#     option"), and doesn't need an equivalent either -- unlike GNU ld's
#     single left-to-right pass over the libraries listed on the command
#     line, ld64 is documented to search back into an earlier archive to
#     satisfy a later one's undefined symbols regardless of link order, so
#     plain -licui18n -licuuc -licudata (or the equivalent full paths) is
#     enough. This is a native build only (host runner arch == target arch,
#     ESCARGOT_ARCH x64/aarch64 both already validated by build/target.cmake's
#     darwin branch) -- ICU's configure picks up the host architecture from
#     CC/CFLAGS, so there is nothing cross-compile-specific to wire here
#     (contrast with the ios branch below, which cross-builds).
#   - windows: ICU's own Windows build only ships common/i18n as DLLs (no
#     static .lib variant, unlike the Unix --enable-static build above/below),
#     so this locates a vcpkg-installed ICU instead (see the vendored-icu CI
#     jobs for the `vcpkg install icu --triplet=...` step) and links against
#     its import libs; the matching DLLs get copied next to the escargot
#     binary as a separate CI/deploy step (selected via `dumpbin
#     /dependents`, mirroring the ldd/otool-based ICU-copy step already used
#     for Linux/macOS release deploys), not handled here at configure time.
#   - ios (iOS Simulator, arm64 only -- see build/target.cmake's ios branch):
#     there is no system/pkg-config ICU on iOS at all, so this is the ONLY
#     supported ICU path for ESCARGOT_HOST=ios (see build/config.cmake's
#     FATAL_ERRORs enforcing "vendored or off, nothing else"). ICU has no
#     native "iOS" autoconf target, so this does the standard two-pass cross
#     build documented in ICU's User Guide ("Building ICU4C" -- cross
#     compiling section, `--with-cross-build`) and used by community iOS ICU
#     recipes (e.g. zhm/icu-ios, the older ICU-for-iOS blog recipes):
#       1. Build ICU's own tools (genrb, genbrk, gencfu, ...) for the BUILD
#          machine (native macOS, via `runConfigureICU MacOSX`) -- a cross
#          build's data-generation step at `make` time shells out to these
#          host tool binaries directly; there is no way to execute an
#          iphonesimulator binary as part of the build graph other than on
#          the simulator itself, which ICU's build system has no support
#          for.
#       2. Cross-compile the real, target ICU with `configure
#          --with-cross-build=<pass-1 build dir>` (this reuses pass 1's host
#          tool binaries for data generation instead of building/running its
#          own), with CC/CFLAGS/LDFLAGS pointed at the iphonesimulator SDK
#          sysroot (`xcrun --sdk iphonesimulator --show-sdk-path`) and an
#          explicit `-target arm64-apple-ios<ver>-simulator` triple (the
#          modern clang-driver equivalent of the older
#          `-arch arm64 -mios-simulator-version-min=<ver>` flag pair; ICU's
#          own configure.ac has no iOS-specific branches at all -- this
#          triple/sysroot pair is the entire mechanism that makes the
#          resulting object files/archives target the simulator instead of
#          the host).
#     `--host=aarch64-apple-darwin` on the pass-2 configure line only tells
#     autoconf "this is a cross build" (skips AC_RUN_IFELSE checks that
#     would need target-side execution, and gates the --with-cross-build
#     requirement) -- it is not otherwise interpreted by ICU.

IF (ESCARGOT_HOST STREQUAL "linux")
    # add_dependencies() on an INTERFACE_LIBRARY target only propagates to
    # dependents since CMake 3.19 -- below that, ESCARGOT_TARGET could start
    # linking before the ExternalProject build finishes. Fail loudly instead
    # of silently racing.
    IF (CMAKE_VERSION VERSION_LESS "3.19")
        MESSAGE (FATAL_ERROR "ESCARGOT_LIBICU_SUPPORT_VENDORED requires CMake >= 3.19 (found ${CMAKE_VERSION})")
    ENDIF()

    IF (NOT EXISTS ${ESCARGOT_THIRD_PARTY_ROOT}/icu/icu4c/source/runConfigureICU)
        MESSAGE (FATAL_ERROR "third_party/icu submodule is not checked out -- run: git submodule update --init third_party/icu")
    ENDIF()

    INCLUDE (ExternalProject)
    INCLUDE (ProcessorCount)
    ProcessorCount (VENDORED_ICU_NPROC)
    IF (VENDORED_ICU_NPROC EQUAL 0)
        SET (VENDORED_ICU_NPROC 1)
    ENDIF()

    SET (VENDORED_ICU_SOURCE_DIR ${ESCARGOT_THIRD_PARTY_ROOT}/icu/icu4c/source)
    SET (VENDORED_ICU_INSTALL_DIR ${CMAKE_BINARY_DIR}/vendored-icu)
    SET (VENDORED_ICU_DATA_FILTER_FILE ${ESCARGOT_ROOT}/build/icu-filters/escargot.json)

    ExternalProject_Add (vendored-icu-build
        SOURCE_DIR ${VENDORED_ICU_SOURCE_DIR}
        DOWNLOAD_COMMAND ""
        UPDATE_COMMAND ""
        BUILD_IN_SOURCE FALSE
        CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env
            "ICU_DATA_FILTER_FILE=${VENDORED_ICU_DATA_FILTER_FILE}"
            ${VENDORED_ICU_SOURCE_DIR}/runConfigureICU Linux/gcc
                --enable-static
                --disable-shared
                --disable-tests
                --disable-samples
                --with-data-packaging=static
                --prefix=${VENDORED_ICU_INSTALL_DIR}
        BUILD_COMMAND ${CMAKE_COMMAND} -E env
            "ICU_DATA_FILTER_FILE=${VENDORED_ICU_DATA_FILTER_FILE}"
            make -j${VENDORED_ICU_NPROC}
        INSTALL_COMMAND make install
        BUILD_BYPRODUCTS
            ${VENDORED_ICU_INSTALL_DIR}/lib/libicui18n.a
            ${VENDORED_ICU_INSTALL_DIR}/lib/libicuuc.a
            ${VENDORED_ICU_INSTALL_DIR}/lib/libicudata.a
        LOG_CONFIGURE TRUE
        LOG_BUILD TRUE
        LOG_INSTALL TRUE
        LOG_OUTPUT_ON_FAILURE TRUE
    )

    # The three static archives are mutually referential (icui18n -> icuuc ->
    # icudata, with some back-references) -- wrap in --start-group/--end-group
    # so link order doesn't matter with GNU ld/lld.
    ADD_LIBRARY (vendored-icu INTERFACE)
    TARGET_LINK_LIBRARIES (vendored-icu INTERFACE
        -Wl,--start-group
        ${VENDORED_ICU_INSTALL_DIR}/lib/libicui18n.a
        ${VENDORED_ICU_INSTALL_DIR}/lib/libicuuc.a
        ${VENDORED_ICU_INSTALL_DIR}/lib/libicudata.a
        -Wl,--end-group
    )
    ADD_DEPENDENCIES (vendored-icu vendored-icu-build)

    SET (VENDORED_ICU_INCLUDE_DIRS ${VENDORED_ICU_INSTALL_DIR}/include)
    SET (VENDORED_ICU_LIBRARIES vendored-icu)
ELSEIF (ESCARGOT_HOST STREQUAL "darwin")
    IF (CMAKE_VERSION VERSION_LESS "3.19")
        MESSAGE (FATAL_ERROR "ESCARGOT_LIBICU_SUPPORT_VENDORED requires CMake >= 3.19 (found ${CMAKE_VERSION})")
    ENDIF()

    IF (NOT EXISTS ${ESCARGOT_THIRD_PARTY_ROOT}/icu/icu4c/source/runConfigureICU)
        MESSAGE (FATAL_ERROR "third_party/icu submodule is not checked out -- run: git submodule update --init third_party/icu")
    ENDIF()

    INCLUDE (ExternalProject)
    INCLUDE (ProcessorCount)
    ProcessorCount (VENDORED_ICU_NPROC)
    IF (VENDORED_ICU_NPROC EQUAL 0)
        SET (VENDORED_ICU_NPROC 1)
    ENDIF()

    SET (VENDORED_ICU_SOURCE_DIR ${ESCARGOT_THIRD_PARTY_ROOT}/icu/icu4c/source)
    SET (VENDORED_ICU_INSTALL_DIR ${CMAKE_BINARY_DIR}/vendored-icu)
    SET (VENDORED_ICU_DATA_FILTER_FILE ${ESCARGOT_ROOT}/build/icu-filters/escargot.json)

    ExternalProject_Add (vendored-icu-build
        SOURCE_DIR ${VENDORED_ICU_SOURCE_DIR}
        DOWNLOAD_COMMAND ""
        UPDATE_COMMAND ""
        BUILD_IN_SOURCE FALSE
        CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env
            "ICU_DATA_FILTER_FILE=${VENDORED_ICU_DATA_FILTER_FILE}"
            ${VENDORED_ICU_SOURCE_DIR}/runConfigureICU MacOSX
                --enable-static
                --disable-shared
                --disable-tests
                --disable-samples
                --with-data-packaging=static
                --prefix=${VENDORED_ICU_INSTALL_DIR}
        BUILD_COMMAND ${CMAKE_COMMAND} -E env
            "ICU_DATA_FILTER_FILE=${VENDORED_ICU_DATA_FILTER_FILE}"
            make -j${VENDORED_ICU_NPROC}
        INSTALL_COMMAND make install
        BUILD_BYPRODUCTS
            ${VENDORED_ICU_INSTALL_DIR}/lib/libicui18n.a
            ${VENDORED_ICU_INSTALL_DIR}/lib/libicuuc.a
            ${VENDORED_ICU_INSTALL_DIR}/lib/libicudata.a
        LOG_CONFIGURE TRUE
        LOG_BUILD TRUE
        LOG_INSTALL TRUE
        LOG_OUTPUT_ON_FAILURE TRUE
    )

    # Unlike the linux branch above, no -Wl,--start-group/--end-group here:
    # Apple's ld64 (a) doesn't accept that GNU ld syntax at all -- it fails
    # with "unknown option: --start-group" -- and (b) doesn't need an
    # equivalent, since it resolves undefined symbols across all archives
    # given on the command line regardless of the order they're listed in
    # (unlike GNU ld's single left-to-right pass), so the three mutually
    # referential static archives link fine listed plainly.
    ADD_LIBRARY (vendored-icu INTERFACE)
    TARGET_LINK_LIBRARIES (vendored-icu INTERFACE
        ${VENDORED_ICU_INSTALL_DIR}/lib/libicui18n.a
        ${VENDORED_ICU_INSTALL_DIR}/lib/libicuuc.a
        ${VENDORED_ICU_INSTALL_DIR}/lib/libicudata.a
    )
    ADD_DEPENDENCIES (vendored-icu vendored-icu-build)

    SET (VENDORED_ICU_INCLUDE_DIRS ${VENDORED_ICU_INSTALL_DIR}/include)
    SET (VENDORED_ICU_LIBRARIES vendored-icu)
ELSEIF (ESCARGOT_HOST STREQUAL "windows")
    IF (DEFINED ICU_ROOT)
        FIND_PATH (VENDORED_ICU_INCLUDE_DIR unicode/utypes.h PATHS ${ICU_ROOT}/include NO_DEFAULT_PATH)
        FIND_LIBRARY (VENDORED_ICU_UC_LIB NAMES icuuc PATHS ${ICU_ROOT}/lib NO_DEFAULT_PATH)
        FIND_LIBRARY (VENDORED_ICU_IN_LIB NAMES icuin PATHS ${ICU_ROOT}/lib NO_DEFAULT_PATH)
    ENDIF()

    IF (NOT DEFINED ICU_ROOT OR NOT VENDORED_ICU_INCLUDE_DIR OR NOT VENDORED_ICU_UC_LIB OR NOT VENDORED_ICU_IN_LIB)
        # ESCARGOT_LIBICU_SUPPORT_VENDORED_IS_DEFAULT (see build/config.cmake)
        # is only ON when the caller never passed
        # -DESCARGOT_LIBICU_SUPPORT_VENDORED explicitly -- i.e. this is just
        # windows's own default, most likely a dev who hasn't set up vcpkg
        # yet. Don't hard-fail their configure over a default they didn't
        # ask for; fall back to loading whatever ICU is already on the
        # target machine at runtime instead, same as this repo's other
        # non-Windows hosts do by default. An explicit
        # -DESCARGOT_LIBICU_SUPPORT_VENDORED=ON still hard-fails below --
        # that's the caller asking for the vendored build specifically, so
        # silently swapping it out from under them would be wrong.
        IF (ESCARGOT_LIBICU_SUPPORT_VENDORED_IS_DEFAULT)
            MESSAGE (WARNING "ESCARGOT_LIBICU_SUPPORT_VENDORED defaulted ON but no vcpkg-installed ICU was found "
                "(ICU_ROOT=${ICU_ROOT}, looked for include/unicode/utypes.h, lib/icuuc.lib, lib/icuin.lib) -- "
                "falling back to ESCARGOT_LIBICU_SUPPORT_WITH_DLOPEN=ON (loads whatever ICU is present on the "
                "target machine at runtime) instead. To use the vendored build, run "
                "`vcpkg install icu --triplet=<x86|x64|arm64>-windows` and pass "
                "-DICU_ROOT=<vcpkg>/installed/<triplet>; to silence this warning without vcpkg, pass "
                "-DESCARGOT_LIBICU_SUPPORT_VENDORED=OFF explicitly.")
            SET (ESCARGOT_LIBICU_SUPPORT_VENDORED OFF)
            SET (ESCARGOT_LIBICU_SUPPORT_WITH_DLOPEN ON)
        ELSE()
            MESSAGE (FATAL_ERROR "ESCARGOT_LIBICU_SUPPORT_VENDORED=ON but no vcpkg-installed ICU was found under "
                "ICU_ROOT=${ICU_ROOT} (looked for include/unicode/utypes.h, lib/icuuc.lib, lib/icuin.lib) -- run "
                "`vcpkg install icu --triplet=<x86|x64|arm64>-windows` and pass "
                "-DICU_ROOT=<vcpkg>/installed/<triplet>")
        ENDIF()
    ELSE()
        MESSAGE (STATUS "Vendored ICU (windows/vcpkg): ${VENDORED_ICU_UC_LIB} / ${VENDORED_ICU_IN_LIB}")
        SET (VENDORED_ICU_INCLUDE_DIRS ${VENDORED_ICU_INCLUDE_DIR})
        SET (VENDORED_ICU_LIBRARIES ${VENDORED_ICU_UC_LIB} ${VENDORED_ICU_IN_LIB})
    ENDIF()
ELSEIF (ESCARGOT_HOST STREQUAL "ios")
    IF (CMAKE_VERSION VERSION_LESS "3.19")
        MESSAGE (FATAL_ERROR "ESCARGOT_LIBICU_SUPPORT_VENDORED requires CMake >= 3.19 (found ${CMAKE_VERSION})")
    ENDIF()

    IF (NOT EXISTS ${ESCARGOT_THIRD_PARTY_ROOT}/icu/icu4c/source/runConfigureICU)
        MESSAGE (FATAL_ERROR "third_party/icu submodule is not checked out -- run: git submodule update --init third_party/icu")
    ENDIF()

    FIND_PROGRAM (VENDORED_ICU_XCRUN xcrun)
    IF (NOT VENDORED_ICU_XCRUN)
        MESSAGE (FATAL_ERROR "ESCARGOT_HOST=ios needs Xcode's xcrun on PATH to locate the iphonesimulator SDK")
    ENDIF()

    EXECUTE_PROCESS (
        COMMAND ${VENDORED_ICU_XCRUN} --sdk iphonesimulator --show-sdk-path
        OUTPUT_VARIABLE VENDORED_ICU_IOS_SIM_SDKROOT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE VENDORED_ICU_XCRUN_RESULT
    )
    IF (NOT VENDORED_ICU_XCRUN_RESULT EQUAL 0 OR NOT VENDORED_ICU_IOS_SIM_SDKROOT)
        MESSAGE (FATAL_ERROR "`xcrun --sdk iphonesimulator --show-sdk-path` failed -- is the full Xcode.app (not just the Command Line Tools) installed and selected via xcode-select?")
    ENDIF()

    # Same deployment target the rest of this project's own sources are
    # being built against (see build/target.cmake's ios branch, which
    # requires the caller to have passed -DCMAKE_OSX_DEPLOYMENT_TARGET=...
    # up front) -- keeping ICU's own min-iOS-version flag in sync avoids a
    # "object file built for newer iOS Simulator version" link warning/error
    # when linking these static archives into the escargot binary.
    IF (NOT CMAKE_OSX_DEPLOYMENT_TARGET)
        SET (VENDORED_ICU_IOS_MIN_VERSION "13.0")
    ELSE()
        SET (VENDORED_ICU_IOS_MIN_VERSION ${CMAKE_OSX_DEPLOYMENT_TARGET})
    ENDIF()
    SET (VENDORED_ICU_IOS_SIM_TARGET_FLAG "arm64-apple-ios${VENDORED_ICU_IOS_MIN_VERSION}-simulator")

    INCLUDE (ExternalProject)
    INCLUDE (ProcessorCount)
    ProcessorCount (VENDORED_ICU_NPROC)
    IF (VENDORED_ICU_NPROC EQUAL 0)
        SET (VENDORED_ICU_NPROC 1)
    ENDIF()

    SET (VENDORED_ICU_SOURCE_DIR ${ESCARGOT_THIRD_PARTY_ROOT}/icu/icu4c/source)
    SET (VENDORED_ICU_DATA_FILTER_FILE ${ESCARGOT_ROOT}/build/icu-filters/escargot.json)
    SET (VENDORED_ICU_HOST_BUILD_DIR ${CMAKE_BINARY_DIR}/vendored-icu-host-build)
    SET (VENDORED_ICU_HOST_INSTALL_DIR ${VENDORED_ICU_HOST_BUILD_DIR}/install)
    SET (VENDORED_ICU_INSTALL_DIR ${CMAKE_BINARY_DIR}/vendored-icu)

    # --- Pass 1: host tools (genrb, genbrk, ...), built natively for the
    # macOS build machine. Deliberately ignores this project's own
    # CMAKE_C_COMPILER/CMAKE_OSX_SYSROOT (which target the iOS Simulator
    # once ESCARGOT_HOST=ios is configured, see build/target.cmake) --
    # `xcrun --sdk macosx clang` is used explicitly instead so this pass
    # always builds for the host regardless of the outer project's own
    # cross-compilation setup.
    ExternalProject_Add (vendored-icu-host-build
        SOURCE_DIR ${VENDORED_ICU_SOURCE_DIR}
        DOWNLOAD_COMMAND ""
        UPDATE_COMMAND ""
        BUILD_IN_SOURCE FALSE
        BINARY_DIR ${VENDORED_ICU_HOST_BUILD_DIR}
        CONFIGURE_COMMAND bash -c "CC='xcrun --sdk macosx clang' CXX='xcrun --sdk macosx clang++' ${VENDORED_ICU_SOURCE_DIR}/runConfigureICU MacOSX --enable-static --disable-shared --disable-tests --disable-samples --with-data-packaging=static --prefix=${VENDORED_ICU_HOST_INSTALL_DIR}"
        BUILD_COMMAND make -j${VENDORED_ICU_NPROC}
        INSTALL_COMMAND make install
        LOG_CONFIGURE TRUE
        LOG_BUILD TRUE
        LOG_INSTALL TRUE
        LOG_OUTPUT_ON_FAILURE TRUE
    )

    # --- Pass 2: the real target build, cross-compiled for the iOS
    # Simulator (arm64) using pass 1's host tools (via --with-cross-build)
    # to generate its data. --disable-tools is required here (NOT in pass
    # 1, which must build tools normally so they exist to be referenced) --
    # without it, ICU's top-level `make` unconditionally still compiles
    # tools/pkgdata (configure.ac's TOOLS conditional has no
    # cross-compiling exception, unlike DATA's, which builds whenever
    # cross_compiling=yes regardless of --disable-tools) even though
    # --with-cross-build already makes data/'s packaging step invoke pass
    # 1's pkgdata via config/icucross.mk instead of this build's own --
    # and pkgdata.cpp fails to compile on the iOS SDK at all (`system()` is
    # marked unavailable there), since it's generic, not iOS-aware, code.
    ExternalProject_Add (vendored-icu-build
        SOURCE_DIR ${VENDORED_ICU_SOURCE_DIR}
        DOWNLOAD_COMMAND ""
        UPDATE_COMMAND ""
        BUILD_IN_SOURCE FALSE
        DEPENDS vendored-icu-host-build
        CONFIGURE_COMMAND bash -c "CC='xcrun --sdk iphonesimulator clang' CXX='xcrun --sdk iphonesimulator clang++' CFLAGS='-target ${VENDORED_ICU_IOS_SIM_TARGET_FLAG} -isysroot ${VENDORED_ICU_IOS_SIM_SDKROOT}' CXXFLAGS='-target ${VENDORED_ICU_IOS_SIM_TARGET_FLAG} -isysroot ${VENDORED_ICU_IOS_SIM_SDKROOT} -std=c++17' LDFLAGS='-target ${VENDORED_ICU_IOS_SIM_TARGET_FLAG} -isysroot ${VENDORED_ICU_IOS_SIM_SDKROOT} -Wl,-dead_strip' ${VENDORED_ICU_SOURCE_DIR}/configure --host=aarch64-apple-darwin --enable-static --disable-shared --disable-tests --disable-samples --disable-tools --with-data-packaging=static --with-cross-build=${VENDORED_ICU_HOST_BUILD_DIR} --prefix=${VENDORED_ICU_INSTALL_DIR}"
        BUILD_COMMAND ${CMAKE_COMMAND} -E env
            "ICU_DATA_FILTER_FILE=${VENDORED_ICU_DATA_FILTER_FILE}"
            make -j${VENDORED_ICU_NPROC}
        INSTALL_COMMAND make install
        BUILD_BYPRODUCTS
            ${VENDORED_ICU_INSTALL_DIR}/lib/libicui18n.a
            ${VENDORED_ICU_INSTALL_DIR}/lib/libicuuc.a
            ${VENDORED_ICU_INSTALL_DIR}/lib/libicudata.a
        LOG_CONFIGURE TRUE
        LOG_BUILD TRUE
        LOG_INSTALL TRUE
        LOG_OUTPUT_ON_FAILURE TRUE
    )

    # Unlike GNU ld/lld (see the linux implementation of this same pattern),
    # Apple's linker resolves symbols across static archives in multiple
    # passes by default, so no -Wl,--start-group/--end-group equivalent is
    # needed here for the three mutually-referential archives.
    ADD_LIBRARY (vendored-icu INTERFACE)
    TARGET_LINK_LIBRARIES (vendored-icu INTERFACE
        ${VENDORED_ICU_INSTALL_DIR}/lib/libicui18n.a
        ${VENDORED_ICU_INSTALL_DIR}/lib/libicuuc.a
        ${VENDORED_ICU_INSTALL_DIR}/lib/libicudata.a
    )
    ADD_DEPENDENCIES (vendored-icu vendored-icu-build)

    SET (VENDORED_ICU_INCLUDE_DIRS ${VENDORED_ICU_INSTALL_DIR}/include)
    SET (VENDORED_ICU_LIBRARIES vendored-icu)
ELSE()
    MESSAGE (FATAL_ERROR "ESCARGOT_LIBICU_SUPPORT_VENDORED has no implementation for ESCARGOT_HOST=${ESCARGOT_HOST}")
ENDIF()
