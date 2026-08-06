CMAKE_MINIMUM_REQUIRED (VERSION 2.8.12 FATAL_ERROR)

SET (ESCARGOT_INCDIRS
    ${ESCARGOT_INCDIRS}
    ${ESCARGOT_ROOT}/src/
    ${ESCARGOT_THIRD_PARTY_ROOT}
    ${ESCARGOT_THIRD_PARTY_ROOT}/checked_arithmetic/
    ${ESCARGOT_THIRD_PARTY_ROOT}/double_conversion/
    ${ESCARGOT_THIRD_PARTY_ROOT}/lz4/
    ${ESCARGOT_THIRD_PARTY_ROOT}/rapidjson/include/
    ${ESCARGOT_THIRD_PARTY_ROOT}/yarr/
    ${ESCARGOT_THIRD_PARTY_ROOT}/runtime_icu_binder/
    ${ESCARGOT_THIRD_PARTY_ROOT}/robin_map/include/
    ${ESCARGOT_THIRD_PARTY_ROOT}/xsum/
)

IF (${ESCARGOT_MODE} STREQUAL "debug")
    SET (ESCARGOT_CXXFLAGS ${ESCARGOT_CXXFLAGS_DEBUG} ${ESCARGOT_CXXFLAGS})
    SET (ESCARGOT_LDFLAGS ${ESCARGOT_LDFLAGS_DEBUG} ${ESCARGOT_LDFLAGS})
    SET (ESCARGOT_DEFINITIONS ${ESCARGOT_DEFINITIONS} ${ESCARGOT_DEFINITIONS_DEBUG})
ELSEIF (${ESCARGOT_MODE} STREQUAL "release")
    SET (ESCARGOT_CXXFLAGS ${ESCARGOT_CXXFLAGS_RELEASE} ${ESCARGOT_CXXFLAGS})
    SET (ESCARGOT_LDFLAGS ${ESCARGOT_LDFLAGS_RELEASE} ${ESCARGOT_LDFLAGS})
    SET (ESCARGOT_DEFINITIONS ${ESCARGOT_DEFINITIONS} ${ESCARGOT_DEFINITIONS_RELEASE})
ENDIF()

IF (${ESCARGOT_OUTPUT} STREQUAL "shell")
    SET (ESCARGOT_CXXFLAGS ${ESCARGOT_CXXFLAGS} ${ESCARGOT_CXXFLAGS_SHELL})
    SET (ESCARGOT_LDFLAGS ${ESCARGOT_LDFLAGS} ${ESCARGOT_LDFLAGS_SHELL})
    SET (ESCARGOT_DEFINITIONS ${ESCARGOT_DEFINITIONS} ${ESCARGOT_DEFINITIONS_SHELL})
ELSEIF (${ESCARGOT_OUTPUT} STREQUAL "shared_lib")
    SET (ESCARGOT_CXXFLAGS ${ESCARGOT_CXXFLAGS} ${ESCARGOT_CXXFLAGS_SHAREDLIB})
    SET (ESCARGOT_LDFLAGS ${ESCARGOT_LDFLAGS} ${ESCARGOT_LDFLAGS_SHAREDLIB})
    SET (ESCARGOT_DEFINITIONS ${ESCARGOT_DEFINITIONS} ${ESCARGOT_DEFINITIONS_SHAREDLIB})
ELSEIF (${ESCARGOT_OUTPUT} STREQUAL "static_lib")
    SET (ESCARGOT_CXXFLAGS ${ESCARGOT_CXXFLAGS} ${ESCARGOT_CXXFLAGS_STATICLIB})
    SET (ESCARGOT_LDFLAGS ${ESCARGOT_LDFLAGS} ${ESCARGOT_LDFLAGS_STATICLIB})
    SET (ESCARGOT_DEFINITIONS ${ESCARGOT_DEFINITIONS} ${ESCARGOT_DEFINITIONS_STATICLIB})
ENDIF()

IF (ESCARGOT_ASAN)
    SET (ESCARGOT_CXXFLAGS ${ESCARGOT_CXXFLAGS} -fsanitize=address)
    SET (ESCARGOT_LDFLAGS ${ESCARGOT_LDFLAGS} -lasan)
ENDIF()

# Code coverage test with gcovr and Codecov
IF (ESCARGOT_COVERAGE)
    SET (ESCARGOT_CXXFLAGS ${ESCARGOT_CXXFLAGS} -O0 -fprofile-arcs -ftest-coverage)
    SET (ESCARGOT_LDFLAGS ${ESCARGOT_LDFLAGS} --coverage)
    SET (ESCARGOT_LIBRARIES ${ESCARGOT_LIBRARIES} gcov)
ENDIF()

# SOURCE FILES
FILE (GLOB_RECURSE ESCARGOT_SRC ${ESCARGOT_ROOT}/src/*.cpp)
FILE (GLOB YARR_SRC ${ESCARGOT_THIRD_PARTY_ROOT}/yarr/*.cpp)
FILE (GLOB DOUBLE_CONVERSION_SRC ${ESCARGOT_THIRD_PARTY_ROOT}/double_conversion/*.cc)
FILE (GLOB LZ4_SRC ${ESCARGOT_THIRD_PARTY_ROOT}/lz4/*.cpp)
FILE (GLOB XSUM_SRC ${ESCARGOT_THIRD_PARTY_ROOT}/xsum/*.cpp)

IF (NOT ${ESCARGOT_OUTPUT} MATCHES "shell")
    LIST (REMOVE_ITEM ESCARGOT_SRC ${ESCARGOT_ROOT}/src/shell/Shell.cpp)
ENDIF()

IF (${ESCARGOT_OUTPUT} STREQUAL "cctest")
    SET (BUILD_GMOCK OFF)
    SET (INSTALL_GTEST OFF)
    ADD_COMPILE_OPTIONS(${ESCARGOT_THIRDPARTY_CFLAGS})
    ADD_SUBDIRECTORY (third_party/googletest)
    FILE (GLOB CCTEST_SRC ${ESCARGOT_ROOT}/test/cctest/*.cpp)
ENDIF()

SET (ESCARGOT_SRC_LIST
    ${ESCARGOT_SRC}
    ${YARR_SRC}
    ${DOUBLE_CONVERSION_SRC}
    ${LZ4_SRC}
    ${XSUM_SRC}
    ${CCTEST_SRC}
)

#######################################################
# GCUTIL
#######################################################
IF (${ESCARGOT_OUTPUT} STREQUAL "shared_lib")
    SET (ESCARGOT_THIRDPARTY_CFLAGS ${ESCARGOT_THIRDPARTY_CFLAGS} ${ESCARGOT_CXXFLAGS_SHAREDLIB})
ELSEIF (${ESCARGOT_OUTPUT} STREQUAL "static_lib")
    SET (ESCARGOT_THIRDPARTY_CFLAGS ${ESCARGOT_THIRDPARTY_CFLAGS} ${ESCARGOT_CXXFLAGS_STATICLIB})
ENDIF()

SET (GCUTIL_CFLAGS ${ESCARGOT_THIRDPARTY_CFLAGS} ${PROFILER_FLAGS})
# Append, don't overwrite: a bare-metal port may have already pre-set this as
# a CACHE variable before add_subdirectory()ing this whole project (e.g.
# NuttX's -D_setjmp=setjmp/-D_longjmp=longjmp remap) -- ESCARGOT_CFLAGS_FROM_EXTERNAL
# (the toolchain/arch flags every other target here also gets) still needs to
# reach GCutil too, since it's compiled for the exact same target.
SET (GCUTIL_CFLAGS_FROM_EXTERNAL ${GCUTIL_CFLAGS_FROM_EXTERNAL} ${ESCARGOT_CFLAGS_FROM_EXTERNAL})

IF (ESCARGOT_SMALL_CONFIG)
    SET (GCUTIL_CFLAGS ${GCUTIL_CFLAGS} -DSMALL_CONFIG)
ENDIF()
IF (ESCARGOT_THREADING)
    SET (GCUTIL_ENABLE_THREADING ON)
ENDIF()
IF (ESCARGOT_TLS_ACCESS_BY_ADDRESS)
    SET (GCUTIL_ENABLE_TLS_ACCESS_BY_ADDRESS ON)
ENDIF()
IF (ESCARGOT_TLS_ACCESS_BY_PTHREAD_KEY)
    SET (GCUTIL_ENABLE_TLS_ACCESS_BY_PTHREAD_KEY ON)
ENDIF()

SET (GCUTIL_MODE ${ESCARGOT_MODE})

# Bare-metal/RTOS ports set GCUTIL_NOSYS_BAREMETAL/GCUTIL_INITIAL_HEAP_SIZE/
# GCUTIL_CFLAGS_FROM_EXTERNAL as CACHE variables before add_subdirectory()ing
# this whole project (see samples/rtos/*/CMakeLists.txt) -- GCutil's own
# CMakeLists.txt picks its NOSYS source list/defines over the hosted ones
# when that option is ON, so one add_subdirectory() here covers both hosted
# and bare-metal builds. gc-lib's PUBLIC include dir propagates automatically
# via the TARGET_LINK_LIBRARIES() calls below, so no separate INCDIRS entry
# is needed for it.
ADD_SUBDIRECTORY (third_party/GCutil)
SET (ESCARGOT_LIBRARIES ${ESCARGOT_LIBRARIES} gc-lib)

#######################################################
# SIMDUTF
#######################################################
ADD_LIBRARY (libsimdutf STATIC
    ${ESCARGOT_THIRD_PARTY_ROOT}/simdutf/simdutf.cpp)
TARGET_INCLUDE_DIRECTORIES (libsimdutf PUBLIC ${ESCARGOT_THIRD_PARTY_ROOT}/simdutf)
SET (LIBSIMDUTF_CXXFLAGS
    ${ESCARGOT_CXXFLAGS}
    ${ESCARGOT_THIRDPARTY_CFLAGS}
    ${CXXFLAGS_FROM_ENV}
# disable simd optimization
    -DSIMDUTF_IMPLEMENTATION_ARM64=0
    -DSIMDUTF_IMPLEMENTATION_ICELAKE=0
    -DSIMDUTF_IMPLEMENTATION_HASWELL=0
    -DSIMDUTF_IMPLEMENTATION_WESTMERE=0
    -DSIMDUTF_IMPLEMENTATION_PPC64=0
    -DSIMDUTF_IMPLEMENTATION_RVV=0
    -DSIMDUTF_IMPLEMENTATION_LSX=0
    -DSIMDUTF_IMPLEMENTATION_FALLBACK=1
# ${ESCARGOT_CXXFLAGS_FROM_EXTERNAL} already included in ${CXXFLAGS_FROM_ENV}
    )

IF (${ESCARGOT_MODE} STREQUAL "debug")
    SET (LIBSIMDUTF_CXXFLAGS ${ESCARGOT_CXXFLAGS_DEBUG} ${LIBSIMDUTF_CXXFLAGS})
ELSEIF (${ESCARGOT_MODE} STREQUAL "release")
    SET (LIBSIMDUTF_CXXFLAGS ${ESCARGOT_CXXFLAGS_RELEASE} ${LIBSIMDUTF_CXXFLAGS})
ENDIF()

TARGET_COMPILE_OPTIONS (libsimdutf PRIVATE ${LIBSIMDUTF_CXXFLAGS})

SET (ESCARGOT_LIBRARIES ${ESCARGOT_LIBRARIES} libsimdutf)

#######################################################
# LIBBF
#######################################################
ADD_LIBRARY (libbf STATIC
    ${ESCARGOT_THIRD_PARTY_ROOT}/libbf/libbf.c
    ${ESCARGOT_THIRD_PARTY_ROOT}/libbf/cutils.c)
TARGET_INCLUDE_DIRECTORIES (libbf PUBLIC ${ESCARGOT_THIRD_PARTY_ROOT}/libbf)
SET (LIBBF_CFLAGS
	${ESCARGOT_THIRDPARTY_CFLAGS}
	${CFLAGS_FROM_ENV}
# ${ESCARGOT_CFLAGS_FROM_EXTERNAL} already included in ${CFLAGS_FROM_ENV}
    )

IF (${ESCARGOT_MODE} STREQUAL "debug")
    SET (LIBBF_CFLAGS ${ESCARGOT_CXXFLAGS_DEBUG} ${LIBBF_CFLAGS})
ELSEIF (${ESCARGOT_MODE} STREQUAL "release")
    SET (LIBBF_CFLAGS ${ESCARGOT_CXXFLAGS_RELEASE} ${LIBBF_CFLAGS})
ENDIF()

TARGET_COMPILE_OPTIONS (libbf PRIVATE ${LIBBF_CFLAGS})

SET (ESCARGOT_LIBRARIES ${ESCARGOT_LIBRARIES} libbf)

#######################################################
# RUNTIME ICU BINDER
#######################################################
SET (RIB_CFLAGS ${ESCARGOT_THIRDPARTY_CFLAGS})
SET (RIB_MODE ${ESCARGOT_MODE})
SET (RIB_CFLAGS_FROM_EXTERNAL ${ESCARGOT_CFLAGS_FROM_EXTERNAL})
# runtime_icu_binder unconditionally includes <dlfcn.h> (RuntimeICUBinder.cpp)
# to dlopen() the system ICU at runtime -- there is no ICU and no dlopen() on
# bare-metal/RTOS targets (newlib/nosys has no dlfcn.h at all), so this
# subdirectory must not even be configured/compiled for ESCARGOT_HOST=baremetal,
# not just left unlinked.
IF (NOT ${ESCARGOT_HOST} STREQUAL "baremetal")
    ADD_SUBDIRECTORY (third_party/runtime_icu_binder)

    IF (ESCARGOT_LIBICU_SUPPORT_WITH_DLOPEN)
        SET (ESCARGOT_LIBRARIES ${ESCARGOT_LIBRARIES} runtime-icu-binder-static)
    ENDIF()
ENDIF()

#######################################################
# WebAssembly (walrus)
#######################################################
IF (ESCARGOT_WASM)
    SET (WALRUS_CXXFLAGS
        ${ESCARGOT_THIRDPARTY_CFLAGS}
        -g3)
    SET (WASM_ARCH ${ESCARGOT_ARCH})
    SET (WALRUS_HOST ${ESCARGOT_HOST})
    SET (WALRUS_ARCH ${ESCARGOT_ARCH})
    SET (WALRUS_MODE ${ESCARGOT_MODE})
    SET (WALRUS_OUTPUT "shared_lib")
    SET (WALRUS_WASI OFF) # WASI should be OFF
    SET (WALRUS_EXTENDED_FEATURES ON) # enable extended features

    IF (${ESCARGOT_MODE} STREQUAL "release")
         SET (WALRUS_CXXFLAGS ${WALRUS_CXXFLAGS} ${ESCARGOT_CXXFLAGS_RELEASE})
    ENDIF()
    SET (WALRUS_CXXFLAGS_FROM_EXTERNAL ${ESCARGOT_CXXFLAGS_FROM_EXTERNAL})
    SET (WALRUS_LDFLAGS_FROM_EXTERNAL ${ESCARGOT_LDFLAGS_FROM_EXTERNAL})

    ADD_SUBDIRECTORY (third_party/walrus)
    SET (ESCARGOT_LIBRARIES ${ESCARGOT_LIBRARIES} walrus)
ENDIF()

MAKE_DIRECTORY(${CMAKE_BINARY_DIR}/escargot_generated/tmp)

# yarr/UnicodePatternTables.h
EXECUTE_PROCESS(
    COMMAND python3 ${PROJECT_SOURCE_DIR}/tools/code_generators/generateYarrUnicodePropertyTables.py ${PROJECT_SOURCE_DIR}/tools/unicode_data ${CMAKE_BINARY_DIR}/escargot_generated/tmp/UnicodePatternTables.h
    RESULT_VARIABLE GENERATE_RESULT
    OUTPUT_VARIABLE GENERATE_OUTPUT
    ERROR_VARIABLE GENERATE_ERROR
)

IF (NOT GENERATE_RESULT EQUAL 0)
    MESSAGE(STATUS "Output:\n${GENERATE_OUTPUT}")
    MESSAGE(FATAL_ERROR "${GENERATE_ERROR}")
ENDIF()

EXECUTE_PROCESS (COMMAND ${CMAKE_COMMAND} -E compare_files ${CMAKE_BINARY_DIR}/escargot_generated/tmp/UnicodePatternTables.h ${CMAKE_BINARY_DIR}/escargot_generated/yarr/UnicodePatternTables.h
                RESULT_VARIABLE COMPARE_RESULT
                OUTPUT_VARIABLE COMPARE_OUTPUT
                ERROR_VARIABLE COMPARE_ERROR
)

IF (NOT ${COMPARE_RESULT} EQUAL 0)
    FILE (COPY ${CMAKE_BINARY_DIR}/escargot_generated/tmp/UnicodePatternTables.h DESTINATION ${CMAKE_BINARY_DIR}/escargot_generated/yarr/)
ENDIF()

# yarr/SimpleCaseFoldingTable.h
EXECUTE_PROCESS(
    COMMAND python3 ${PROJECT_SOURCE_DIR}/tools/code_generators/generateSimpleCaseFoldingTable.py ${PROJECT_SOURCE_DIR}/tools/unicode_data ${CMAKE_BINARY_DIR}/escargot_generated/tmp/SimpleCaseFoldingTable.h
    RESULT_VARIABLE GENERATE_RESULT
    OUTPUT_VARIABLE GENERATE_OUTPUT
    ERROR_VARIABLE GENERATE_ERROR
)

IF (NOT GENERATE_RESULT EQUAL 0)
    MESSAGE(STATUS "Output:\n${GENERATE_OUTPUT}")
    MESSAGE(FATAL_ERROR "${GENERATE_ERROR}")
ENDIF()

EXECUTE_PROCESS (COMMAND ${CMAKE_COMMAND} -E compare_files ${CMAKE_BINARY_DIR}/escargot_generated/tmp/SimpleCaseFoldingTable.h ${CMAKE_BINARY_DIR}/escargot_generated/yarr/SimpleCaseFoldingTable.h
                RESULT_VARIABLE COMPARE_RESULT
                OUTPUT_VARIABLE COMPARE_OUTPUT
                ERROR_VARIABLE COMPARE_ERROR
)

IF (NOT ${COMPARE_RESULT} EQUAL 0)
    FILE (COPY ${CMAKE_BINARY_DIR}/escargot_generated/tmp/SimpleCaseFoldingTable.h DESTINATION ${CMAKE_BINARY_DIR}/escargot_generated/yarr/)
ENDIF()

SET (ESCARGOT_INCDIRS
    ${ESCARGOT_INCDIRS}
    ${CMAKE_BINARY_DIR}/escargot_generated/yarr/
)

# BUILD
IF (${ESCARGOT_OUTPUT} STREQUAL "shell")
    ADD_EXECUTABLE (${ESCARGOT_TARGET} ${ESCARGOT_SRC_LIST})

    TARGET_LINK_LIBRARIES (${ESCARGOT_TARGET} PRIVATE ${ESCARGOT_LIBRARIES} ${ESCARGOT_LDFLAGS} ${LDFLAGS_FROM_ENV})
    TARGET_INCLUDE_DIRECTORIES (${ESCARGOT_TARGET} PRIVATE ${ESCARGOT_INCDIRS})
    TARGET_COMPILE_DEFINITIONS (${ESCARGOT_TARGET} PRIVATE ${ESCARGOT_DEFINITIONS})
    TARGET_COMPILE_OPTIONS (${ESCARGOT_TARGET} PRIVATE ${ESCARGOT_CXXFLAGS} ${CXXFLAGS_FROM_ENV} ${PROFILER_FLAGS})

ELSEIF (${ESCARGOT_OUTPUT} STREQUAL "shared_lib")
    ADD_LIBRARY (${ESCARGOT_TARGET} SHARED ${ESCARGOT_SRC_LIST})

    TARGET_LINK_LIBRARIES (${ESCARGOT_TARGET} PUBLIC ${ESCARGOT_LIBRARIES} ${ESCARGOT_LDFLAGS} ${LDFLAGS_FROM_ENV})
    TARGET_INCLUDE_DIRECTORIES (${ESCARGOT_TARGET} PRIVATE ${ESCARGOT_INCDIRS})
    TARGET_INCLUDE_DIRECTORIES (${ESCARGOT_TARGET} PUBLIC ${ESCARGOT_ROOT}/src/api)
    TARGET_COMPILE_DEFINITIONS (${ESCARGOT_TARGET} PRIVATE ${ESCARGOT_DEFINITIONS})
    TARGET_COMPILE_OPTIONS (${ESCARGOT_TARGET} PRIVATE ${ESCARGOT_CXXFLAGS} ${CXXFLAGS_FROM_ENV})

ELSEIF (${ESCARGOT_OUTPUT} STREQUAL "static_lib")
    ADD_LIBRARY (${ESCARGOT_TARGET} STATIC ${ESCARGOT_SRC_LIST})

    TARGET_LINK_LIBRARIES (${ESCARGOT_TARGET} PUBLIC ${ESCARGOT_LIBRARIES} ${ESCARGOT_LDFLAGS} ${LDFLAGS_FROM_ENV})
    TARGET_INCLUDE_DIRECTORIES (${ESCARGOT_TARGET} PRIVATE ${ESCARGOT_INCDIRS})
    TARGET_INCLUDE_DIRECTORIES (${ESCARGOT_TARGET} PUBLIC ${ESCARGOT_ROOT}/src/api)
    TARGET_COMPILE_DEFINITIONS (${ESCARGOT_TARGET} PRIVATE ${ESCARGOT_DEFINITIONS})
    TARGET_COMPILE_OPTIONS (${ESCARGOT_TARGET} PRIVATE ${ESCARGOT_CXXFLAGS} ${CXXFLAGS_FROM_ENV})

ELSEIF (${ESCARGOT_OUTPUT} STREQUAL "cctest")
    ADD_EXECUTABLE (${ESCARGOT_CCTEST_TARGET} ${ESCARGOT_SRC_LIST})

    TARGET_LINK_LIBRARIES (${ESCARGOT_CCTEST_TARGET} PRIVATE ${ESCARGOT_LIBRARIES} ${ESCARGOT_LDFLAGS} ${LDFLAGS_FROM_ENV} gtest gtest_main)
    TARGET_INCLUDE_DIRECTORIES (${ESCARGOT_CCTEST_TARGET} PRIVATE ${ESCARGOT_INCDIRS})
    TARGET_COMPILE_DEFINITIONS (${ESCARGOT_CCTEST_TARGET} PRIVATE ${ESCARGOT_DEFINITIONS})
    TARGET_COMPILE_OPTIONS (${ESCARGOT_CCTEST_TARGET} PRIVATE ${ESCARGOT_CXXFLAGS} ${CXXFLAGS_FROM_ENV})

    IF (ESCARGOT_NAPI)
        # build real Node-API TCs (vendored under test/napi-tc) into .so files the
        # cctest binary dlopen()s directly. Add the TC's source file(s), relative
        # to test/js-native-api/, here as more TCs get supported; join multiple
        # sources for one addon with `|` (see 7_factory_wrap). The .so is named
        # after the FIRST source's stem (matching its binding.gyp target_name),
        # not necessarily its containing directory.
        SET (NAPI_TEST_ADDON_DIR ${CMAKE_BINARY_DIR}/napi_test_addons)
        SET (NAPI_TEST_TC_ENTRIES
            2_function_arguments/2_function_arguments.c
            3_callbacks/3_callbacks.c
            4_object_factory/4_object_factory.c
            5_function_factory/5_function_factory.c
            6_object_wrap/myobject.cc
            7_factory_wrap/7_factory_wrap.cc|7_factory_wrap/myobject.cc
            test_handle_scope/test_handle_scope.c
            # --- test/cctest/napi_harness (NapiSuite.*) additions below ---
            test_number/test_number.c|test_number/test_null.c
            test_string/test_string.c|test_string/test_null.c
            test_object/test_exceptions.c
            test_array/test_array.c
            test_conversions/test_conversions.c|test_conversions/test_null.c
            test_properties/test_properties.c
            test_constructor/test_constructor.c|test_constructor/test_null.c
            test_symbol/test_symbol.c
            test_bigint/test_bigint.c
            test_error/test_error.c
            test_exception/test_exception.c
            test_typedarray/test_typedarray.c
            test_typedarray/test_typedarray_sharedarraybuffer.c
            test_date/test_date.c
            test_new_target/test_new_target.c
            test_reference/test_reference.c
            # test_reference/binding.gyp and test_finalizer/binding.gyp each
            # independently declare a target_name "test_finalizer" backed by
            # a *different* source file; disambiguate the one from
            # test_reference/ with an explicit output name (see the `=`
            # override support in the loop below) so it doesn't collide with
            # test_finalizer/test_finalizer.c's own entry name.
            test_reference/test_finalizer.c=test_reference_test_finalizer
            test_promise/test_promise.c
            test_function/test_function.c
            test_instance_data/test_instance_data.c
            8_passing_wrapped/8_passing_wrapped.cc|8_passing_wrapped/myobject.cc
            # A second, independently-built copy of 7_factory_wrap's own addon
            # (identical source, different output .so) - NapiSuite.FactoryWrap
            # (testnapi_suite.cpp) dlopen()s *this* one instead of the same
            # .so Napi.FactoryWrap (testnapi.cpp) uses, so the two suites don't
            # share that addon's static `finalizeCount`/`instanceCount`
            # counters (see the NapiSuite report's cross-suite isolation note -
            # neither suite ever dlclose()s, and test.js's own
            # `assert.strictEqual(test.finalizeCount, 0)` at the top requires
            # a fresh-from-zero counter every run).
            7_factory_wrap/7_factory_wrap.cc|7_factory_wrap/myobject.cc=7_factory_wrap_napisuite
            # These four now compile (node_api_create_object_with_properties/
            # node_api_set_prototype/node_api_post_finalizer/
            # node_api_is_sharedarraybuffer are declared - see the
            # node_api_is_sharedarraybuffer are declared in Node.js's own
            # src/js_native_api.h (fetched via the test/napi-tc submodule's
            # sparse checkout, not the standalone node-api-headers package
            # which lags behind) - and implemented
            # in src/napi/NapiExtras.cpp). test_general/testEnvCleanup.js and
            # test_finalizer/test_fatal_finalize.js are now wired into
            # NapiSuite.* TESTs (test/cctest/testnapi_suite.cpp) - both are
            # child_process.spawnSync-based self-respawn tests, supported via
            # the harness's single-test CLI mode (--napi-run, same file).
            # test_general/test.js and test_finalizer/test.js themselves (as
            # opposed to the specific files above) still aren't wired in
            # (worker_threads-style respawn, which this harness has no shim
            # for); test_object/test.js and test_dataview/test.js are simply
            # not yet ported. Listed here only to keep them compiling as
            # further evidence the addon-level gap is closed.
            test_object/test_object.c
            test_general/test_general.c
            test_dataview/test_dataview.c
            test_finalizer/test_finalizer.c
            # Now ported to real test.js under NapiSuite.* (test/cctest/
            # testnapi_suite.cpp): test_dataview/test.js (above),
            # test_sharedarraybuffer/test.js (needs node_api_create_
            # sharedarraybuffer, implemented in src/napi/NapiExtras.cpp and
            # declared in Node.js's own src/js_native_api.h), and
            # test_reference_double_free/test.js (standard wrap/remove_wrap
            # double-free regression, no new API needed).
            test_sharedarraybuffer/test_sharedarraybuffer.c
            test_reference_double_free/test_reference_double_free.c
        )
        # Tier 1 & 2 node-api/ addons (real test.js under NapiSuite.NodeApi*).
        # Same entry grammar as NAPI_TEST_TC_ENTRIES, but sources resolve
        # under test/napi-tc/test/node-api/ instead of js-native-api/. Many
        # node-api addons name their single source binding.c/binding.cc, so
        # the =explicitName override (derived name would collide as "binding")
        # is used heavily here; test_general/test_exception also exist under
        # js-native-api/, so their node-api builds get a node_api_ prefix to
        # avoid the shared SO_PATH compile-def macro colliding.
        SET (NAPI_TEST_TC_ENTRIES_NODE_API
            test_uv_loop/test_uv_loop.cc
            # Tier 1 (no worker_threads/async_hooks/child_process deps)
            test_env_teardown_gc/binding.c=test_env_teardown_gc
            test_fatal_exception/test_fatal_exception.c
            test_init_order/test_init_order.cc
            test_make_callback/binding.c=test_make_callback
            test_make_callback_recurse/binding.c=test_make_callback_recurse
            test_callback_scope/binding.c=test_callback_scope
            # test_buffer deferred: this vendored copy's test_buffer.c calls the
            # non-standard node_api_create_external_sharedarraybuffer and the
            # test.js uses the global Buffer (not provided by this harness).
            test_threadsafe_function_abort/binding.cc=test_threadsafe_function_abort
            # Tier 2 (child_process self-respawn via the harness --napi-run shim)
            test_async/test_async.c
            test_cleanup_hook/binding.c=test_cleanup_hook
            test_fatal/test_fatal.c
            test_threadsafe_function/binding.c=test_threadsafe_function
            test_threadsafe_function_shutdown/binding.cc=test_threadsafe_function_shutdown
        )
        SET (NAPI_TEST_ADDON_SOS)
        # process one entries list, resolving each source relative to
        # test/napi-tc/test/${_subdir}/ (js-native-api or node-api).
        MACRO (BUILD_NAPI_TEST_ADDONS _subdir)
        FOREACH (NAPI_TEST_TC_ENTRY ${ARGN})
            # optional `=explicitName` suffix to override the derived output
            # name, for the rare case where two addons in different test
            # directories legitimately share a binding.gyp target_name (and
            # would otherwise produce the same .so path/compile-def macro)
            STRING (REPLACE "=" ";" NAPI_TEST_TC_ENTRY_PARTS ${NAPI_TEST_TC_ENTRY})
            LIST (LENGTH NAPI_TEST_TC_ENTRY_PARTS NAPI_TEST_TC_ENTRY_NPARTS)
            LIST (GET NAPI_TEST_TC_ENTRY_PARTS 0 NAPI_TEST_TC_ENTRY_SRCPART)

            STRING (REPLACE "|" ";" NAPI_TEST_TC_SRC_RELS ${NAPI_TEST_TC_ENTRY_SRCPART})
            SET (NAPI_TEST_TC_SRCS)
            FOREACH (NAPI_TEST_TC_SRC_REL ${NAPI_TEST_TC_SRC_RELS})
                LIST (APPEND NAPI_TEST_TC_SRCS ${ESCARGOT_ROOT}/test/napi-tc/test/${_subdir}/${NAPI_TEST_TC_SRC_REL})
            ENDFOREACH()

            LIST (GET NAPI_TEST_TC_SRC_RELS 0 NAPI_TEST_TC_FIRST_SRC_REL)
            IF (${NAPI_TEST_TC_ENTRY_NPARTS} GREATER 1)
                LIST (GET NAPI_TEST_TC_ENTRY_PARTS 1 NAPI_TEST_TC_NAME)
            ELSE()
                GET_FILENAME_COMPONENT (NAPI_TEST_TC_NAME ${NAPI_TEST_TC_FIRST_SRC_REL} NAME_WE)
            ENDIF()
            GET_FILENAME_COMPONENT (NAPI_TEST_TC_EXT ${NAPI_TEST_TC_FIRST_SRC_REL} EXT)
            SET (NAPI_TEST_TC_SO ${NAPI_TEST_ADDON_DIR}/${NAPI_TEST_TC_NAME}.so)

            IF (${NAPI_TEST_TC_EXT} STREQUAL ".c")
                SET (NAPI_TEST_TC_COMPILER ${CMAKE_C_COMPILER})
            ELSE()
                SET (NAPI_TEST_TC_COMPILER ${CMAKE_CXX_COMPILER})
            ENDIF()

            ADD_CUSTOM_COMMAND (
                OUTPUT ${NAPI_TEST_TC_SO}
                COMMAND ${CMAKE_COMMAND} -E make_directory ${NAPI_TEST_ADDON_DIR}
                COMMAND ${NAPI_TEST_TC_COMPILER} -shared -fPIC -DNAPI_VERSION=10 -I${ESCARGOT_ROOT}/test/napi-tc/src ${NAPI_TEST_TC_SRCS} -o ${NAPI_TEST_TC_SO}
                DEPENDS ${NAPI_TEST_TC_SRCS}
                COMMENT "Building napi test addon ${NAPI_TEST_TC_NAME}.so"
            )
            LIST (APPEND NAPI_TEST_ADDON_SOS ${NAPI_TEST_TC_SO})

            STRING (TOUPPER ${NAPI_TEST_TC_NAME} NAPI_TEST_TC_NAME_UPPER)
            TARGET_COMPILE_DEFINITIONS (${ESCARGOT_CCTEST_TARGET} PRIVATE NAPI_${NAPI_TEST_TC_NAME_UPPER}_SO_PATH="${NAPI_TEST_TC_SO}")
        ENDFOREACH()
        ENDMACRO()
        BUILD_NAPI_TEST_ADDONS (js-native-api ${NAPI_TEST_TC_ENTRIES})
        BUILD_NAPI_TEST_ADDONS (node-api ${NAPI_TEST_TC_ENTRIES_NODE_API})

        # Custom (non-upstream) test addons live in the main repo tree under
        # test/cctest/napi_custom_addons/, NOT in the test/napi-tc submodule
        # (which is a sparse checkout of nodejs/node and gets re-cloned in CI).
        # Build them the same way as the macro above, just from a different
        # source root.
        SET (NAPI_CUSTOM_ADDON_DIR ${ESCARGOT_ROOT}/test/cctest/napi_custom_addons)
        SET (NAPI_CUSTOM_SYMBOL_VERIFY_SO ${NAPI_TEST_ADDON_DIR}/test_symbol_verify.so)
        SET (NAPI_CUSTOM_SYMBOL_VERIFY_SRC ${NAPI_CUSTOM_ADDON_DIR}/test_symbol_verify/test_symbol_verify.c)
        ADD_CUSTOM_COMMAND (
            OUTPUT ${NAPI_CUSTOM_SYMBOL_VERIFY_SO}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${NAPI_TEST_ADDON_DIR}
            COMMAND ${CMAKE_C_COMPILER} -shared -fPIC -DNAPI_VERSION=10 -I${ESCARGOT_ROOT}/test/napi-tc/src -I${ESCARGOT_ROOT}/test/napi-tc/test/js-native-api ${NAPI_CUSTOM_SYMBOL_VERIFY_SRC} -o ${NAPI_CUSTOM_SYMBOL_VERIFY_SO}
            DEPENDS ${NAPI_CUSTOM_SYMBOL_VERIFY_SRC}
            COMMENT "Building napi custom test addon test_symbol_verify.so"
        )
        LIST (APPEND NAPI_TEST_ADDON_SOS ${NAPI_CUSTOM_SYMBOL_VERIFY_SO})
        TARGET_COMPILE_DEFINITIONS (${ESCARGOT_CCTEST_TARGET} PRIVATE NAPI_TEST_SYMBOL_VERIFY_SO_PATH="${NAPI_CUSTOM_SYMBOL_VERIFY_SO}")

        ADD_CUSTOM_TARGET (napi_test_addons ALL DEPENDS ${NAPI_TEST_ADDON_SOS})
        ADD_DEPENDENCIES (${ESCARGOT_CCTEST_TARGET} napi_test_addons)

        # test/cctest/testnapi_suite.cpp (NapiSuite.*): the JS compatibility
        # harness (require()/assert/common shims) and the base directory the
        # real Node js-native-api test.js files live under, so the suite can
        # resolve `test_number` -> `<dir>/test_number/test.js` etc. at runtime
        # without hardcoding the repo layout into the .cpp itself.
        TARGET_COMPILE_DEFINITIONS (${ESCARGOT_CCTEST_TARGET} PRIVATE NAPI_HARNESS_JS_PATH="${ESCARGOT_ROOT}/test/cctest/napi_harness/harness.js")
        TARGET_COMPILE_DEFINITIONS (${ESCARGOT_CCTEST_TARGET} PRIVATE NAPI_TC_JS_DIR="${ESCARGOT_ROOT}/test/napi-tc/test/js-native-api")
        TARGET_COMPILE_DEFINITIONS (${ESCARGOT_CCTEST_TARGET} PRIVATE NAPI_TC_NODE_API_JS_DIR="${ESCARGOT_ROOT}/test/napi-tc/test/node-api")
        TARGET_COMPILE_DEFINITIONS (${ESCARGOT_CCTEST_TARGET} PRIVATE NAPI_CUSTOM_ADDON_JS_DIR="${ESCARGOT_ROOT}/test/cctest/napi_custom_addons")
    ENDIF()
ENDIF()
