/*
 * Copyright (C) 2021 Sony Interactive Entertainment Inc.
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "Escargot.h"
#include "Int128.h"

namespace Escargot {

CheckedInt128 checkedCastDoubleToInt128(double n)
{
    // Based on __fixdfti() and __fixunsdfti() from compiler_rt:
    // https://github.com/llvm/llvm-project/blob/f3671de5500ff1f8210419226a9603a7d83b1a31/compiler-rt/lib/builtins/fp_fixint_impl.inc
    // https://github.com/llvm/llvm-project/blob/f3671de5500ff1f8210419226a9603a7d83b1a31/compiler-rt/lib/builtins/fp_fixuint_impl.inc

    static constexpr int significandBits = std::numeric_limits<double>::digits - 1;
    static constexpr int exponentBits = std::numeric_limits<uint64_t>::digits - std::numeric_limits<double>::digits;
    static constexpr int exponentBias = std::numeric_limits<double>::max_exponent - 1;
    static constexpr uint64_t implicitBit = uint64_t{ 1 } << significandBits;
    static constexpr uint64_t significandMask = implicitBit - uint64_t{ 1 };
    static constexpr uint64_t signMask = uint64_t{ 1 } << (significandBits + exponentBits);
    static constexpr uint64_t absMask = signMask - uint64_t{ 1 };

    // Break n into sign, exponent, significand parts.
    const uint64_t bits = *reinterpret_cast<uint64_t*>(&n);
    const uint64_t nAbs = bits & absMask;
    const int sign = bits & signMask ? -1 : 1;
    const int exponent = (nAbs >> significandBits) - exponentBias;
    const uint64_t significand = (nAbs & significandMask) | implicitBit;

    // If exponent is negative, the result is zero.
    if (exponent < 0)
        return { 0 };

    // If the value is too large for the integer type, overflow.
    if (exponent >= 128)
        return { ResultOverflowed };

    // If 0 <= exponent < significandBits, right shift to get the result.
    // Otherwise, shift left.
    Int128 result{ significand };
    if (exponent < significandBits)
        result >>= significandBits - exponent;
    else
        result <<= exponent - significandBits;
    result *= sign;
    return { result };
}

} // namespace Escargot
