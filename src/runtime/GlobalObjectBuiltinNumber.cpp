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

#include "Escargot.h"
#include "GlobalObject.h"
#include "Context.h"
#include "NumberObject.h"

// dtoa
#include "ieee.h"
#include "double-conversion.h"

#define NUMBER_TO_STRING_BUFFER_LENGTH 96

namespace Escargot {

static int itoa(int64_t value, char* sp, int radix)
{
    char tmp[256]; // be careful with the length of the buffer
    char* tp = tmp;
    int i;
    uint64_t v;

    int sign = (radix == 10 && value < 0);
    if (sign) {
        v = -value;
    } else {
        v = (uint64_t)value;
    }

    while (v || tp == tmp) {
        i = v % radix;
        v /= radix; // v/=radix uses less CPU clocks than v=v/radix does
        if (i < 10) {
            *tp++ = i + '0';
        } else {
            *tp++ = i + 'a' - 10;
        }
    }

    int64_t len = tp - tmp;

    if (sign) {
        *sp++ = '-';
        len++;
    }

    while (tp > tmp) {
        *sp++ = *--tp;
    }
    *sp++ = 0;

    return len;
}

static Value builtinNumberConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    NumberObject* numObj;
    if (isNewExpression) {
        numObj = thisValue.asPointerValue()->asObject()->asNumberObject();
        if (argc == 0) {
            numObj->setPrimitiveValue(state, 0);
        } else {
            numObj->setPrimitiveValue(state, argv[0].toNumber(state));
        }
        return numObj;
    } else {
        if (argc == 0) {
            return Value(0);
        } else {
            return Value(argv[0].toNumber(state));
        }
    }
}

static Value builtinNumberToFixed(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    double number = 0.0;

    if (thisValue.isNumber()) {
        number = thisValue.asNumber();
    } else if (thisValue.isPointerValue() && thisValue.asPointerValue()->isNumberObject()) {
        number = thisValue.asPointerValue()->asNumberObject()->primitiveValue();
    } else {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Number.string(), true, state.context()->staticStrings().toFixed.string(), errorMessage_GlobalObject_ThisNotNumber);
    }

    if (argc == 0) {
        bool isInteger = (static_cast<int64_t>(number) == number);
        if (isInteger) {
            char buffer[256];
            itoa(static_cast<int64_t>(number), buffer, 10);
            return new ASCIIString(buffer);
        } else {
            return Value(round(number)).toString(state);
        }
    } else if (argc >= 1) {
        double digitD = argv[0].toNumber(state);
        if (digitD == 0 || std::isnan(digitD)) {
            return Value(round(number)).toString(state);
        }
        int digit = (int)trunc(digitD);
        if (digit < 0 || digit > 20) {
            ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, state.context()->staticStrings().Number.string(), true, state.context()->staticStrings().toFixed.string(), errorMessage_GlobalObject_RangeError);
        }
        if (std::isnan(number) || std::isinf(number)) {
            return Value(number).toString(state);
        } else if (std::abs(number) >= pow(10, 21)) {
            return Value(round(number)).toString(state);
        }

        char buffer[NUMBER_TO_STRING_BUFFER_LENGTH];
        double_conversion::StringBuilder builder(buffer, NUMBER_TO_STRING_BUFFER_LENGTH);
        double_conversion::DoubleToStringConverter::EcmaScriptConverter().ToFixed(number, digit, &builder);
        return Value(new ASCIIString(builder.Finalize()));
    }
    return Value();
}

static Value builtinNumberToExponential(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    double number = 0.0;

    if (thisValue.isNumber()) {
        number = thisValue.asNumber();
    } else if (thisValue.isPointerValue() && thisValue.asPointerValue()->isNumberObject()) {
        number = thisValue.asPointerValue()->asNumberObject()->primitiveValue();
    } else {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Number.string(), true, state.context()->staticStrings().toExponential.string(), errorMessage_GlobalObject_ThisNotNumber);
    }

    int digit = 0; // only used when an argument is given
    bool undefinedDigit = argc == 0 || argv[0].isUndefined();
    if (!undefinedDigit) {
        double fractionDigits = argv[0].toNumber(state);
        digit = (int)trunc(fractionDigits);
    }
    if (std::isnan(number)) { // 3
        return state.context()->staticStrings().NaN.string();
    }

    if (std::isinf(number)) { // 6
        if (number < 0) {
            return state.context()->staticStrings().NegativeInfinity.string();
        } else {
            return state.context()->staticStrings().Infinity.string();
        }
    }
    if (digit < 0 || digit > 20) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, state.context()->staticStrings().Number.string(), true, state.context()->staticStrings().toExponential.string(), errorMessage_GlobalObject_RangeError);
    }

    char buffer[NUMBER_TO_STRING_BUFFER_LENGTH];
    double_conversion::StringBuilder builder(buffer, NUMBER_TO_STRING_BUFFER_LENGTH);
    if (undefinedDigit) {
        double_conversion::DoubleToStringConverter::EcmaScriptConverter().ToExponential(number, -1, &builder);
    } else {
        double_conversion::DoubleToStringConverter::EcmaScriptConverter().ToExponential(number, digit, &builder);
    }
    return Value(new ASCIIString(builder.Finalize()));
}

static Value builtinNumberToPrecision(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    double number = 0.0;

    if (thisValue.isNumber()) {
        number = thisValue.asNumber();
    } else if (thisValue.isPointerValue() && thisValue.asPointerValue()->isNumberObject()) {
        number = thisValue.asPointerValue()->asNumberObject()->primitiveValue();
    } else {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Number.string(), true, state.context()->staticStrings().toPrecision.string(), errorMessage_GlobalObject_ThisNotNumber);
    }

    if (argc == 0 || argv[0].isUndefined()) {
        return Value(number).toString(state);
    } else if (argc >= 1) {
        double p_d = argv[0].toNumber(state);
        if (std::isnan(number)) {
            return state.context()->staticStrings().NaN.string();
        }
        if (std::isinf(number)) {
            if (number < 0) {
                return state.context()->staticStrings().NegativeInfinity.string();
            } else {
                return state.context()->staticStrings().Infinity.string();
            }
        } else {
            int p = (int)trunc(p_d);
            if (p < 1 || p > 21) {
                ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, state.context()->staticStrings().Number.string(), true, state.context()->staticStrings().toPrecision.string(), errorMessage_GlobalObject_RangeError);
            }
            char buffer[NUMBER_TO_STRING_BUFFER_LENGTH];
            double_conversion::StringBuilder builder(buffer, NUMBER_TO_STRING_BUFFER_LENGTH);
            double_conversion::DoubleToStringConverter::EcmaScriptConverter().ToPrecision(number, p, &builder);
            return Value(new ASCIIString(builder.Finalize()));
        }
    }
    return Value();
}

static Value builtinNumberToString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    double number = 0.0;

    if (thisValue.isNumber()) {
        number = thisValue.asNumber();
    } else if (thisValue.isPointerValue() && thisValue.asPointerValue()->isNumberObject()) {
        number = thisValue.asPointerValue()->asNumberObject()->primitiveValue();
    } else {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Number.string(), true, state.context()->staticStrings().toString.string(), errorMessage_GlobalObject_ThisNotNumber);
    }

    if (std::isnan(number) || std::isinf(number)) {
        return (Value(number).toString(state));
    }
    double radix = 10;
    if (argc > 0 && !argv[0].isUndefined()) {
        radix = argv[0].toInteger(state);
        if (radix < 2 || radix > 36) {
            ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, state.context()->staticStrings().Number.string(), true, state.context()->staticStrings().toString.string(), errorMessage_GlobalObject_RadixInvalidRange);
        }
    }
    if (radix == 10) {
        return (Value(number).toString(state));
    } else {
        bool isInteger = (static_cast<int64_t>(number) == number);
        if (isInteger) {
            bool minusFlag = (number < 0) ? 1 : 0;
            number = (number < 0) ? (-1 * number) : number;
            char buffer[256];
            if (minusFlag) {
                buffer[0] = '-';
                itoa(static_cast<int64_t>(number), &buffer[1], radix);
            } else {
                itoa(static_cast<int64_t>(number), buffer, radix);
            }
            return (new ASCIIString(buffer));
        } else {
            ASSERT(Value(number).isDouble());
            NumberObject::RadixBuffer s;
            const char* str = NumberObject::toStringWithRadix(state, s, number, radix);
            return new ASCIIString(str);
        }
    }
    return Value();
}

static Value builtinNumberToLocaleString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    RESOLVE_THIS_BINDING_TO_OBJECT(thisObject, Number, toLocaleString);

    if (!thisObject->isNumberObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, errorMessage_GlobalObject_ThisNotNumber);
    }

    ObjectGetResult toStrFuncGetResult = thisObject->get(state, ObjectPropertyName(state.context()->staticStrings().toString));
    if (toStrFuncGetResult.hasValue()) {
        Value toStrFunc = toStrFuncGetResult.value(state, thisObject);
        if (toStrFunc.isFunction()) {
            // toLocaleString() ignores the first argument, unlike toString()
            return FunctionObject::call(state, toStrFunc, thisObject, 0, argv);
        }
    }
    ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Number.string(), true, state.context()->staticStrings().toLocaleString.string(), errorMessage_GlobalObject_ToLocaleStringNotCallable);
    RELEASE_ASSERT_NOT_REACHED();
}

static Value builtinNumberValueOf(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (thisValue.isNumber()) {
        return Value(thisValue);
    } else if (thisValue.isObject() && thisValue.asObject()->isNumberObject()) {
        return Value(thisValue.asPointerValue()->asNumberObject()->primitiveValue());
    }
    ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, errorMessage_GlobalObject_ThisNotNumber);
    RELEASE_ASSERT_NOT_REACHED();
}

static Value builtinNumberIsFinite(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (!argv[0].isNumber()) {
        return Value(Value::False);
    }

    double number = argv[0].asNumber();
    if (std::isnan(number) || number == std::numeric_limits<double>::infinity() || number == -std::numeric_limits<double>::infinity()) {
        return Value(Value::False);
    }
    return Value(Value::True);
}

static Value builtinNumberIsInteger(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (!argv[0].isNumber()) {
        return Value(Value::False);
    }

    double number = argv[0].asNumber();
    if (std::isnan(number) || number == std::numeric_limits<double>::infinity() || number == -std::numeric_limits<double>::infinity()) {
        return Value(Value::False);
    }

    double integer = argv[0].toInteger(state);
    if (number != integer) {
        return Value(Value::False);
    }
    return Value(Value::True);
}

static Value builtinNumberIsNaN(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (!argv[0].isNumber()) {
        return Value(Value::False);
    }

    double number = argv[0].asNumber();
    if (std::isnan(number)) {
        return Value(Value::True);
    }
    return Value(Value::False);
}

static Value builtinNumberIsSafeInteger(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (!argv[0].isNumber()) {
        return Value(Value::False);
    }

    double number = argv[0].asNumber();
    if (std::isnan(number) || number == std::numeric_limits<double>::infinity() || number == -std::numeric_limits<double>::infinity()) {
        return Value(Value::False);
    }

    double integer = argv[0].toInteger(state);
    if (number != integer) {
        return Value(Value::False);
    }

    if (std::abs(integer) <= 9007199254740991.0) {
        return Value(Value::True);
    }
    return Value(Value::False);
}

void GlobalObject::installNumber(ExecutionState& state)
{
    const StaticStrings* strings = &state.context()->staticStrings();
    m_number = new FunctionObject(state, NativeFunctionInfo(strings->Number, builtinNumberConstructor, 1, [](ExecutionState& state, CodeBlock* codeBlock, size_t argc, Value* argv) {
                                      return (new NumberObject(state))->asObject();
                                  }),
                                  FunctionObject::__ForBuiltin__);
    m_number->markThisObjectDontNeedStructureTransitionTable(state);
    m_number->setPrototype(state, m_functionPrototype);
    m_numberPrototype = m_objectPrototype;
    m_numberPrototype = new NumberObject(state, 0);
    m_numberPrototype->markThisObjectDontNeedStructureTransitionTable(state);
    m_numberPrototype->setPrototype(state, m_objectPrototype);
    m_number->setFunctionPrototype(state, m_numberPrototype);
    m_numberPrototype->defineOwnProperty(state, ObjectPropertyName(strings->constructor), ObjectPropertyDescriptor(m_number, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_numberPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->toString),
                                                        ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->toString, builtinNumberToString, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_numberPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->toLocaleString),
                                                        ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->toLocaleString, builtinNumberToLocaleString, 0, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_numberPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->toFixed),
                                                        ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->toFixed, builtinNumberToFixed, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_numberPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->toExponential),
                                                        ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->toExponential, builtinNumberToExponential, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_numberPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->toPrecision),
                                                        ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->toPrecision, builtinNumberToPrecision, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // $20.1.3.26 Number.prototype.valueOf
    m_numberPrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->valueOf),
                                                        ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->valueOf, builtinNumberValueOf, 0, nullptr, NativeFunctionInfo::Strict)),
                                                                                 (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));


    m_number->setFunctionPrototype(state, m_numberPrototype);

    ObjectPropertyDescriptor::PresentAttribute allFalsePresent = (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::NonWritablePresent
                                                                                                              | ObjectPropertyDescriptor::NonEnumerablePresent
                                                                                                              | ObjectPropertyDescriptor::NonConfigurablePresent);

    // $20.1.2.1 Number.EPSILON
    m_number->defineOwnPropertyThrowsException(state, strings->EPSILON, ObjectPropertyDescriptor(Value(std::numeric_limits<double>::epsilon()), allFalsePresent));
    // $20.1.2.2 Number.isFinite
    m_number->defineOwnPropertyThrowsException(state, strings->isFinite,
                                               ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->isFinite, builtinNumberIsFinite, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // $20.1.2.3 Number.isInteger
    m_number->defineOwnPropertyThrowsException(state, strings->isInteger,
                                               ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->isInteger, builtinNumberIsInteger, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // $20.1.2.3 Number.isNaN
    m_number->defineOwnPropertyThrowsException(state, strings->isNaN,
                                               ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->isNaN, builtinNumberIsNaN, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // $20.1.2.5 Number.isSafeInteger
    m_number->defineOwnPropertyThrowsException(state, strings->isSafeInteger,
                                               ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->isSafeInteger, builtinNumberIsSafeInteger, 1, nullptr, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // $20.1.2.6 Number.MAX_SAFE_INTEGER
    m_number->defineOwnPropertyThrowsException(state, strings->MAX_SAFE_INTEGER, ObjectPropertyDescriptor(Value(9007199254740991.0), (ObjectPropertyDescriptor::PresentAttribute)allFalsePresent));
    // $20.1.2.7 Number.MAX_VALUE
    m_number->defineOwnPropertyThrowsException(state, strings->MAX_VALUE, ObjectPropertyDescriptor(Value(1.7976931348623157E+308), (ObjectPropertyDescriptor::PresentAttribute)allFalsePresent));
    // $20.1.2.8 Number.MIN_SAFE_INTEGER
    m_number->defineOwnPropertyThrowsException(state, strings->MIN_SAFE_INTEGER, ObjectPropertyDescriptor(Value(-9007199254740991.0), (ObjectPropertyDescriptor::PresentAttribute)allFalsePresent));
    // $20.1.2.9 Number.MIN_VALUE
    m_number->defineOwnPropertyThrowsException(state, strings->MIN_VALUE, ObjectPropertyDescriptor(Value(5E-324), allFalsePresent));
    // $20.1.2.10 Number.NaN
    m_number->defineOwnPropertyThrowsException(state, strings->NaN, ObjectPropertyDescriptor(Value(std::numeric_limits<double>::quiet_NaN()), allFalsePresent));
    // $20.1.2.11 Number.NEGATIVE_INFINITY
    m_number->defineOwnPropertyThrowsException(state, strings->NEGATIVE_INFINITY, ObjectPropertyDescriptor(Value(-std::numeric_limits<double>::infinity()), allFalsePresent));
    // $20.1.2.14 Number.POSITIVE_INFINITY
    m_number->defineOwnPropertyThrowsException(state, strings->POSITIVE_INFINITY, ObjectPropertyDescriptor(Value(std::numeric_limits<double>::infinity()), allFalsePresent));

    defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().Number),
                      ObjectPropertyDescriptor(m_number, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}
}
