/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99 ft=cpp:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla SpiderMonkey JavaScript 1.9 code, released
 * June 12, 2009.
 *
 * The Initial Developer of the Original Code is
 *   the Mozilla Corporation.
 *
 * Contributor(s):
 *   David Mandelin <dmandelin@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef yarr_wtfbridge_h
#define yarr_wtfbridge_h

#include "Escargot.h"

#include <stdio.h>
#include <stdarg.h>
#include <type_traits>

#define JS_EXPORT_PRIVATE
#define WTF_EXPORT_PRIVATE
#define WTF_MAKE_FAST_ALLOCATED
#define NO_RETURN_DUE_TO_ASSERT
#define ASSERT_WITH_SECURITY_IMPLICATION ASSERT
#define UNUSED_PARAM(e)
#define ASSERT_UNUSED(variable, assertion) ASSERT(assertion)

#define PLATFORM(WTF_FEATURE) (defined WTF_PLATFORM_##WTF_FEATURE && WTF_PLATFORM_##WTF_FEATURE)
#define CPU(WTF_FEATURE) (defined WTF_CPU_##WTF_FEATURE && WTF_CPU_##WTF_FEATURE)
#define HAVE(WTF_FEATURE) (defined HAVE_##WTF_FEATURE && HAVE_##WTF_FEATURE)
#define OS(NAME) (defined OS_##NAME && OS_##NAME)
#define USE(WTF_FEATURE) (defined WTF_USE_##WTF_FEATURE && WTF_USE_##WTF_FEATURE)
#define ENABLE(WTF_FEATURE) (defined ENABLE_##WTF_FEATURE && ENABLE_##WTF_FEATURE)

#if ESCARGOT_64
#define WTF_CPU_X86_64 1
#endif

#if defined(OS_WINDOWS)
#define WTF_OS_WINDOWS 1
#else
#define WTF_OS_LINUX 1
#define WTF_OS_UNIX 1
#define HAVE_ERRNO_H 1
#define HAVE_MMAP 1
#endif

#define WTFMove std::move

#if ESCARGOT_64
using CPURegister = int64_t;
using UCPURegister = uint64_t;
#else
using CPURegister = int32_t;
using UCPURegister = uint32_t;
#endif


#if (__cplusplus < 201402L)
namespace std {
// NOTE there is no make_unique in c++11
template <typename T, typename... Ts>
std::unique_ptr<T> make_unique(Ts&&... params)
{
    return std::unique_ptr<T>(new T(std::forward<Ts>(params)...));
}

// NOTE there is no conditional_t in c++11
template< bool B, class T, class F >
using conditional_t = typename conditional<B,T,F>::type;

// NOTE there is no make_unsigned_t in c++11
template< class T >
using make_unsigned_t = typename make_unsigned<T>::type;
}
#endif

template <typename T, typename... Ts>
std::unique_ptr<T> makeUnique(Ts&&... params)
{
    return std::unique_ptr<T>(new T(std::forward<Ts>(params)...));
}

template<typename T> constexpr bool hasOneBitSet(T value)
{
    return !((value - 1) & value) && value;
}

inline constexpr bool isPowerOfTwo(size_t size) { return !(size & (size - 1)); }

template<typename T, typename U>
ALWAYS_INLINE T roundUpToMultipleOfImpl(U divisor, T x)
{
    T remainderMask = static_cast<T>(divisor) - 1;
    return (x + remainderMask) & ~remainderMask;
}

// Efficient implementation that takes advantage of powers of two.
template<typename T, typename U>
inline constexpr T roundUpToMultipleOf(U divisor, T x)
{
    return roundUpToMultipleOfImpl<T, U>(divisor, x);
}

template<size_t divisor> constexpr size_t roundUpToMultipleOf(size_t x)
{
    static_assert(divisor && isPowerOfTwo(divisor), "");
    return roundUpToMultipleOfImpl(divisor, x);
}

template<size_t divisor, typename T> inline constexpr T* roundUpToMultipleOf(T* x)
{
    static_assert(sizeof(T*) == sizeof(size_t), "");
    return reinterpret_cast<T*>(roundUpToMultipleOf<divisor>(reinterpret_cast<size_t>(x)));
}

template<typename T>
bool findBitInWord(T word, size_t& startOrResultIndex, size_t endIndex, bool value)
{
    static_assert(std::is_unsigned<T>::value, "Type used in findBitInWord must be unsigned");

    constexpr size_t bitsInWord = sizeof(word) * 8;
    ASSERT_UNUSED(bitsInWord, startOrResultIndex <= bitsInWord && endIndex <= bitsInWord);

    size_t index = startOrResultIndex;
    word >>= index;

    while (index < endIndex) {
        if ((word & 1) == static_cast<T>(value)) {
            startOrResultIndex = index;
            return true;
        }
        index++;
        word >>= 1;
    }

    startOrResultIndex = endIndex;
    return false;
}

#include "runtime/String.h"
#include "CheckedArithmetic.h"

namespace WTF {

template<typename T>
using Optional = Escargot::Optional<T>;

typedef Checked<uint32_t, RecordOverflow> CheckedUint32;

constexpr size_t notFound = static_cast<size_t>(-1);

// Returns a count of the number of bits set in 'bits'.
inline size_t bitCount(unsigned bits)
{
    bits = bits - ((bits >> 1) & 0x55555555);
    bits = (bits & 0x33333333) + ((bits >> 2) & 0x33333333);
    return (((bits + (bits >> 4)) & 0xF0F0F0F) * 0x1010101) >> 24;
}

inline size_t bitCount(uint64_t bits)
{
    return bitCount(static_cast<unsigned>(bits)) + bitCount(static_cast<unsigned>(bits >> 32));
}

template <typename ToType, typename FromType>
inline ToType bitwise_cast(FromType from)
{
    ASSERT(sizeof(FromType) == sizeof(ToType));
    union {
        FromType from;
        ToType to;
    } u;
    u.from = from;
    return u.to;
}

} /* namespace WTF */

using WTF::CheckedUint32;

#include "ASCIICType.h"
#include "StringHasher.h"
#include "SuperFastHash.h"
#include "StringHasherInlines.h"
#include "WTFString.h"

#include "Vector.h"
#include "HashMap.h"
#include "HashSet.h"

#include "BitSet.h"
#include "BitVector.h"
#include "OptionSet.h"

#include "StackCheck.h"

namespace std {
template <>
struct hash<::WTF::String> {
    size_t operator()(::WTF::String const& x) const
    {
        return x.hash();
    }
};

template <>
struct equal_to<::WTF::String> {
    bool operator()(::WTF::String const& a, ::WTF::String const& b) const
    {
        return a.equals(b);
    }
};
}


using WTF::Optional;
using WTF::StringView;

#undef RELEASE_ASSERT
#define RELEASE_ASSERT ASSERT

typedef const char* ASCIILiteral;

#endif /* yarr_wtfbridge_h */
