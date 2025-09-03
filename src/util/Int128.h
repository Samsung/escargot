/*
 * Copyright (c) 2025-present Samsung Electronics Co., Ltd
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
#ifndef __EscargotInt128__
#define __EscargotInt128__

#define INT128_NO_EXPORT
#define INT128_SPECIALIZATION

#if !defined(__BYTE_ORDER__)

#if defined(ESCARGOT_LITTLE_ENDIAN)
#define __ORDER_LITTLE_ENDIAN__ 1
#define __BYTE_ORDER__ __ORDER_LITTLE_ENDIAN__
#else
#define __ORDER_BIG_ENDIAN__ 1
#define __BYTE_ORDER__ __ORDER_BIG_ENDIAN__
#endif

#endif

// from https://gist.github.com/pps83/3210a2f980fd02bb2ba2e5a1fc4a2ef0
#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
#include <limits.h>

__forceinline int __builtin_clz(unsigned x)
{
#if defined(_M_ARM) || defined(_M_ARM64) || defined(_M_HYBRID_X86_ARM64) || defined(_M_ARM64EC)
    return (int)_CountLeadingZeros(x);
#elif defined(__AVX2__) || defined(__LZCNT__)
    return (int)_lzcnt_u32(x);
#else
    unsigned long r;
    _BitScanReverse(&r, x);
    return (int)(r ^ 31);
#endif
}

__forceinline int __builtin_clzll(unsigned long long x)
{
#if defined(_M_ARM) || defined(_M_ARM64) || defined(_M_HYBRID_X86_ARM64) || defined(_M_ARM64EC)
    return (int)_CountLeadingZeros64(x);
#elif defined(_WIN64)
#if defined(__AVX2__) || defined(__LZCNT__)
    return (int)_lzcnt_u64(x);
#else
    unsigned long r;
    _BitScanReverse64(&r, x);
    return (int)(r ^ 63);
#endif
#else
    int l = __builtin_clz((unsigned)x) + 32;
    int h = __builtin_clz((unsigned)(x >> 32));
    return !!((unsigned)(x >> 32)) ? h : l;
#endif
}

static __forceinline int __builtin_clzl(unsigned long x)
{
    return sizeof(x) == 8 ? __builtin_clzll(x) : __builtin_clz((unsigned)x);
}
#endif

#include "int128/int128.h"
#undef INT128_SPECIALIZATION
#define INT128_NO_EXPORT

namespace Escargot {

using Int128 = large_int::int128_t;

} // namespace Escargot

namespace std {

inline string to_string(const Escargot::Int128& val)
{
    std::stringstream ss;
    ss << val;
    return ss.str();
}

inline Escargot::Int128 abs(const Escargot::Int128& value)
{
    if (value < 0) {
        return -value;
    }
    return value;
}

} // namespace std

#endif
