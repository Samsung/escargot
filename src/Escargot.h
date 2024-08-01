/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 */

#ifndef __Escargot__
#define __Escargot__

#include <algorithm>
#include <cassert>
#include <climits>
#include <clocale>
#include <cmath>
#include <csetjmp>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <functional>
#include <limits>
#include <list>
#include <locale>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <random>
#include <queue>
#include <chrono>

/* COMPILER() - the compiler being used to build the project */
// Outdated syntax: use defined()
// #define COMPILER(FEATURE) (defined COMPILER_##FEATURE && COMPILER_##FEATURE)

#if defined(__clang__)
#define COMPILER_CLANG 1
// clang-cl defines _MSC_VER and __clang__ both
#if defined(_MSC_VER)
#define COMPILER_CLANG_CL 1
#endif
#elif defined(_MSC_VER)
#define COMPILER_MSVC 1
#elif (__GNUC__)
#define COMPILER_GCC 1
#else
#error "Compiler dectection failed"
#endif

#if defined(COMPILER_CLANG)
/* Check clang version */
#if __clang_major__ < 6
#error "Clang version is low (require clang version 6+)"
#endif
/* Keep strong enums turned off when building with clang-cl: We cannot yet build all of Blink without fallback to cl.exe, and strong enums are exposed at ABI boundaries. */
#undef COMPILER_SUPPORTS_CXX_STRONG_ENUMS
#else
#define COMPILER_SUPPORTS_CXX_OVERRIDE_CONTROL 1
#define COMPILER_QUIRK_FINAL_IS_CALLED_SEALED 1
#endif

#if defined(ENABLE_CODE_CACHE) && defined(ESCARGOT_DEBUGGER)
#error "Code Cache does not support Debugger mode"
#endif

/* ALWAYS_INLINE */
#ifndef ALWAYS_INLINE
#if defined(ESCARGOT_SMALL_CONFIG)
#define ALWAYS_INLINE inline
#else
#if (defined(COMPILER_GCC) || defined(COMPILER_CLANG)) && defined(NDEBUG) && !defined(COMPILER_MINGW)
#define ALWAYS_INLINE inline __attribute__((__always_inline__))
#elif defined(COMPILER_MSVC) && defined(NDEBUG)
#define ALWAYS_INLINE __forceinline
#else
#define ALWAYS_INLINE inline
#endif
#endif
#endif

/* NEVER_INLINE */
#ifndef NEVER_INLINE
#if defined(COMPILER_GCC) || defined(COMPILER_CLANG)
#define NEVER_INLINE __attribute__((__noinline__))
#else
#define NEVER_INLINE
#endif
#endif

/* UNLIKELY */
#ifndef UNLIKELY
#if defined(COMPILER_GCC) || defined(COMPILER_CLANG)
#define UNLIKELY(x) __builtin_expect((x), 0)
#else
#define UNLIKELY(x) (x)
#endif
#endif


/* LIKELY */
#ifndef LIKELY
#if defined(COMPILER_GCC) || defined(COMPILER_CLANG)
#define LIKELY(x) __builtin_expect((x), 1)
#else
#define LIKELY(x) (x)
#endif
#endif


/* NO_RETURN */
#ifndef NO_RETURN
#if defined(COMPILER_GCC) || defined(COMPILER_CLANG)
#define NO_RETURN __attribute((__noreturn__))
#elif defined(COMPILER_MSVC)
#define NO_RETURN __declspec(noreturn)
#else
#define NO_RETURN
#endif
#endif

/* EXPORT */
#ifndef EXPORT
#if defined(COMPILER_MSVC)
#define EXPORT __declspec(dllexport)
#else
#define EXPORT __attribute__((visibility("default")))
#endif
#endif

/* PREFETCH_READ */
#ifndef PREFETCH_READ
#if defined(COMPILER_GCC) || defined(COMPILER_CLANG)
#define PREFETCH_READ(x) __builtin_prefetch((x), 0, 0)
#else
#define PREFETCH_READ(x)
#endif
#endif

/* LOG2 */
#ifndef FAST_LOG2_UINT
#if defined(COMPILER_GCC) || defined(COMPILER_CLANG)
#define FAST_LOG2_UINT(x) ((unsigned)(8 * sizeof(unsigned long long) - __builtin_clzll((x)) - 1))
#else
#define FAST_LOG2_UINT(x) log2l(x)
#endif
#endif

#ifndef ATTRIBUTE_NO_SANITIZE_ADDRESS
#if defined(COMPILER_GCC) || defined(COMPILER_CLANG)
#define ATTRIBUTE_NO_SANITIZE_ADDRESS __attribute__((no_sanitize_address))
#else
#define ATTRIBUTE_NO_SANITIZE_ADDRESS
#endif
#endif

#ifndef ATTRIBUTE_NO_OPTIMIZE
#if defined(COMPILER_GCC)
#define ATTRIBUTE_NO_OPTIMIZE __attribute__((optimize("O0")))
#elif defined(COMPILER_CLANG)
#define ATTRIBUTE_NO_OPTIMIZE [[clang::optnone]]
#else
#define ATTRIBUTE_NO_OPTIMIZE
#endif
#endif

// #define OS(NAME) (defined OS_##NAME && OS_##NAME)

#ifdef _WIN32
#define OS_WINDOWS 1
#elif _WIN64
#define OS_WINDOWS 1
#elif __APPLE__
#define OS_DARWIN 1
#include "TargetConditionals.h"
#if TARGET_IPHONE_SIMULATOR
#define OS_POSIX 1
#elif TARGET_OS_IPHONE
#define OS_POSIX 1
#elif TARGET_OS_MAC
#define OS_POSIX 1
#else
#error "Unknown Apple platform"
#endif
#elif __linux__
#define OS_POSIX 1
#elif __unix__ // all unices not caught above
#define OS_POSIX 1
#elif defined(_POSIX_VERSION)
#define OS_POSIX 1
#else
#error "failed to detect target OS"
#endif

#if defined(ANDROID)
#define OS_ANDROID 1
#endif

#if defined(OS_WINDOWS)
#define NOMINMAX
#endif

#if defined(OS_WINDOWS)
#define strncasecmp _strnicmp
#define strcasecmp _stricmp
#endif


/*
we need to mark enum as unsigned if needs.
because processing enum in msvc is little different

ex) enum Type { A, B };
struct Foo { Type type: 1; };
Foo f; f.type = 1;
if (f.type == Type::B) { puts("failed in msvc."); }
*/
#if defined(OS_WINDOWS)
#define ENSURE_ENUM_UNSIGNED : unsigned int
#else
#define ENSURE_ENUM_UNSIGNED
#endif

#if defined(__amd64__) || defined(__amd64) || defined(__x86_64__) || defined(__x86_64) || defined(_M_X64) || defined(_M_AMD64)
#define CPU_X86_64

#elif defined(i386) || defined(__i386) || defined(__i386__) || defined(__IA32__) || defined(_M_IX86) || defined(__X86__) || defined(_X86_) || defined(__THW_INTEL__) || defined(__I86__) || defined(__INTEL__) || defined(__386)
#define CPU_X86

#elif defined(__arm__) || defined(__thumb__) || defined(_ARM) || defined(_M_ARM) || defined(_M_ARMT) || defined(__arm) || defined(__arm)
#define CPU_ARM32

#elif defined(__aarch64__)
#define CPU_ARM64

#elif defined(__riscv) && defined(__riscv_xlen) && __riscv_xlen == 32
#define CPU_RISCV32

#elif defined(__riscv) && defined(__riscv_xlen) && __riscv_xlen == 64
#define CPU_RISCV64

#else
#error "Could't find cpu arch."
#endif

// FIXME arm devices raise SIGBUS when using unaligned address to __atomic_* functions
#if defined(COMPILER_GCC) && !defined(CPU_ARM32) && !defined(CPU_ARM64)
#define HAVE_BUILTIN_ATOMIC_FUNCTIONS
#endif

#if defined(COMPILER_MSVC)
#include <stddef.h>
#endif

#if defined(ENABLE_THREADING)
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#endif

extern "C" {
#include <libbf.h>
}

#ifndef ESCARGOT_BUILD_DATE
#define ESCARGOT_BUILD_DATE __DATE__
#endif

#ifdef ENABLE_ICU
#if defined(ENABLE_RUNTIME_ICU_BINDER)
#include "RuntimeICUBinder.h"
#include "ICUPolyfill.h"
#else

#if defined(OS_WINDOWS)
#include <icu.h>
#else
#include <unicode/locid.h>
#include <unicode/uchar.h>
#include <unicode/ustring.h>
#include <unicode/datefmt.h>
#include <unicode/vtzone.h>
#include <unicode/unorm.h> // for normalize builtin function of String
#include <unicode/ucol.h> // for Intl
#include <unicode/urename.h> // for Intl
#include <unicode/unumsys.h> // for Intl
#include <unicode/udat.h> // for Intl
#include <unicode/udatpg.h> // for Intl
#include <unicode/unum.h> // for Intl
#include <unicode/upluralrules.h> // for Intl
#include <unicode/unumberformatter.h> // for Intl
#include <unicode/ureldatefmt.h> // for Intl
#include <unicode/uformattedvalue.h> // for Intl
#include <unicode/ucurr.h> // for Intl
#include <unicode/uloc.h> // for Intl
#include <unicode/uldnames.h> // for Intl
#include <unicode/ulistformatter.h> // for Intl

// FIXME replace these vzone decl into include
// I declare vzone api because there is no header file in include folder
extern "C" {
// VZone c api
struct VZone;
VZone* vzone_openID(const UChar* ID, int32_t idLength);
void vzone_close(VZone* zone);
UBool vzone_getTZURL(VZone* zone, UChar*& url, int32_t& urlLength);
void vzone_getOffset3(VZone* zone, UDate date, UBool local, int32_t& rawOffset,
                      int32_t& dstOffset, UErrorCode& ec);
int32_t vzone_getRawOffset(VZone* zone);
}
#endif // !defined(OS_WINDOWS_UWP)

#endif

#else

typedef char16_t UChar;
typedef unsigned char LChar;
typedef int32_t UChar32;

// macros from icu
#define U16_IS_LEAD(c) (((c)&0xfffffc00) == 0xd800)
#define U16_IS_TRAIL(c) (((c)&0xfffffc00) == 0xdc00)
#define U16_SURROGATE_OFFSET ((0xd800 << 10UL) + 0xdc00 - 0x10000)
#define U16_GET_SUPPLEMENTARY(lead, trail) \
    (((UChar32)(lead) << 10UL) + (UChar32)(trail)-U16_SURROGATE_OFFSET)

#define U16_NEXT(s, i, length, c)                                   \
    {                                                               \
        (c) = (s)[(i)++];                                           \
        if (U16_IS_LEAD(c)) {                                       \
            uint16_t __c2;                                          \
            if ((i) != (length) && U16_IS_TRAIL(__c2 = (s)[(i)])) { \
                ++(i);                                              \
                (c) = U16_GET_SUPPLEMENTARY((c), __c2);             \
            }                                                       \
        }                                                           \
    }
#define U16_LENGTH(c) ((uint32_t)(c) <= 0xffff ? 1 : 2)
#define U_IS_BMP(c) ((uint32_t)(c) <= 0xffff)
#define UCHAR_MAX_VALUE 0x10ffff
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifdef ENABLE_CUSTOM_LOGGING
// use customized logging
#include <stdarg.h>
namespace Escargot {
void customEscargotInfoLogger(const char* format, ...);
void customEscargotErrorLogger(const char* format, ...);
} // namespace Escargot
#define ESCARGOT_LOG_INFO(...) ::Escargot::customEscargotInfoLogger(__VA_ARGS__);
#define ESCARGOT_LOG_ERROR(...) ::Escargot::customEscargotErrorLogger(__VA_ARGS__);
#else
// use default logging
#define ESCARGOT_LOG_INFO(...) fprintf(stdout, __VA_ARGS__);
#define ESCARGOT_LOG_ERROR(...) fprintf(stderr, __VA_ARGS__);

#if defined(ESCARGOT_ANDROID)
#include <android/log.h>
#undef ESCARGOT_LOG_INFO
#undef ESCARGOT_LOG_ERROR
#define ESCARGOT_LOG_INFO(...) __android_log_print(ANDROID_LOG_INFO, "Escargot", __VA_ARGS__);
#define ESCARGOT_LOG_ERROR(...) __android_log_print(ANDROID_LOG_ERROR, "Escargot", __VA_ARGS__);
#endif

#if defined(ESCARGOT_TIZEN)
#include <dlog/dlog.h>
#undef ESCARGOT_LOG_INFO
#undef ESCARGOT_LOG_ERROR
#define ESCARGOT_LOG_INFO(...) dlog_print(DLOG_INFO, "Escargot", __VA_ARGS__);
#define ESCARGOT_LOG_ERROR(...) dlog_print(DLOG_ERROR, "Escargot", __VA_ARGS__);
#endif
#endif

#ifndef CRASH
#define CRASH ASSERT_NOT_REACHED
#endif

#if defined(NDEBUG)
#define ASSERT(assertion) ((void)0)
#define ASSERT_NOT_REACHED() ((void)0)
#define ASSERT_STATIC(assertion, reason)
#else
#define ASSERT(assertion) assert(assertion);
#define ASSERT_NOT_REACHED() \
    do {                     \
        assert(false);       \
    } while (0)
#define ASSERT_STATIC(assertion, reason) static_assert(assertion, reason)
#endif

/* COMPILE_ASSERT */
#ifndef COMPILE_ASSERT
#define COMPILE_ASSERT(exp, name) static_assert((exp), #name)
#endif

#define RELEASE_ASSERT(assertion)                                                  \
    do {                                                                           \
        if (!(assertion)) {                                                        \
            ESCARGOT_LOG_ERROR("RELEASE_ASSERT at %s (%d)\n", __FILE__, __LINE__); \
            abort();                                                               \
        }                                                                          \
    } while (0);
#define RELEASE_ASSERT_NOT_REACHED()                                                       \
    do {                                                                                   \
        ESCARGOT_LOG_ERROR("RELEASE_ASSERT_NOT_REACHED at %s (%d)\n", __FILE__, __LINE__); \
        abort();                                                                           \
    } while (0)

#if !defined(WARN_UNUSED_RETURN) && (defined(COMPILER_GCC) || defined(COMPILER_CLANG))
#define WARN_UNUSED_RETURN __attribute__((__warn_unused_result__))
#endif

#if !defined(WARN_UNUSED_RETURN)
#define WARN_UNUSED_RETURN
#endif

/* UNUSED_PARAMETER */

#if !defined(UNUSED_PARAMETER) && defined(COMPILER_MSVC)
#define UNUSED_PARAMETER(variable) (void)&variable
#endif

#if !defined(UNUSED_PARAMETER)
#define UNUSED_PARAMETER(variable) (void)variable
#endif

/* UNUSED_VARIABLE */

#if !defined(UNUSED_VARIABLE)
#define UNUSED_VARIABLE(variable) UNUSED_PARAMETER(variable)
#endif

#if !defined(FALLTHROUGH) && defined(COMPILER_GCC)
#if __GNUC__ >= 7
#define FALLTHROUGH __attribute__((fallthrough))
#else
#define FALLTHROUGH /* fall through */
#endif
#elif !defined(FALLTHROUGH) && defined(COMPILER_CLANG)
#define FALLTHROUGH /* fall through */
#else
#define FALLTHROUGH
#endif

#if (defined(__BYTE_ORDER__) && __BYTE_ORDER == __LITTLE_ENDIAN) || defined(__LITTLE_ENDIAN__) || defined(__i386) || defined(_M_IX86) || defined(__ia64) || defined(__ia64__) || defined(_M_IA64) || defined(_M_IX86) || defined(_M_X64) || defined(__ARMEL__) || defined(__THUMBEL__) || defined(__AARCH64EL__) || defined(_MIPSEL) || defined(__MIPSEL) || defined(__MIPSEL__) || defined(ANDROID)
#define ESCARGOT_LITTLE_ENDIAN
// #pragma message "little endian"
#elif (defined(__BYTE_ORDER__) && __BYTE_ORDER == __BIG_ENDIAN) || defined(__BIG_ENDIAN__) || defined(__ARMEB__) || defined(__THUMBEB__) || defined(__AARCH64EB__) || defined(_MIBSEB) || defined(__MIBSEB) || defined(__MIBSEB__)
#define ESCARGOT_BIG_ENDIAN
// #pragma message "big endian"
#else
#error "I don't know what architecture this is!"
#endif

#if defined(ENABLE_THREADING)
#if defined(COMPILER_MSVC)
#define MAY_THREAD_LOCAL __declspec(thread)
#else
#define MAY_THREAD_LOCAL __thread
#endif
#else
#define MAY_THREAD_LOCAL
#endif

#if defined(COMPILER_GCC) || defined(COMPILER_CLANG)
#define ESCARGOT_COMPUTED_GOTO_INTERPRETER
// some devices cannot support getting label address from outside well
#if (defined(CPU_ARM64) || (defined(CPU_ARM32) && defined(COMPILER_CLANG))) || defined(OS_DARWIN) || defined(OS_ANDROID) || defined(OS_WINDOWS)
#define ESCARGOT_COMPUTED_GOTO_INTERPRETER_INIT_WITH_NULL
#endif
#endif

#define MAKE_STACK_ALLOCATED()                    \
    static void* operator new(size_t) = delete;   \
    static void* operator new[](size_t) = delete; \
    static void operator delete(void*) = delete;  \
    static void operator delete[](void*) = delete;

#define ALLOCA(bytes, typenameWithoutPointer) (typenameWithoutPointer*)(LIKELY(bytes < 512) ? alloca(bytes) : GC_MALLOC(bytes))

typedef uint16_t ByteCodeRegisterIndex;
#define REGISTER_INDEX_IN_BIT 16
#define REGISTER_LIMIT (std::numeric_limits<ByteCodeRegisterIndex>::max())
#define REGULAR_REGISTER_LIMIT (REGISTER_LIMIT / 2)
#define VARIABLE_LIMIT (REGISTER_LIMIT / 4)
#define BINDED_NUMERAL_VARIABLE_LIMIT (REGISTER_LIMIT / 4)

#ifndef KEEP_NUMERAL_LITERDATA_IN_REGISTERFILE_LIMIT
#define KEEP_NUMERAL_LITERDATA_IN_REGISTERFILE_LIMIT 16
#endif

typedef uint16_t LexicalBlockIndex;
#define LEXICAL_BLOCK_INDEX_MAX (std::numeric_limits<LexicalBlockIndex>::max())

#if !defined(STACK_GROWS_DOWN) && !defined(STACK_GROWS_UP)
#define STACK_GROWS_DOWN
#endif

#ifndef STACK_FREESPACE_FROM_LIMIT
#define STACK_FREESPACE_FROM_LIMIT (1024 * 256) // 256KB
#endif

#ifndef STACK_USAGE_LIMIT
#ifdef ESCARGOT_ENABLE_TEST
#define STACK_USAGE_LIMIT (1024 * 1024 * 2) // 2MB
#else
#define STACK_USAGE_LIMIT (1024 * 1024 * 4) // 4MB
#endif
#endif

#ifdef STACK_GROWS_DOWN
#define CHECK_STACK_OVERFLOW(state)                                                                       \
    if (UNLIKELY(ThreadLocal::stackLimit() > (size_t)currentStackPointer())) {                            \
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Maximum call stack size exceeded"); \
    }
#else
#define CHECK_STACK_OVERFLOW(state)                                                                       \
    if (UNLIKELY(ThreadLocal::stackLimit() < (size_t)currentStackPointer())) {                            \
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, "Maximum call stack size exceeded"); \
    }
#endif


#ifndef STRING_MAXIMUM_LENGTH
#define STRING_MAXIMUM_LENGTH 1024 * 1024 * 512 // 512MB
#endif

#ifndef STRING_SUB_STRING_MIN_VIEW_LENGTH
#define STRING_SUB_STRING_MIN_VIEW_LENGTH 32
#endif

#ifndef STRING_BUILDER_INLINE_STORAGE_DEFAULT
#define STRING_BUILDER_INLINE_STORAGE_DEFAULT 24
#endif

#ifndef SCRIPT_FUNCTION_OBJECT_BYTECODE_SIZE_MAX
#define SCRIPT_FUNCTION_OBJECT_BYTECODE_SIZE_MAX 1024 * 256
#endif

#ifndef REGEXP_CACHE_SIZE_MAX
#define REGEXP_CACHE_SIZE_MAX 64
#endif

// maximum number of tail call arguments allowed
#ifndef TCO_ARGUMENT_COUNT_LIMIT
#define TCO_ARGUMENT_COUNT_LIMIT 8
#endif

#include <tsl/robin_set.h>
#include <tsl/robin_map.h>

namespace Escargot {

template <class Key, class Hash = std::hash<Key>,
          class KeyEqual = std::equal_to<Key>,
          class Allocator = std::allocator<Key>,
          bool StoreHash = false,
          class GrowthPolicy = tsl::rh::power_of_two_growth_policy<2>>
using HashSet = tsl::robin_set<Key, Hash, KeyEqual, Allocator, StoreHash, GrowthPolicy>;

template <class Key, class T, class Hash = std::hash<Key>,
          class KeyEqual = std::equal_to<Key>,
          class Allocator = std::allocator<std::pair<Key, T>>,
          bool StoreHash = false,
          class GrowthPolicy = tsl::rh::power_of_two_growth_policy<2>>
using HashMap = tsl::robin_map<Key, T, Hash, KeyEqual, Allocator, StoreHash, GrowthPolicy>;

} // namespace Escargot


#include "EscargotInfo.h"
#include "heap/Heap.h"
#include "util/Util.h"
#include "util/Optional.h"
#include "runtime/ThreadLocal.h"

#endif
