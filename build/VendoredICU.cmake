# Builds/locates Escargot's own ICU instead of relying on a system-provided
# one. Included only when ESCARGOT_LIBICU_SUPPORT_VENDORED is ON (see
# build/config.cmake, which resolves its default and host guard before
# build/target.cmake even runs). Mechanism differs per host:
#   - linux: builds the vendored third_party/icu submodule (pinned to tag
#     release-78.1, matching CI's Linux ICU pin) with its data trimmed via
#     build/icu-filters/escargot.json, and links it statically. Prototype
#     for eventually reusing the same approach on targets (e.g. iOS) with no
#     usable system ICU.
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
#     (contrast with the separate ESCARGOT_HOST=ios work, which cross-builds).
#   - windows: ICU's own Windows build only ships common/i18n as DLLs (no
#     static .lib variant, unlike the Unix --enable-static build above/below),
#     so this locates a vcpkg-installed ICU instead (see the vendored-icu CI
#     jobs for the `vcpkg install icu --triplet=...` step) and links against
#     its import libs; the matching DLLs get copied next to the escargot
#     binary as a separate CI/deploy step (selected via `dumpbin
#     /dependents`, mirroring the ldd/otool-based ICU-copy step already used
#     for Linux/macOS release deploys), not handled here at configure time.

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
ELSE()
    MESSAGE (FATAL_ERROR "ESCARGOT_LIBICU_SUPPORT_VENDORED has no implementation for ESCARGOT_HOST=${ESCARGOT_HOST}")
ENDIF()
