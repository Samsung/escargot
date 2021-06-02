project (WASM)
cmake_minimum_required (VERSION 2.8)

IF (NOT CMAKE_BUILD_TYPE)
    SET (CMAKE_BUILD_TYPE Debug)
ENDIF()

set(CMAKE_CXX_STANDARD 14)

string (TOLOWER ${CMAKE_HOST_SYSTEM_NAME} WAMR_BUILD_PLATFORM)
if (APPLE)
  add_definitions(-DBH_PLATFORM_DARWIN)
endif ()

# Resetdefault linker flags
set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")
set(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS "")

# WAMR features switch

# Set WAMR_BUILD_TARGET, currently values supported:
# "X86_64", "AMD_64", "X86_32", "AARCH64[sub]", "ARM[sub]", "THUMB[sub]",
# "MIPS", "XTENSA", "RISCV64[sub]", "RISCV32[sub]"
if (NOT DEFINED WAMR_BUILD_TARGET)
  if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64)")
    set (WAMR_BUILD_TARGET "AARCH64")
  elseif (CMAKE_SYSTEM_PROCESSOR STREQUAL "riscv64")
    set (WAMR_BUILD_TARGET "RISCV64")
  elseif (CMAKE_SIZEOF_VOID_P EQUAL 8)
    # Build as X86_64 by default in 64-bit platform
    set (WAMR_BUILD_TARGET "X86_64")
  elseif (CMAKE_SIZEOF_VOID_P EQUAL 4)
    # Build as X86_32 by default in 32-bit platform
    set (WAMR_BUILD_TARGET "X86_32")
  else ()
    message(SEND_ERROR "Unsupported build target platform!")
  endif ()
endif ()

# enable interpreter mode by default
set(WAMR_BUILD_INTERP 1)

# disable AOT and JIT by default
set(WAMR_BUILD_AOT 0)
set(WAMR_BUILD_JIT 0)

set(WAMR_BUILD_LIBC_BUILTIN 1)
set(WAMR_BUILD_LIBC_WASI 0)
set(WAMR_BUILD_MULTI_MODULE 1)
set(WAMR_BUILD_DUMP_CALL_STACK 1)
set(WAMR_BUILD_REF_TYPES 1)

if(NOT DEFINED WAMR_BUILD_FAST_INTERP)
  set(WAMR_BUILD_FAST_INTERP 1)
endif()

if (NOT MSVC)
  # compiling and linking flags
  set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pie -fPIE")
  if (NOT (CMAKE_C_COMPILER MATCHES ".*clang.*" OR CMAKE_C_COMPILER_ID MATCHES ".*Clang"))
    set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--gc-sections")
  endif ()
  set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wformat -Wformat-security")
  if (WAMR_BUILD_TARGET MATCHES "X86_.*" OR WAMR_BUILD_TARGET STREQUAL "AMD_64")
    if (NOT (CMAKE_C_COMPILER MATCHES ".*clang.*" OR CMAKE_C_COMPILER_ID MATCHES ".*Clang"))
      set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mindirect-branch-register")
    endif ()
  endif ()
endif()

# set(WASM_CFLAGS_FROM_ENV $ENV{CFLAGS})
# separate_arguments(WASM_CFLAGS_FROM_ENV)
# add_compile_options(${WASM_CFLAGS_FROM_ENV})

# build out wamr lib
set(WAMR_ROOT_DIR ${CMAKE_CURRENT_LIST_DIR}/wamr)
include (${WAMR_ROOT_DIR}/build-scripts/runtime_lib.cmake)

add_library(wasm SHARED ${WAMR_RUNTIME_LIB_SOURCE})
if (MSVC)
  target_compile_definitions(wasm PRIVATE WASM_API_EXTERN=)
endif()
target_link_libraries (wasm ${LLVM_AVAILABLE_LIBS} ${UV_A_LIBS} -lm -ldl -lpthread)
target_include_directories(wasm PUBLIC ${WAMR_ROOT_DIR}/core/iwasm/include)

# TARGET_COMPILE_DEFINITIONS(wasm PRIVATE "WASM_API_EXTERN=__attribute__((visibility(\"default\")))")
# SET_TARGET_PROPERTIES(wasm PROPERTIES CXX_VISIBILITY_PRESET hidden)

# add compile options from parent scope
target_compile_options(wasm PRIVATE ${WASM_CFLAGS})
