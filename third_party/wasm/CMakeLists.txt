PROJECT(WASM)
CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (NOT "${CMAKE_PROJECT_VERSION}")
  set(CMAKE_PROJECT_VERSION "1.0.29")
endif ()

if (MSVC)
  set(COMPILER_IS_CLANG 0)
  set(COMPILER_IS_GNU 0)
  set(COMPILER_IS_MSVC 1)
elseif (CMAKE_C_COMPILER_ID MATCHES "Clang")
  set(COMPILER_IS_CLANG 1)
  set(COMPILER_IS_GNU 0)
  set(COMPILER_IS_MSVC 0)
elseif (CMAKE_C_COMPILER_ID STREQUAL "GNU")
  set(COMPILER_IS_CLANG 0)
  set(COMPILER_IS_GNU 1)
  set(COMPILER_IS_MSVC 0)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  set(COMPILER_IS_CLANG 1)
  set(COMPILER_IS_GNU 0)
  set(COMPILER_IS_MSVC 0)
else ()
  set(COMPILER_IS_CLANG 0)
  set(COMPILER_IS_GNU 0)
  set(COMPILER_IS_MSVC 0)
endif ()

include(CheckIncludeFile)
include(CheckSymbolExists)
check_include_file("alloca.h" HAVE_ALLOCA_H)
check_include_file("unistd.h" HAVE_UNISTD_H)
check_symbol_exists(snprintf "stdio.h" HAVE_SNPRINTF)
check_symbol_exists(strcasecmp "strings.h" HAVE_STRCASECMP)

include(CheckTypeSize)
check_type_size(ssize_t SSIZE_T)
#check_type_size(size_t SIZEOF_SIZE_T)

# Escargot specific option (for the case of compilation to 32bit from 64bit)
if (ESCARGOT_BUILD_32BIT)
    set(SIZEOF_SIZE_T 4)
else ()
    set(SIZEOF_SIZE_T 8)
endif ()

set(WABT_ROOT ${PROJECT_SOURCE_DIR}/wabt)
configure_file(${WABT_ROOT}/src/config.h.in ${PROJECT_SOURCE_DIR}/config.h)

include_directories(${WABT_ROOT} ${PROJECT_SOURCE_DIR})

set(WABT_SRC
    ${PROJECT_SOURCE_DIR}/config.h

    wabt/src/apply-names.h
    wabt/src/apply-names.cc
    wabt/src/binary.h
    wabt/src/binary.cc
    wabt/src/binary-reader.h
    wabt/src/binary-reader.cc
    wabt/src/binary-reader-ir.h
    wabt/src/binary-reader-ir.cc
    wabt/src/binary-reader-logging.h
    wabt/src/binary-reader-logging.cc
    wabt/src/binary-writer.h
    wabt/src/binary-writer.cc
    wabt/src/binary-writer-spec.h
    wabt/src/binary-writer-spec.cc
    wabt/src/binding-hash.h
    wabt/src/binding-hash.cc
    wabt/src/color.h
    wabt/src/color.cc
    wabt/src/common.h
    wabt/src/common.cc
    wabt/src/config.cc
    wabt/src/decompiler.h
    wabt/src/decompiler-ast.h
    wabt/src/decompiler-ls.h
    wabt/src/decompiler-naming.h
    wabt/src/decompiler.cc
    wabt/src/error-formatter.h
    wabt/src/error-formatter.cc
    wabt/src/expr-visitor.h
    wabt/src/expr-visitor.cc
    wabt/src/feature.h
    wabt/src/feature.cc
    wabt/src/filenames.h
    wabt/src/filenames.cc
    wabt/src/generate-names.h
    wabt/src/generate-names.cc
    wabt/src/ir.h
    wabt/src/ir.cc
    wabt/src/ir-util.h
    wabt/src/ir-util.cc
    wabt/src/leb128.h
    wabt/src/leb128.cc
    wabt/src/lexer-source.h
    wabt/src/lexer-source.cc
    wabt/src/lexer-source-line-finder.h
    wabt/src/lexer-source-line-finder.cc
    wabt/src/literal.h
    wabt/src/literal.cc
    wabt/src/opcode.h
    wabt/src/opcode.cc
    wabt/src/opcode-code-table.h
    wabt/src/opcode-code-table.c
    wabt/src/option-parser.h
    wabt/src/option-parser.cc
    wabt/src/resolve-names.h
    wabt/src/resolve-names.cc
    wabt/src/shared-validator.h
    wabt/src/shared-validator.cc
    wabt/src/stream.h
    wabt/src/stream.cc
    wabt/src/string-util.h
    wabt/src/token.h
    wabt/src/token.cc
    wabt/src/tracing.h
    wabt/src/tracing.cc
    wabt/src/type.h
    wabt/src/type-checker.h
    wabt/src/type-checker.cc
    wabt/src/utf8.h
    wabt/src/utf8.cc
    wabt/src/validator.h
    wabt/src/validator.cc
    wabt/src/wast-lexer.h
    wabt/src/wast-lexer.cc
    wabt/src/wast-parser.h
    wabt/src/wast-parser.cc
    wabt/src/wat-writer.h
    wabt/src/wat-writer.cc

    # TODO(binji): Move this into its own library?
    wabt/src/interp/binary-reader-interp.h
    wabt/src/interp/binary-reader-interp.cc
    wabt/src/interp/interp.h
    wabt/src/interp/interp.cc
    wabt/src/interp/interp-inl.h
    wabt/src/interp/interp-math.h
    wabt/src/interp/interp-util.h
    wabt/src/interp/interp-util.cc
    wabt/src/interp/istream.h
    wabt/src/interp/istream.cc
)

# disable -Wpointer-arith: this is a GCC extension, and doesn't work in MSVC.
set(WASM_CXX_FLAGS_INTERNAL
    -Wall -Wextra -Werror -Wno-unused-parameter -Wpointer-arith
    -Wuninitialized -fPIC -fdata-sections -ffunction-sections
)

# set c++ flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wold-style-cast")

# Need to define __STDC_*_MACROS because C99 specifies that C++ shouldn't
# define format (e.g. PRIu64) or limit (e.g. UINT32_MAX) macros without the
# definition, and some libcs (e.g. glibc2.17 and earlier) follow that.
set(WASM_CXX_FLAGS_INTERNAL ${WASM_CXX_FLAGS_INTERNAL} -D__STDC_LIMIT_MACROS=1 -D__STDC_FORMAT_MACROS=1 -DNDEBUG)

if (COMPILER_IS_GNU)
# disable -Wclobbered: it seems to be guessing incorrectly about a local
# variable being clobbered by longjmp.
    set(WASM_CXX_FLAGS_INTERNAL ${WASM_CXX_FLAGS_INTERNAL} -Wno-clobbered)
endif ()

if (WASM_ARCH STREQUAL "x86")
# wasm doesn't allow for x87 floating point math
    set(WASM_CXX_FLAGS_INTERNAL ${WASM_CXX_FLAGS_INTERNAL} -msse2 -mfpmath=sse)
endif ()

add_compile_options(${WASM_CXX_FLAGS_INTERNAL})
add_compile_options(${WASM_CXX_FLAGS})

set(WASM_CFLAGS_FROM_ENV $ENV{CFLAGS})
separate_arguments(WASM_CFLAGS_FROM_ENV)
add_compile_options(${WASM_CFLAGS_FROM_ENV})

# build wabt lib
add_library(wabt STATIC ${WABT_SRC})

# build wasm lib
add_library(wasm SHARED wabt/src/interp/interp-wasm-c-api.cc)
target_link_libraries(wasm wabt -Wl,--gc-sections ${WASM_CXX_FLAGS})
target_include_directories(wasm PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_compile_options(wasm PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wno-old-style-cast>)
target_compile_definitions(wasm PRIVATE "WASM_API_EXTERN=__attribute__((visibility(\"default\")))")
set_target_properties(wasm PROPERTIES CXX_VISIBILITY_PRESET hidden)
