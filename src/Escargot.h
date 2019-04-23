/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
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

/* COMPILER() - the compiler being used to build the project */
// Outdated syntax: use defined()
// #define COMPILER(FEATURE) (defined COMPILER_##FEATURE && COMPILER_##FEATURE)

#if defined(__clang__)
#define COMPILER_CLANG 1
#elif defined(_MSC_VER)
#define COMPILER_MSVC 1
#elif (__GNUC__)
#define COMPILER_GCC 1
#else
#error "Compiler dectection failed"
#endif

#if defined(COMPILER_CLANG)
/* Keep strong enums turned off when building with clang-cl: We cannot yet build all of Blink without fallback to cl.exe, and strong enums are exposed at ABI boundaries. */
#undef COMPILER_SUPPORTS_CXX_STRONG_ENUMS
#else
#define COMPILER_SUPPORTS_CXX_OVERRIDE_CONTROL 1
#define COMPILER_QUIRK_FINAL_IS_CALLED_SEALED 1
#endif

/* ALWAYS_INLINE */
#ifndef ALWAYS_INLINE
#if (defined(COMPILER_GCC) || defined(COMPILER_CLANG)) && defined(NDEBUG) && !defined(COMPILER_MINGW)
#define ALWAYS_INLINE inline __attribute__((__always_inline__))
#elif defined(COMPILER_MSVC) && defined(NDEBUG)
#define ALWAYS_INLINE __forceinline
#else
#define ALWAYS_INLINE inline
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

#if defined(COMPILER_MSVC)
#define strncasecmp _strnicmp
#define strcasecmp _stricmp
#ifndef NDEBUG
#define _ITERATOR_DEBUG_LEVEL 0
#endif
#endif

// #define OS(NAME) (defined OS_##NAME && OS_##NAME)

#ifdef _WIN32
#define OS_WINDOWS 1
#elif _WIN64
#define OS_WINODWS 1
#elif __APPLE__
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

#if defined(OS_WINDOWS)
#define NOMINMAX
#endif

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

#ifdef ENABLE_ICU
#include <unicode/locid.h>
#include <unicode/uchar.h>
#include <unicode/datefmt.h>
#include <unicode/unorm.h> // for normalize builtin function of String
#include <unicode/ucol.h> // for Intl
#include <unicode/urename.h> // for Intl
#include <unicode/unumsys.h> // for Intl
#include <unicode/udat.h> // for Intl
#include <unicode/udatpg.h> // for Intl
#include <unicode/unum.h> // for Intl
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
#define u_tolower tolower
#define u_toupper toupper
#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#endif

#define ESCARGOT_LOG_INFO(...) fprintf(stdout, __VA_ARGS__);
#define ESCARGOT_LOG_ERROR(...) fprintf(stderr, __VA_ARGS__);

#if defined(ESCARGOT_ANDROID)
#include <android/log.h>
#undef ESCARGOT_LOG_ERROR
#define ESCARGOT_LOG_ERROR(...) __android_log_print(ANDROID_LOG_ERROR, "Escargot", __VA_ARGS__);
#endif

#if defined(ESCARGOT_TIZEN) && !defined(ESCARGOT_STANDALONE)
#include <dlog/dlog.h>
#undef ESCARGOT_LOG_INFO
#undef ESCARGOT_LOG_ERROR
#define ESCARGOT_LOG_INFO(...) dlog_print(DLOG_INFO, "Escargot", __VA_ARGS__);
#define ESCARGOT_LOG_ERROR(...) dlog_print(DLOG_ERROR, "Escargot", __VA_ARGS__);
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

#if defined(__BYTE_ORDER__) && __BYTE_ORDER == __LITTLE_ENDIAN || defined(__LITTLE_ENDIAN__) || defined(__i386) || defined(_M_IX86) || defined(__ia64) || defined(__ia64__) || defined(_M_IA64) || defined(__ARMEL__) || defined(__THUMBEL__) || defined(__AARCH64EL__) || defined(_MIPSEL) || defined(__MIPSEL) || defined(__MIPSEL__) || defined(ANDROID)
#define ESCARGOT_LITTLE_ENDIAN
// #pragma message "little endian"
#elif defined(__BYTE_ORDER__) && __BYTE_ORDER == __BIG_ENDIAN || defined(__BIG_ENDIAN__) || defined(__ARMEB__) || defined(__THUMBEB__) || defined(__AARCH64EB__) || defined(_MIBSEB) || defined(__MIBSEB) || defined(__MIBSEB__)
#define ESCARGOT_BIG_ENDIAN
// #pragma message "big endian"
#else
#error "I don't know what architecture this is!"
#endif

#define MAKE_STACK_ALLOCATED()                    \
    static void* operator new(size_t) = delete;   \
    static void* operator new[](size_t) = delete; \
    static void operator delete(void*) = delete;  \
    static void operator delete[](void*) = delete;

#define ALLOCA(bytes, typenameWithoutPointer, ec) (typenameWithoutPointer*)(LIKELY(bytes < 512) ? alloca(bytes) : GC_MALLOC(bytes))

typedef uint16_t ByteCodeRegisterIndex;
#define REGISTER_INDEX_IN_BIT 16
#define REGULAR_REGISTER_LIMIT (std::numeric_limits<ByteCodeRegisterIndex>::max() / 2)
#define VARIABLE_LIMIT (std::numeric_limits<ByteCodeRegisterIndex>::max() / 4)
#define BINDED_NUMERAL_VARIABLE_LIMIT (std::numeric_limits<ByteCodeRegisterIndex>::max() / 4)

#ifndef KEEP_NUMERAL_LITERDATA_IN_REGISTERFILE_LIMIT
#define KEEP_NUMERAL_LITERDATA_IN_REGISTERFILE_LIMIT 16
#endif

#if !defined(STACK_GROWS_DOWN) && !defined(STACK_GROWS_UP)
#define STACK_GROWS_DOWN
#endif

#ifndef STACK_LIMIT_FROM_BASE
#define STACK_LIMIT_FROM_BASE (1024 * 1024 * 3) // 3MB
#endif

#ifndef STRING_MAXIMUM_LENGTH
#define STRING_MAXIMUM_LENGTH 1024 * 1024 * 512 // 512MB
#endif

#ifndef STRING_SUB_STRING_MIN_VIEW_LENGTH
#define STRING_SUB_STRING_MIN_VIEW_LENGTH 32
#endif

#ifndef STRING_BUILDER_INLINE_STORAGE_MAX
#define STRING_BUILDER_INLINE_STORAGE_MAX 24
#endif

#ifndef FUNCTION_OBJECT_BYTECODE_SIZE_MAX
#define FUNCTION_OBJECT_BYTECODE_SIZE_MAX 1024 * 1024 * 2
#endif


#ifndef ROPE_STRING_MIN_LENGTH
#define ROPE_STRING_MIN_LENGTH 24
#endif

#include "heap/Heap.h"
#include "CheckedArithmetic.h"
#include "runtime/String.h"

#include "wtfbridge.h"
using namespace JSC::Yarr;

#endif
