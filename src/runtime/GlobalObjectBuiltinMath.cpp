#include "Escargot.h"
#include "GlobalObject.h"
#include "Context.h"
#include "StringObject.h"

#include <math.h>

namespace Escargot {

static Value builtinMathAbs(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    return Value(std::abs(argv[0].toNumber(state)));
}

static Value builtinMathMax(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (argc == 0) {
        double n_inf = -1 * std::numeric_limits<double>::infinity();
        return Value(n_inf);
    } else {
        double maxValue = argv[0].toNumber(state);
        for (unsigned i = 1; i < argc; i++) {
            double value = argv[i].toNumber(state);
            double qnan = std::numeric_limits<double>::quiet_NaN();
            if (std::isnan(value))
                return Value(qnan);
            if (value > maxValue || (!value && !maxValue && !std::signbit(value)))
                maxValue = value;
        }
        return Value(maxValue);
    }
    return Value();
}

static Value builtinMathMin(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (argc == 0) {
        return Value(std::numeric_limits<double>::infinity());
    } else {
        double minValue = argv[0].toNumber(state);
        for (unsigned i = 1; i < argc; i++) {
            double value = argv[i].toNumber(state);
            double qnan = std::numeric_limits<double>::quiet_NaN();
            if (std::isnan(value))
                return Value(qnan);
            if (value < minValue || (!value && !minValue && std::signbit(value)))
                minValue = value;
        }
        return Value(minValue);
    }
    return Value();
}

static Value builtinMathRound(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    double x = argv[0].toNumber(state);
    if (x == -0.5)
        return Value(-0.0);
    else if (x > -0.5)
        return Value(round(x));
    else
        return Value(floor(x + 0.5));
}

static Value builtinMathSin(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    Value x = argv[0];
    return Value(sin(x.toNumber(state)));
}

static Value builtinMathCos(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    Value x = argv[0];
    return Value(cos(x.toNumber(state)));
}

static Value builtinMathTan(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
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
    } else if (std::isinf(x))
        return Value(std::numeric_limits<double>::quiet_NaN());
    return Value(tan(x));
}

static Value builtinMathSqrt(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    Value x = argv[0];
    return Value(sqrt(x.toNumber(state)));
}

static Value builtinMathPow(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
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

void GlobalObject::installMath(ExecutionState& state)
{
    m_math = new Object(state);
    m_math->markThisObjectDontNeedStructureTransitionTable(state);

    // initialize math object: $20.2.1.6 Math.PI
    const StaticStrings* strings = &state.context()->staticStrings();
    m_math->defineOwnPropertyThrowsException(state, strings->PI, ObjectPropertyDescriptorForDefineOwnProperty(Value(3.1415926535897932), ObjectPropertyDescriptor::NotPresent));
    // TODO(add reference)
    m_math->defineOwnPropertyThrowsException(state, strings->E, ObjectPropertyDescriptorForDefineOwnProperty(Value(2.718281828459045), ObjectPropertyDescriptor::NotPresent));
    // http://www.ecma-international.org/ecma-262/5.1/#sec-15.8.1.3
    m_math->defineOwnPropertyThrowsException(state, strings->LN2, ObjectPropertyDescriptorForDefineOwnProperty(Value(0.6931471805599453), ObjectPropertyDescriptor::NotPresent));
    // http://www.ecma-international.org/ecma-262/5.1/#sec-15.8.1.2
    m_math->defineOwnPropertyThrowsException(state, strings->LN10, ObjectPropertyDescriptorForDefineOwnProperty(Value(2.302585092994046), ObjectPropertyDescriptor::NotPresent));
    // http://www.ecma-international.org/ecma-262/5.1/#sec-15.8.1.4
    m_math->defineOwnPropertyThrowsException(state, strings->LOG2E, ObjectPropertyDescriptorForDefineOwnProperty(Value(1.4426950408889634), ObjectPropertyDescriptor::NotPresent));
    // http://www.ecma-international.org/ecma-262/5.1/#sec-15.8.1.5
    m_math->defineOwnPropertyThrowsException(state, strings->LOG10E, ObjectPropertyDescriptorForDefineOwnProperty(Value(0.4342944819032518), ObjectPropertyDescriptor::NotPresent));
    // http://www.ecma-international.org/ecma-262/5.1/#sec-15.8.1.7
    m_math->defineOwnPropertyThrowsException(state, strings->SQRT1_2, ObjectPropertyDescriptorForDefineOwnProperty(Value(0.7071067811865476), ObjectPropertyDescriptor::NotPresent));
    // http://www.ecma-international.org/ecma-262/5.1/#sec-15.8.1.8
    m_math->defineOwnPropertyThrowsException(state, strings->SQRT2, ObjectPropertyDescriptorForDefineOwnProperty(Value(1.4142135623730951), ObjectPropertyDescriptor::NotPresent));

    // initialize math object: $20.2.2.12 Math.cos()
    m_math->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().cos),
                                             Object::ObjectPropertyDescriptorForDefineOwnProperty(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().cos, builtinMathCos, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::EnumerablePresent)));
    // initialize math object: $20.2.2.24 Math.abs()
    m_math->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().abs),
                                             Object::ObjectPropertyDescriptorForDefineOwnProperty(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().abs, builtinMathAbs, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::EnumerablePresent)));
    // initialize math object: $20.2.2.24 Math.max()
    m_math->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().max),
                                             Object::ObjectPropertyDescriptorForDefineOwnProperty(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().max, builtinMathMax, 2, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::EnumerablePresent)));
    // initialize math object: $20.2.2.25 Math.min()
    m_math->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().min),
                                             Object::ObjectPropertyDescriptorForDefineOwnProperty(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().min, builtinMathMin, 2, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::EnumerablePresent)));
    // initialize math object: $20.2.2.26 Math.pow()
    m_math->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().pow),
                                             Object::ObjectPropertyDescriptorForDefineOwnProperty(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().pow, builtinMathPow, 2, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::EnumerablePresent)));
    // initialize math object: $20.2.2.28 Math.round()
    m_math->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().round),
                                             Object::ObjectPropertyDescriptorForDefineOwnProperty(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().round, builtinMathRound, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::EnumerablePresent)));
    // initialize math object: $20.2.2.30 Math.sin()
    m_math->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().sin),
                                             Object::ObjectPropertyDescriptorForDefineOwnProperty(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().sin, builtinMathSin, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::EnumerablePresent)));
    // initialize math object: $20.2.2.32 Math.sqrt()
    m_math->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().sqrt),
                                             Object::ObjectPropertyDescriptorForDefineOwnProperty(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().sqrt, builtinMathSqrt, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::EnumerablePresent)));
    // initialize math object: $20.2.2.33 Math.tan()
    m_math->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->staticStrings().tan),
                                             Object::ObjectPropertyDescriptorForDefineOwnProperty(new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().tan, builtinMathTan, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::EnumerablePresent)));

    defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().Math),
                      Object::ObjectPropertyDescriptorForDefineOwnProperty(m_math, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::EnumerablePresent)));
}
}
