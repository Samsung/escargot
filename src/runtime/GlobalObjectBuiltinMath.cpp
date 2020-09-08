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
#include "GlobalObject.h"
#include "Context.h"
#include "VMInstance.h"
#include "StringObject.h"
#include "NativeFunctionObject.h"

#include "IEEE754.h"
#include <math.h>

namespace Escargot {

static Value builtinMathAbs(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    return Value(std::abs(argv[0].toNumber(state)));
}

static Value builtinMathMax(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    bool is_NaN = false;
    if (argc == 0) {
        double n_inf = -1 * std::numeric_limits<double>::infinity();
        return Value(n_inf);
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
        double qnan = std::numeric_limits<double>::quiet_NaN();
        return Value(qnan);
    }
    return Value(maxValue);
}

static Value builtinMathMin(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (argc == 0) {
        return Value(std::numeric_limits<double>::infinity());
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
        double qnan = std::numeric_limits<double>::quiet_NaN();
        return Value(qnan);
    }
    return Value(minValue);
}

static Value builtinMathRound(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    double x = argv[0].toNumber(state);
    if (x == static_cast<int64_t>(x)) {
        return Value(x);
    }
    if (x == -0.5)
        return Value(-0.0);
    else if (x > -0.5)
        return Value(round(x));
    else
        return Value(floor(x + 0.5));
}

static Value builtinMathSin(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    Value x = argv[0];
    return Value(ieee754::sin(x.toNumber(state)));
}

static Value builtinMathSinh(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    double x = argv[0].toNumber(state);
    return Value(ieee754::sinh(x));
}

static Value builtinMathCos(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    Value x = argv[0];
    return Value(ieee754::cos(x.toNumber(state)));
}

static Value builtinMathCosh(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    double x = argv[0].toNumber(state);
    return Value(ieee754::cosh(x));
}

static Value builtinMathAcos(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    double x = argv[0].toNumber(state);
    return Value(ieee754::acos(x));
}

static Value builtinMathAcosh(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    double x = argv[0].toNumber(state);
    return Value(ieee754::acosh(x));
}

static Value builtinMathAsin(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    double x = argv[0].toNumber(state);
    return Value(ieee754::asin(x));
}

static Value builtinMathAsinh(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    double x = argv[0].toNumber(state);
    return Value(ieee754::asinh(x));
}

static Value builtinMathAtan(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    double x = argv[0].toNumber(state);
    return Value(ieee754::atan(x));
}

static Value builtinMathAtan2(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    double y = argv[0].toNumber(state);
    double x = argv[1].toNumber(state);
    return Value(ieee754::atan2(y, x));
}

static Value builtinMathAtanh(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    double x = argv[0].toNumber(state);
    return Value(ieee754::atanh(x));
}

static Value builtinMathTan(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    double x = argv[0].toNumber(state);
    return Value(ieee754::tan(x));
}

static Value builtinMathTanh(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    double x = argv[0].toNumber(state);
    return Value(ieee754::tanh(x));
}

static Value builtinMathTrunc(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    double x = argv[0].toNumber(state);
    return Value(trunc(x));
}

static Value builtinMathSign(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    double x = argv[0].toNumber(state);
    if (std::isnan(x))
        return Value(std::numeric_limits<double>::quiet_NaN());
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
    return Value(sqrt(x.toNumber(state)));
}

static Value builtinMathPow(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    double x = argv[0].toNumber(state);
    double y = argv[1].toNumber(state);
    if (UNLIKELY(std::isnan(y)))
        return Value(std::numeric_limits<double>::quiet_NaN());
    if (UNLIKELY(std::abs(x) == 1 && std::isinf(y)))
        return Value(std::numeric_limits<double>::quiet_NaN());

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
                    return (result == 0 && std::isinf(p)) ? Value(pow(x, static_cast<double>(y))) // Avoid pow(double, int).
                                                          : Value(result);
                }

                return Value(p);
            }
            m *= m;
        }
    }

    if (std::isinf(x)) {
        if (x > 0) {
            if (y > 0) {
                return Value(std::numeric_limits<double>::infinity());
            } else {
                return Value(0.0);
            }
        } else {
            if (y > 0) {
                if (y == y_int && y_int % 2) { // odd
                    return Value(-std::numeric_limits<double>::infinity());
                } else {
                    return Value(std::numeric_limits<double>::infinity());
                }
            } else {
                if (y == y_int && y_int % 2) {
                    return Value(-0.0);
                } else {
                    return Value(0.0);
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
            return Value(std::numeric_limits<double>::infinity());
        }
    }

    if (y == 0.5) {
        return Value(sqrt(x));
    } else if (y == -0.5) {
        return Value(1.0 / sqrt(x));
    }

    return Value(pow(x, y));
}

static Value builtinMathCbrt(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    double x = argv[0].toNumber(state);
    return Value(ieee754::cbrt(x));
}

static Value builtinMathCeil(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    double x = argv[0].toNumber(state);

    // I add custom ceil implementation
    // because I found some problem from gcc implementation about negative zero
    return Value(ieee754::ceil(x));
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
    return Value(floor(x));
}

static Value builtinMathFround(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    double x = argv[0].toNumber(state);
    return Value(static_cast<double>(static_cast<float>(x)));
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
        return Value(std::numeric_limits<double>::infinity());
    }

    if (has_nan) {
        double qnan = std::numeric_limits<double>::quiet_NaN();
        return Value(qnan);
    }

    if (maxValue == 0) {
        return Value(0.0);
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
    return Value(std::sqrt(sum) * maxValue);
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
    return Value(ieee754::log(x));
}

static Value builtinMathLog1p(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    double x = argv[0].toNumber(state);
    return Value(ieee754::log1p(x));
}

static Value builtinMathLog10(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    double x = argv[0].toNumber(state);
    return Value(ieee754::log10(x));
}

static Value builtinMathLog2(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    double x = argv[0].toNumber(state);
    return Value(ieee754::log2(x));
}

static Value builtinMathRandom(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    std::uniform_real_distribution<double> distribution;
    return Value(distribution(state.context()->vmInstance()->randEngine()));
}

static Value builtinMathExp(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    double x = argv[0].toNumber(state);
    return Value(ieee754::exp(x));
}

static Value builtinMathExpm1(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    double x = argv[0].toNumber(state);
    return Value(ieee754::expm1(x));
}

void GlobalObject::installMath(ExecutionState& state)
{
    m_math = new Object(state);
    m_math->setGlobalIntrinsicObject(state);

    m_math->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, Value(state.context()->vmInstance()->globalSymbols().toStringTag)),
                                             ObjectPropertyDescriptor(Value(state.context()->staticStrings().Math.string()), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));

    // initialize math object: $20.2.1.6 Math.PI
    const StaticStrings* strings = &state.context()->staticStrings();
    m_math->defineOwnPropertyThrowsException(state, strings->PI, ObjectPropertyDescriptor(Value(3.1415926535897932), ObjectPropertyDescriptor::ValuePresent));
    // TODO(add reference)
    m_math->defineOwnPropertyThrowsException(state, strings->E, ObjectPropertyDescriptor(Value(2.718281828459045), ObjectPropertyDescriptor::ValuePresent));
    // http://www.ecma-international.org/ecma-262/5.1/#sec-15.8.1.3
    m_math->defineOwnPropertyThrowsException(state, strings->LN2, ObjectPropertyDescriptor(Value(0.6931471805599453), ObjectPropertyDescriptor::ValuePresent));
    // http://www.ecma-international.org/ecma-262/5.1/#sec-15.8.1.2
    m_math->defineOwnPropertyThrowsException(state, strings->LN10, ObjectPropertyDescriptor(Value(2.302585092994046), ObjectPropertyDescriptor::ValuePresent));
    // http://www.ecma-international.org/ecma-262/5.1/#sec-15.8.1.4
    m_math->defineOwnPropertyThrowsException(state, strings->LOG2E, ObjectPropertyDescriptor(Value(1.4426950408889634), ObjectPropertyDescriptor::ValuePresent));
    // http://www.ecma-international.org/ecma-262/5.1/#sec-15.8.1.5
    m_math->defineOwnPropertyThrowsException(state, strings->LOG10E, ObjectPropertyDescriptor(Value(0.4342944819032518), ObjectPropertyDescriptor::ValuePresent));
    // http://www.ecma-international.org/ecma-262/5.1/#sec-15.8.1.7
    m_math->defineOwnPropertyThrowsException(state, strings->SQRT1_2, ObjectPropertyDescriptor(Value(0.7071067811865476), ObjectPropertyDescriptor::ValuePresent));
    // http://www.ecma-international.org/ecma-262/5.1/#sec-15.8.1.8
    m_math->defineOwnPropertyThrowsException(state, strings->SQRT2, ObjectPropertyDescriptor(Value(1.4142135623730951), ObjectPropertyDescriptor::ValuePresent));

    // initialize math object: $20.2.2.1 Math.abs()
    m_math->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().abs),
                                             ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().abs, builtinMathAbs, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // initialize math object: $20.2.2.2 Math.acos()
    m_math->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().acos),
                                             ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().acos, builtinMathAcos, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // initialize math object: $20.2.2.3 Math.acosh()
    m_math->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().acosh),
                                             ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().acosh, builtinMathAcosh, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // initialize math object: $20.2.2.4 Math.asin()
    m_math->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().asin),
                                             ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().asin, builtinMathAsin, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // initialize math object: $20.2.2.5 Math.asinh()
    m_math->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().asinh),
                                             ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().asinh, builtinMathAsinh, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // initialize math object: $20.2.2.6 Math.atan()
    m_math->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().atan),
                                             ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().atan, builtinMathAtan, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // initialize math object: $20.2.2.7 Math.atanh()
    m_math->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().atanh),
                                             ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().atanh, builtinMathAtanh, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // initialize math object: $20.2.2.8 Math.atan2()
    m_math->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().atan2),
                                             ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().atan2, builtinMathAtan2, 2, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // initialize math object: $20.2.2.9 Math.cbrt()
    m_math->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().cbrt),
                                             ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().cbrt, builtinMathCbrt, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // initialize math object: $20.2.2.10 Math.ceil()
    m_math->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().ceil),
                                             ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().ceil, builtinMathCeil, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // initialize math object: $20.2.2.11 Math.clz32()
    m_math->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().clz32),
                                             ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().clz32, builtinMathClz32, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // initialize math object: $20.2.2.12 Math.cos()
    m_math->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().cos),
                                             ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().cos, builtinMathCos, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // initialize math object: $20.2.2.13 Math.cosh()
    m_math->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().cosh),
                                             ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().cosh, builtinMathCosh, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // initialize math object: $20.2.2.14 Math.exp()
    m_math->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().exp),
                                             ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().exp, builtinMathExp, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // initialize math object: $20.2.2.15 Math.expm1()
    m_math->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().expm1),
                                             ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().expm1, builtinMathExpm1, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // initialize math object: $20.2.2.16 Math.floor()
    m_math->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().floor),
                                             ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().floor, builtinMathFloor, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // initialize math object: $20.2.2.17 Math.fround()
    m_math->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().fround),
                                             ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().fround, builtinMathFround, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // initialize math object: $20.2.2.18 Math.hypot()
    m_math->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().hypot),
                                             ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().hypot, builtinMathHypot, 2, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // initialize math object: $20.2.2.19 Math.imul()
    m_math->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().imul),
                                             ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().imul, builtinMathIMul, 2, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // initialize math object: $20.2.2.20 Math.log()
    m_math->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().log),
                                             ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().log, builtinMathLog, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // initialize math object: $20.2.2.21 Math.log1p()
    m_math->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().log1p),
                                             ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().log1p, builtinMathLog1p, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // initialize math object: $20.2.2.22 Math.log10()
    m_math->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().log10),
                                             ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().log10, builtinMathLog10, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // initialize math object: $20.2.2.23 Math.log2()
    m_math->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().log2),
                                             ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().log2, builtinMathLog2, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // initialize math object: $20.2.2.24 Math.max()
    m_math->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().max),
                                             ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().max, builtinMathMax, 2, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // initialize math object: $20.2.2.25 Math.min()
    m_math->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().min),
                                             ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().min, builtinMathMin, 2, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // initialize math object: $20.2.2.26 Math.pow()
    m_math->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().pow),
                                             ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().pow, builtinMathPow, 2, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // initialize math object: $20.2.2.27 Math.random()
    m_math->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().random),
                                             ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().random, builtinMathRandom, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // initialize math object: $20.2.2.28 Math.round()
    m_math->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().round),
                                             ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().round, builtinMathRound, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // initialize math object: $20.2.2.29 Math.sign()
    m_math->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().sign),
                                             ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().sign, builtinMathSign, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // initialize math object: $20.2.2.30 Math.sin()
    m_math->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().sin),
                                             ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().sin, builtinMathSin, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // initialize math object: $20.2.2.31 Math.sinh()
    m_math->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().sinh),
                                             ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().sinh, builtinMathSinh, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // initialize math object: $20.2.2.32 Math.sqrt()
    m_math->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().sqrt),
                                             ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().sqrt, builtinMathSqrt, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // initialize math object: $20.2.2.33 Math.tan()
    m_math->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().tan),
                                             ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().tan, builtinMathTan, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // initialize math object: $20.2.2.34 Math.tanh()
    m_math->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().tanh),
                                             ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().tanh, builtinMathTanh, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // initialize math object: $20.2.2.35 Math.trunc()
    m_math->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().trunc),
                                             ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().trunc, builtinMathTrunc, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().Math),
                      ObjectPropertyDescriptor(m_math, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}
}
