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

#include "Escargot.h"
#include "runtime/GlobalObject.h"
#include "runtime/Context.h"
#include "runtime/VMInstance.h"
#include "runtime/ThreadLocal.h"
#include "runtime/StringObject.h"
#include "runtime/NativeFunctionObject.h"
#include "runtime/IteratorObject.h"

#include "runtime/IEEE754.h"
#include <math.h>

#include "xsum.h"

namespace Escargot {

static Value builtinMathAbs(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    return Value(Value::DoubleToIntConvertibleTestNeeds, std::abs(argv[0].toNumber(state)));
}

static Value builtinMathMax(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    bool is_NaN = false;
    if (argc == 0) {
        return Value(Value::NegativeInfinityInit);
    }

    double maxValue = argv[0].toNumber(state);
    for (unsigned i = 1; i < argc; i++) {
        double value = argv[i].toNumber(state);
        if (std::isnan(value))
            is_NaN = true;
        if (value > maxValue || (!value && !maxValue && !std::signbit(value)))
            maxValue = value;
    }
    if (is_NaN) {
        return Value(Value::NanInit);
    }
    return Value(Value::DoubleToIntConvertibleTestNeeds, maxValue);
}

static Value builtinMathMin(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (argc == 0) {
        return Value(Value::PostiveInfinityInit);
    }

    bool hasNaN = false;
    double minValue = argv[0].toNumber(state);
    for (unsigned i = 1; i < argc; i++) {
        double value = argv[i].toNumber(state);
        if (std::isnan(value)) {
            hasNaN = true;
        }
        if (value < minValue || (!value && !minValue && std::signbit(value))) {
            minValue = value;
        }
    }
    if (hasNaN) {
        return Value(Value::NanInit);
    }
    return Value(Value::DoubleToIntConvertibleTestNeeds, minValue);
}

static Value builtinMathRound(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    double x = argv[0].toNumber(state);
    if (x == static_cast<int64_t>(x)) {
        return Value(Value::DoubleToIntConvertibleTestNeeds, x);
    }
    if (x == -0.5)
        return Value(UnconvertibleDoubleToInt32(-0.0));
    else if (x > -0.5)
        return Value(Value::DoubleToIntConvertibleTestNeeds, round(x));
    else
        return Value(Value::DoubleToIntConvertibleTestNeeds, floor(x + 0.5));
}

static Value builtinMathSin(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    Value x = argv[0];
    return Value(Value::DoubleToIntConvertibleTestNeeds, ieee754::sin(x.toNumber(state)));
}

static Value builtinMathSinh(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    double x = argv[0].toNumber(state);
    return Value(Value::DoubleToIntConvertibleTestNeeds, ieee754::sinh(x));
}

static Value builtinMathCos(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    Value x = argv[0];
    return Value(Value::DoubleToIntConvertibleTestNeeds, ieee754::cos(x.toNumber(state)));
}

static Value builtinMathCosh(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    double x = argv[0].toNumber(state);
    return Value(Value::DoubleToIntConvertibleTestNeeds, ieee754::cosh(x));
}

static Value builtinMathAcos(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    double x = argv[0].toNumber(state);
    return Value(Value::DoubleToIntConvertibleTestNeeds, ieee754::acos(x));
}

static Value builtinMathAcosh(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    double x = argv[0].toNumber(state);
    return Value(Value::DoubleToIntConvertibleTestNeeds, ieee754::acosh(x));
}

static Value builtinMathAsin(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    double x = argv[0].toNumber(state);
    return Value(Value::DoubleToIntConvertibleTestNeeds, ieee754::asin(x));
}

static Value builtinMathAsinh(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    double x = argv[0].toNumber(state);
    return Value(Value::DoubleToIntConvertibleTestNeeds, ieee754::asinh(x));
}

static Value builtinMathAtan(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    double x = argv[0].toNumber(state);
    return Value(Value::DoubleToIntConvertibleTestNeeds, ieee754::atan(x));
}

static Value builtinMathAtan2(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    double y = argv[0].toNumber(state);
    double x = argv[1].toNumber(state);
    return Value(Value::DoubleToIntConvertibleTestNeeds, ieee754::atan2(y, x));
}

static Value builtinMathAtanh(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    double x = argv[0].toNumber(state);
    return Value(Value::DoubleToIntConvertibleTestNeeds, ieee754::atanh(x));
}

static Value builtinMathTan(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    double x = argv[0].toNumber(state);
    return Value(Value::DoubleToIntConvertibleTestNeeds, ieee754::tan(x));
}

static Value builtinMathTanh(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    double x = argv[0].toNumber(state);
    return Value(Value::DoubleToIntConvertibleTestNeeds, ieee754::tanh(x));
}

static Value builtinMathTrunc(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    double x = argv[0].toNumber(state);
    return Value(Value::DoubleToIntConvertibleTestNeeds, trunc(x));
}

static Value builtinMathSign(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    double x = argv[0].toNumber(state);
    if (std::isnan(x))
        return Value(Value::NanInit);
    else if (x == 0.0) {
        if (std::signbit(x)) {
            return Value(Value::EncodeAsDouble, -0.0);
        } else {
            return Value(0);
        }
    } else if (std::signbit(x))
        return Value(-1);
    else
        return Value(1);
}

static Value builtinMathSqrt(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    Value x = argv[0];
    return Value(Value::DoubleToIntConvertibleTestNeeds, sqrt(x.toNumber(state)));
}

static Value builtinMathPow(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    double x = argv[0].toNumber(state);
    double y = argv[1].toNumber(state);
    if (UNLIKELY(std::isnan(y)))
        return Value(Value::NanInit);
    if (UNLIKELY(std::abs(x) == 1 && std::isinf(y)))
        return Value(Value::NanInit);

    int y_int = static_cast<int>(y);

    if (y == y_int) {
        unsigned n = (y < 0) ? -y : y;
        double m = x;
        double p = 1;
        while (true) {
            if ((n & 1) != 0)
                p *= m;
            n >>= 1;
            if (n == 0) {
                if (y < 0) {
                    // Unfortunately, we have to be careful when p has reached
                    // infinity in the computation, because sometimes the higher
                    // internal precision in the pow() implementation would have
                    // given us a finite p. This happens very rarely.

                    double result = 1.0 / p;
                    return (result == 0 && std::isinf(p)) ? Value(Value::DoubleToIntConvertibleTestNeeds, pow(x, static_cast<double>(y))) // Avoid pow(double, int).
                                                          : Value(Value::DoubleToIntConvertibleTestNeeds, result);
                }

                return Value(Value::DoubleToIntConvertibleTestNeeds, p);
            }
            m *= m;
        }
    }

    if (std::isinf(x)) {
        if (x > 0) {
            if (y > 0) {
                return Value(Value::PostiveInfinityInit);
            } else {
                return Value(0);
            }
        } else {
            if (y > 0) {
                if (y == y_int && y_int % 2) { // odd
                    return Value(Value::NegativeInfinityInit);
                } else {
                    return Value(Value::PostiveInfinityInit);
                }
            } else {
                if (y == y_int && y_int % 2) {
                    return Value(UnconvertibleDoubleToInt32(-0.0));
                } else {
                    return Value(0);
                }
            }
        }
    }
    // x == -0
    if (1 / x == -std::numeric_limits<double>::infinity()) {
        // y cannot be an odd integer because the case is filtered by "if (y_int == y)" above
        if (y > 0) {
            return Value(0);
        } else if (y < 0) {
            return Value(Value::PostiveInfinityInit);
        }
    }

    if (y == 0.5) {
        return Value(Value::DoubleToIntConvertibleTestNeeds, sqrt(x));
    } else if (y == -0.5) {
        return Value(Value::DoubleToIntConvertibleTestNeeds, 1.0 / sqrt(x));
    }

    return Value(Value::DoubleToIntConvertibleTestNeeds, pow(x, y));
}

static Value builtinMathCbrt(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    double x = argv[0].toNumber(state);
    return Value(Value::DoubleToIntConvertibleTestNeeds, ieee754::cbrt(x));
}

static Value builtinMathCeil(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    double x = argv[0].toNumber(state);

    // I add custom ceil implementation
    // because I found some problem from gcc implementation about negative zero
    return Value(Value::DoubleToIntConvertibleTestNeeds, ieee754::ceil(x));
}

static Value builtinMathClz32(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    uint32_t x = argv[0].toUint32(state);
    int clz32 = 0;
    for (int i = 31; i >= 0; i--) {
        if (!(x >> i))
            clz32++;
        else
            break;
    }
    return Value(clz32);
}

static Value builtinMathFloor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    double x = argv[0].toNumber(state);
    return Value(Value::DoubleToIntConvertibleTestNeeds, floor(x));
}

static Value builtinMathFround(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    double x = argv[0].toNumber(state);
    return Value(Value::DoubleToIntConvertibleTestNeeds, static_cast<double>(static_cast<float>(x)));
}

// This is extracted from Version 2.2.0 of the half library by Christian Rau.
// See https://sourceforge.net/projects/half/.
// The original copyright and MIT license are reproduced below:

// half - IEEE 754-based half-precision floating-point library.
//
// Copyright (c) 2012-2021 Christian Rau <rauy@users.sourceforge.net>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

/// Type traits for floating-point bits.
template <typename T>
struct bits {
    using type = unsigned char;
};
template <typename T>
struct bits<const T> : bits<T> {};
template <typename T>
struct bits<volatile T> : bits<T> {};
template <typename T>
struct bits<const volatile T> : bits<T> {};

/// Unsigned integer of (at least) 64 bits width.
template <>
struct bits<double> {
    using type = std::uint_least64_t;
};


/// Fastest unsigned integer of (at least) 32 bits width.
using uint32 = std::uint_fast32_t;

/// Half-precision overflow.
/// \param sign half-precision value with sign bit only
/// \return rounded overflowing half-precision value
constexpr unsigned int overflow(unsigned int sign = 0) { return sign | 0x7C00; }

/// Half-precision underflow.
/// \param sign half-precision value with sign bit only
/// \return rounded underflowing half-precision value
constexpr unsigned int underflow(unsigned int sign = 0) { return sign; }

/// Round half-precision number.
/// \param value finite half-precision number to round
/// \param g guard bit (most significant discarded bit)
/// \param s sticky bit (or of all but the most significant discarded bits)
/// \return rounded half-precision value
constexpr unsigned int rounded(unsigned int value, int g, int s)
{
    return value + (g & (s | value));
}

/// Convert IEEE double-precision to half-precision.
/// \param value double-precision value to convert
/// \return rounded half-precision value
inline unsigned int float2half_impl(double value)
{
    bits<double>::type dbits;
    std::memcpy(&dbits, &value, sizeof(double));
    uint32 hi = dbits >> 32, lo = dbits & 0xFFFFFFFF;
    unsigned int sign = (hi >> 16) & 0x8000;
    hi &= 0x7FFFFFFF;
    if (hi >= 0x7FF00000)
        return sign | 0x7C00 | ((dbits & 0xFFFFFFFFFFFFF) ? (0x200 | ((hi >> 10) & 0x3FF)) : 0);
    if (hi >= 0x40F00000)
        return overflow(sign);
    if (hi >= 0x3F100000)
        return rounded(sign | (((hi >> 20) - 1008) << 10) | ((hi >> 10) & 0x3FF),
                       (hi >> 9) & 1, ((hi & 0x1FF) | lo) != 0);
    if (hi >= 0x3E600000) {
        int i = 1018 - (hi >> 20);
        hi = (hi & 0xFFFFF) | 0x100000;
        return rounded(sign | (hi >> (i + 1)), (hi >> i) & 1,
                       ((hi & ((static_cast<uint32>(1) << i) - 1)) | lo) != 0);
    }
    if ((hi | lo) != 0)
        return underflow(sign);
    return sign;
}

/// Convert half-precision to IEEE double-precision.
/// \param value half-precision value to convert
/// \return double-precision value
inline double half2float_impl(unsigned int value)
{
    uint32 hi = static_cast<uint32>(value & 0x8000) << 16;
    unsigned int abs = value & 0x7FFF;
    if (abs) {
        hi |= 0x3F000000 << static_cast<unsigned>(abs >= 0x7C00);
        for (; abs < 0x400; abs <<= 1, hi -= 0x100000)
            ;
        hi += static_cast<uint32>(abs) << 10;
    }
    bits<double>::type dbits = static_cast<bits<double>::type>(hi) << 32;
    double out;
    std::memcpy(&out, &dbits, sizeof(double));
    return out;
}

// End of the half library extraction.

// \brief Conversion from float/double to half round number.
// \details For the specification, see https://tc39.es/proposal-float16array/#sec-function-properties-of-the-math-object.
// \author Anwar Fuadi
// \date 2025-present
static auto builtinMathF16round(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget) -> Value
{
    // 1. Let n be ? ToNumber(x).
    auto x = double{ argv[0].toNumber(state) };

    // 2. If n is NaN, return NaN.
    // 3. If n is one of +0𝔽, -0𝔽, +∞𝔽, or -∞𝔽, return n.
    // 4. Let n16 be the result of converting n to IEEE 754-2019 binary16 format using roundTiesToEven mode.
    auto f16 = float2half_impl(x);
    x = half2float_impl(f16);
    // 5. Let n64 be the result of converting n16 to IEEE 754-2019 binary64 format.
    // 6. Return the ECMAScript Number value corresponding to n64.
    return Value(Value::DoubleToIntConvertibleTestNeeds, x);
}

static Value builtinMathHypot(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    double maxValue = 0;
    bool has_nan = false;
    bool has_inf = false;
    for (unsigned i = 0; i < argc; i++) {
        double value = argv[i].toNumber(state);
        if (std::isinf(value)) {
            has_inf = true;
        } else if (std::isnan(value)) {
            has_nan = true;
        }
        double absValue = std::abs(value);
        maxValue = std::max(maxValue, absValue);
    }
    if (has_inf) {
        return Value(Value::PostiveInfinityInit);
    }

    if (has_nan) {
        return Value(Value::NanInit);
    }

    if (maxValue == 0) {
        return Value(0);
    }

    double sum = 0;
    double compensation = 0;
    for (unsigned i = 0; i < argc; i++) {
        double value = argv[i].toNumber(state);
        double scaledArgument = value / maxValue;
        double summand = scaledArgument * scaledArgument - compensation;
        double preliminary = sum + summand;
        compensation = (preliminary - sum) - summand;
        sum = preliminary;
    }
    return Value(Value::DoubleToIntConvertibleTestNeeds, std::sqrt(sum) * maxValue);
}

static Value builtinMathIMul(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    int32_t x = argv[0].toInt32(state);
    int32_t y = argv[1].toInt32(state);
    return Value(x * y);
}

static Value builtinMathLog(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    double x = argv[0].toNumber(state);
    return Value(Value::DoubleToIntConvertibleTestNeeds, ieee754::log(x));
}

static Value builtinMathLog1p(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    double x = argv[0].toNumber(state);
    return Value(Value::DoubleToIntConvertibleTestNeeds, ieee754::log1p(x));
}

static Value builtinMathLog10(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    double x = argv[0].toNumber(state);
    return Value(Value::DoubleToIntConvertibleTestNeeds, ieee754::log10(x));
}

static Value builtinMathLog2(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    double x = argv[0].toNumber(state);
    return Value(Value::DoubleToIntConvertibleTestNeeds, ieee754::log2(x));
}

static Value builtinMathRandom(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    std::uniform_real_distribution<double> distribution;
    return Value(Value::DoubleToIntConvertibleTestNeeds, distribution(ThreadLocal::randEngine()));
}

static Value builtinMathExp(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    double x = argv[0].toNumber(state);
    return Value(Value::DoubleToIntConvertibleTestNeeds, ieee754::exp(x));
}

static Value builtinMathExpm1(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    double x = argv[0].toNumber(state);
    return Value(Value::DoubleToIntConvertibleTestNeeds, ieee754::expm1(x));
}

enum class SumPreciseState {
    MinusZero,
    NotANumber,
    MinusInfinity,
    PlusInfinity,
    Finite
};

// https://tc39.es/proposal-math-sum/#sec-math.sumprecise
static Value builtinMathSumPrecise(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    const Value& items = argv[0];

    // 1. Perform ? RequireObjectCoercible(items).
    if (items.isUndefinedOrNull()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "sumPrecise called on undefined or null");
    }

    xsum_small_accumulator sacc;
    xsum_small_init(&sacc);

    // 2. Let iteratorRecord be ? GetIterator(items, sync).
    auto iteratorRecord = IteratorObject::getIterator(state, items, true);
    // 3. Let state be minus-zero.
    SumPreciseState sumPreciseState = SumPreciseState::MinusZero;
    // 4. Let sum be 0.
    // 5. Let count be 0.
    // 6. Let next be not-started.
    uint64_t count = 0;
    // 7. Repeat, while next is not done,
    while (true) {
        // a. Set next to ? IteratorStepValue(iteratorRecord).
        auto next = IteratorObject::iteratorStepValue(state, iteratorRecord);
        // b. If next is not done, then
        if (next) {
            // i. Set count to count + 1.
            count++;
            // ii. If count ≥ 2**53, then
            if (count >= (1LL << 53LL)) {
                // 1. Let error be ThrowCompletion(a newly created RangeError object).
                Value error = ErrorObject::createError(state, ErrorCode::TypeError, new ASCIIStringFromExternalMemory("Too many result on sumPrecise function"));
                // 2. Return ? IteratorClose(iteratorRecord, error).
                IteratorObject::iteratorClose(state, iteratorRecord, error, true);
                ASSERT_NOT_REACHED();
            }
            // iii. NOTE: The above case is not expected to be reached in practice and is included only so that implementations may rely on inputs being "reasonably sized" without violating this specification.
            // iv. If next is not a Number, then
            if (!next.value().isNumber()) {
                // 1. Let error be ThrowCompletion(a newly created TypeError object).
                Value error = ErrorObject::createError(state, ErrorCode::TypeError, new ASCIIStringFromExternalMemory("sumPrecise function needs number value"));
                // 2. Return ? IteratorClose(iteratorRecord, error).
                IteratorObject::iteratorClose(state, iteratorRecord, error, true);
                ASSERT_NOT_REACHED();
            }
            // v. Let n be next.
            double n = next.value().asNumber();
            // vi. If state is not not-a-number, then
            if (sumPreciseState != SumPreciseState::NotANumber) {
                // 1. If n is NaN, then
                if (std::isnan(n)) {
                    // a. Set state to not-a-number.
                    sumPreciseState = SumPreciseState::NotANumber;
                } else if (std::isinf(n) && std::signbit(n) == 0) {
                    // 2. Else if n is +∞𝔽, then
                    // a. If state is minus-infinity, set state to not-a-number.
                    if (sumPreciseState == SumPreciseState::MinusInfinity) {
                        sumPreciseState = SumPreciseState::NotANumber;
                    } else {
                        // b. Else, set state to plus-infinity.
                        sumPreciseState = SumPreciseState::PlusInfinity;
                    }
                } else if (std::isinf(n) && std::signbit(n) == 1) {
                    // 3. Else if n is -∞𝔽, then
                    // a. If state is plus-infinity, set state to not-a-number.
                    if (sumPreciseState == SumPreciseState::PlusInfinity) {
                        sumPreciseState = SumPreciseState::NotANumber;
                    } else {
                        // b. Else, set state to minus-infinity.
                        sumPreciseState = SumPreciseState::MinusInfinity;
                    }
                } else if (!(n == 0 && std::signbit(n)) && (sumPreciseState == SumPreciseState::MinusZero || sumPreciseState == SumPreciseState::Finite)) {
                    // 4. Else if n is not -0𝔽 and state is either minus-zero or finite, then
                    // a. Set state to finite.
                    sumPreciseState = SumPreciseState::Finite;
                    // b. Set sum to sum + ℝ(n).
                    xsum_small_add1(&sacc, n);
                }
            }
        } else {
            break;
        }
    }

    // 8. If state is not-a-number, return NaN.
    // 9. If state is plus-infinity, return +∞𝔽.
    // 10. If state is minus-infinity, return -∞𝔽.
    // 11. If state is minus-zero, return -0𝔽.
    // 12. Return 𝔽(sum).
    double result = 0;
    switch (sumPreciseState) {
    case SumPreciseState::NotANumber:
        result = std::numeric_limits<double>::quiet_NaN();
        break;
    case SumPreciseState::PlusInfinity:
        result = std::numeric_limits<double>::infinity();
        break;
    case SumPreciseState::MinusInfinity:
        result = -std::numeric_limits<double>::infinity();
        break;
    case SumPreciseState::MinusZero:
        result = -0.0;
        break;
    case SumPreciseState::Finite:
        result = xsum_small_round(&sacc);
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    return Value(Value::DoubleToIntConvertibleTestNeeds, result);
}

void GlobalObject::initializeMath(ExecutionState& state)
{
    ObjectPropertyNativeGetterSetterData* nativeData = new ObjectPropertyNativeGetterSetterData(true, false, true, [](ExecutionState& state, Object* self, const Value& receiver, const EncodedValue& privateDataFromObjectPrivateArea) -> Value {
                                                                                                    ASSERT(self->isGlobalObject());
                                                                                                    return self->asGlobalObject()->math(); }, nullptr);

    defineNativeDataAccessorProperty(state, ObjectPropertyName(state.context()->staticStrings().Math), nativeData, Value(Value::EmptyValue));
}

void GlobalObject::installMath(ExecutionState& state)
{
    m_math = new Object(state);
    m_math->setGlobalIntrinsicObject(state);

    m_math->directDefineOwnProperty(state, ObjectPropertyName(state, Value(state.context()->vmInstance()->globalSymbols().toStringTag)),
                                    ObjectPropertyDescriptor(Value(state.context()->staticStrings().Math.string()), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));

    // initialize math object: $20.2.1.6 Math.PI
    const StaticStrings* strings = &state.context()->staticStrings();
    m_math->directDefineOwnProperty(state, strings->PI, ObjectPropertyDescriptor(Value(UnconvertibleDoubleToInt32(3.1415926535897932)), ObjectPropertyDescriptor::ValuePresent));
    m_math->directDefineOwnProperty(state, strings->E, ObjectPropertyDescriptor(Value(UnconvertibleDoubleToInt32(2.718281828459045)), ObjectPropertyDescriptor::ValuePresent));
    // http://www.ecma-international.org/ecma-262/5.1/#sec-15.8.1.3
    m_math->directDefineOwnProperty(state, strings->LN2, ObjectPropertyDescriptor(Value(UnconvertibleDoubleToInt32(0.6931471805599453)), ObjectPropertyDescriptor::ValuePresent));
    // http://www.ecma-international.org/ecma-262/5.1/#sec-15.8.1.2
    m_math->directDefineOwnProperty(state, strings->LN10, ObjectPropertyDescriptor(Value(UnconvertibleDoubleToInt32(2.302585092994046)), ObjectPropertyDescriptor::ValuePresent));
    // http://www.ecma-international.org/ecma-262/5.1/#sec-15.8.1.4
    m_math->directDefineOwnProperty(state, strings->LOG2E, ObjectPropertyDescriptor(Value(UnconvertibleDoubleToInt32(1.4426950408889634)), ObjectPropertyDescriptor::ValuePresent));
    // http://www.ecma-international.org/ecma-262/5.1/#sec-15.8.1.5
    m_math->directDefineOwnProperty(state, strings->LOG10E, ObjectPropertyDescriptor(Value(UnconvertibleDoubleToInt32(0.4342944819032518)), ObjectPropertyDescriptor::ValuePresent));
    // http://www.ecma-international.org/ecma-262/5.1/#sec-15.8.1.7
    m_math->directDefineOwnProperty(state, strings->SQRT1_2, ObjectPropertyDescriptor(Value(UnconvertibleDoubleToInt32(0.7071067811865476)), ObjectPropertyDescriptor::ValuePresent));
    // http://www.ecma-international.org/ecma-262/5.1/#sec-15.8.1.8
    m_math->directDefineOwnProperty(state, strings->SQRT2, ObjectPropertyDescriptor(Value(UnconvertibleDoubleToInt32(1.4142135623730951)), ObjectPropertyDescriptor::ValuePresent));

    // initialize math object: $20.2.2.1 Math.abs()
    m_math->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().abs),
                                    ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().abs, builtinMathAbs, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // initialize math object: $20.2.2.2 Math.acos()
    m_math->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().acos),
                                    ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().acos, builtinMathAcos, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // initialize math object: $20.2.2.3 Math.acosh()
    m_math->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().acosh),
                                    ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().acosh, builtinMathAcosh, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // initialize math object: $20.2.2.4 Math.asin()
    m_math->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().asin),
                                    ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().asin, builtinMathAsin, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // initialize math object: $20.2.2.5 Math.asinh()
    m_math->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().asinh),
                                    ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().asinh, builtinMathAsinh, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // initialize math object: $20.2.2.6 Math.atan()
    m_math->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().atan),
                                    ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().atan, builtinMathAtan, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // initialize math object: $20.2.2.7 Math.atanh()
    m_math->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().atanh),
                                    ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().atanh, builtinMathAtanh, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // initialize math object: $20.2.2.8 Math.atan2()
    m_math->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().atan2),
                                    ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().atan2, builtinMathAtan2, 2, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // initialize math object: $20.2.2.9 Math.cbrt()
    m_math->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().cbrt),
                                    ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().cbrt, builtinMathCbrt, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // initialize math object: $20.2.2.10 Math.ceil()
    m_math->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().ceil),
                                    ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().ceil, builtinMathCeil, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // initialize math object: $20.2.2.11 Math.clz32()
    m_math->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().clz32),
                                    ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().clz32, builtinMathClz32, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // initialize math object: $20.2.2.12 Math.cos()
    m_math->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().cos),
                                    ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().cos, builtinMathCos, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // initialize math object: $20.2.2.13 Math.cosh()
    m_math->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().cosh),
                                    ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().cosh, builtinMathCosh, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // initialize math object: $20.2.2.14 Math.exp()
    m_math->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().exp),
                                    ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().exp, builtinMathExp, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // initialize math object: $20.2.2.15 Math.expm1()
    m_math->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().expm1),
                                    ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().expm1, builtinMathExpm1, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // initialize math object: $20.2.2.16 Math.floor()
    m_math->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().floor),
                                    ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().floor, builtinMathFloor, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // initialize math object: $20.2.2.17 Math.fround()
    m_math->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().fround),
                                    ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().fround, builtinMathFround, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // initialize math object: $20.2.2.18 Math.hypot()
    m_math->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().hypot),
                                    ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().hypot, builtinMathHypot, 2, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // initialize math object: $20.2.2.19 Math.imul()
    m_math->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().imul),
                                    ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().imul, builtinMathIMul, 2, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // initialize math object: $20.2.2.20 Math.log()
    m_math->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().log),
                                    ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().log, builtinMathLog, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // initialize math object: $20.2.2.21 Math.log1p()
    m_math->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().log1p),
                                    ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().log1p, builtinMathLog1p, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // initialize math object: $20.2.2.22 Math.log10()
    m_math->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().log10),
                                    ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().log10, builtinMathLog10, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // initialize math object: $20.2.2.23 Math.log2()
    m_math->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().log2),
                                    ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().log2, builtinMathLog2, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // initialize math object: $20.2.2.24 Math.max()
    m_math->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().max),
                                    ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().max, builtinMathMax, 2, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // initialize math object: $20.2.2.25 Math.min()
    m_math->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().min),
                                    ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().min, builtinMathMin, 2, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // initialize math object: $20.2.2.26 Math.pow()
    m_math->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().pow),
                                    ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().pow, builtinMathPow, 2, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // initialize math object: $20.2.2.27 Math.random()
    m_math->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().random),
                                    ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().random, builtinMathRandom, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // initialize math object: $20.2.2.28 Math.round()
    m_math->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().round),
                                    ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().round, builtinMathRound, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // initialize math object: $20.2.2.29 Math.sign()
    m_math->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().sign),
                                    ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().sign, builtinMathSign, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // initialize math object: $20.2.2.30 Math.sin()
    m_math->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().sin),
                                    ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().sin, builtinMathSin, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // initialize math object: $20.2.2.31 Math.sinh()
    m_math->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().sinh),
                                    ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().sinh, builtinMathSinh, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // initialize math object: $20.2.2.32 Math.sqrt()
    m_math->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().sqrt),
                                    ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().sqrt, builtinMathSqrt, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // initialize math object: $20.2.2.33 Math.tan()
    m_math->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().tan),
                                    ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().tan, builtinMathTan, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // initialize math object: $20.2.2.34 Math.tanh()
    m_math->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().tanh),
                                    ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().tanh, builtinMathTanh, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // initialize math object: $20.2.2.35 Math.trunc()
    m_math->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().trunc),
                                    ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().trunc, builtinMathTrunc, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_math->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().sumPrecise),
                                    ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().sumPrecise, builtinMathSumPrecise, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_math->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().f16round),
                                    ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().f16round, builtinMathF16round, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));


    redefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().Math),
                        ObjectPropertyDescriptor(m_math, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}
} // namespace Escargot
