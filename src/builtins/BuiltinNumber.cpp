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
#include "runtime/NumberObject.h"
#include "runtime/NativeFunctionObject.h"

// dtoa
#include "ieee.h"
#include "double-conversion.h"

#if defined(ENABLE_ICU) && defined(ENABLE_INTL)
#include "intl/IntlNumberFormat.h"
#endif

#define NUMBER_TO_STRING_BUFFER_LENGTH 128

namespace Escargot {

static int itoa(int64_t value, char* sp, int radix)
{
    char tmp[256]; // be careful with the length of the buffer
    char* tp = tmp;
    uint64_t v;

    int sign = (radix == 10 && value < 0);
    if (sign) {
        v = -value;
    } else {
        v = (uint64_t)value;
    }

    while (v || tp == tmp) {
        int i = v % radix;
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

static Value builtinNumberConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    double num = 0;
    if (argc > 0) {
        if (UNLIKELY(argv[0].isBigInt())) {
            num = argv[0].asBigInt()->toNumber();
        } else {
            num = argv[0].toNumber(state);
        }
    }

    if (!newTarget.hasValue()) {
        return Value(Value::DoubleToIntConvertibleTestNeeds, num);
    } else {
        Object* proto = Object::getPrototypeFromConstructor(state, newTarget.value(), [](ExecutionState& state, Context* constructorRealm) -> Object* {
            return constructorRealm->globalObject()->numberPrototype();
        });
        NumberObject* numObj = new NumberObject(state, proto, num);
        return numObj;
    }
}

static Value builtinNumberToFixed(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    double number = 0.0;

    if (thisValue.isNumber()) {
        number = thisValue.asNumber();
    } else if (thisValue.isPointerValue() && thisValue.asPointerValue()->isNumberObject()) {
        number = thisValue.asPointerValue()->asNumberObject()->primitiveValue();
    } else {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().Number.string(), true, state.context()->staticStrings().toFixed.string(), ErrorObject::Messages::GlobalObject_ThisNotNumber);
    }

    Value fractionDigits = argv[0];
    int digit = fractionDigits.toInteger(state);
    if (digit < 0 || digit > 100) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, state.context()->staticStrings().Number.string(), true, state.context()->staticStrings().toFixed.string(), ErrorObject::Messages::GlobalObject_RangeError);
    }

    if (std::isnan(number)) {
        return state.context()->staticStrings().NaN.string();
    }

    if (std::isinf(number)) {
        if (number < 0) {
            return state.context()->staticStrings().NegativeInfinity.string();
        } else {
            return state.context()->staticStrings().Infinity.string();
        }
    }

    if (std::abs(number) >= pow(10, 21)) {
        return Value(Value::DoubleToIntConvertibleTestNeeds, number).toString(state);
    }

    char buffer[NUMBER_TO_STRING_BUFFER_LENGTH];
    double_conversion::StringBuilder builder(buffer, NUMBER_TO_STRING_BUFFER_LENGTH);
    double_conversion::DoubleToStringConverter::EcmaScriptConverter().ToFixed(number, digit, &builder);
    auto len = builder.position();
    return Value(String::fromASCII(builder.Finalize(), len));
}

static Value builtinNumberToExponential(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    double number = 0.0;

    if (thisValue.isNumber()) {
        number = thisValue.asNumber();
    } else if (thisValue.isPointerValue() && thisValue.asPointerValue()->isNumberObject()) {
        number = thisValue.asPointerValue()->asNumberObject()->primitiveValue();
    } else {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().Number.string(), true, state.context()->staticStrings().toExponential.string(), ErrorObject::Messages::GlobalObject_ThisNotNumber);
    }

    Value fractionDigits = argv[0];
    int digit = fractionDigits.toInteger(state);

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

    if (digit < 0 || digit > 100) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, state.context()->staticStrings().Number.string(), true, state.context()->staticStrings().toExponential.string(), ErrorObject::Messages::GlobalObject_RangeError);
    }

    char buffer[NUMBER_TO_STRING_BUFFER_LENGTH];
    double_conversion::StringBuilder builder(buffer, NUMBER_TO_STRING_BUFFER_LENGTH);
    if (fractionDigits.isUndefined()) {
        double_conversion::DoubleToStringConverter::EcmaScriptConverter().ToExponential(number, -1, &builder);
    } else {
        double_conversion::DoubleToStringConverter::EcmaScriptConverter().ToExponential(number, digit, &builder);
    }
    auto len = builder.position();
    return Value(String::fromASCII(builder.Finalize(), len));
}

static Value builtinNumberToPrecision(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    double number = 0.0;

    if (thisValue.isNumber()) {
        number = thisValue.asNumber();
    } else if (thisValue.isPointerValue() && thisValue.asPointerValue()->isNumberObject()) {
        number = thisValue.asPointerValue()->asNumberObject()->primitiveValue();
    } else {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().Number.string(), true, state.context()->staticStrings().toPrecision.string(), ErrorObject::Messages::GlobalObject_ThisNotNumber);
    }

    Value precision = argv[0];
    if (precision.isUndefined()) {
        return Value(Value::DoubleToIntConvertibleTestNeeds, number).toString(state);
    }

    int p = precision.toInteger(state);

    if (std::isnan(number)) {
        return state.context()->staticStrings().NaN.string();
    }

    if (std::isinf(number)) {
        if (number < 0) {
            return state.context()->staticStrings().NegativeInfinity.string();
        } else {
            return state.context()->staticStrings().Infinity.string();
        }
    }

    if (p < 1 || p > 100) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, state.context()->staticStrings().Number.string(), true, state.context()->staticStrings().toPrecision.string(), ErrorObject::Messages::GlobalObject_RangeError);
    }

    char buffer[NUMBER_TO_STRING_BUFFER_LENGTH];
    double_conversion::StringBuilder builder(buffer, NUMBER_TO_STRING_BUFFER_LENGTH);
    double_conversion::DoubleToStringConverter::EcmaScriptConverter().ToPrecision(number, p, &builder);
    auto len = builder.position();
    return Value(String::fromASCII(builder.Finalize(), len));
}

static Value builtinNumberToString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    double number = 0.0;

    if (thisValue.isNumber()) {
        number = thisValue.asNumber();
    } else if (thisValue.isPointerValue() && thisValue.asPointerValue()->isNumberObject()) {
        number = thisValue.asPointerValue()->asNumberObject()->primitiveValue();
    } else {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().Number.string(), true, state.context()->staticStrings().toString.string(), ErrorObject::Messages::GlobalObject_ThisNotNumber);
    }

    double radix = 10;
    if (argc > 0 && !argv[0].isUndefined()) {
        radix = argv[0].toInteger(state);
        if (radix < 2 || radix > 36) {
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, state.context()->staticStrings().Number.string(), true, state.context()->staticStrings().toString.string(), ErrorObject::Messages::GlobalObject_RadixInvalidRange);
        }
    }
    if (std::isnan(number) || std::isinf(number)) {
        return Value(Value::DoubleToIntConvertibleTestNeeds, number).toString(state);
    }
    if (radix == 10) {
        return Value(Value::DoubleToIntConvertibleTestNeeds, number).toString(state);
    }

    bool isInteger = (static_cast<int64_t>(number) == number);
    if (isInteger) {
        bool minusFlag = (number < 0) ? 1 : 0;
        number = (number < 0) ? (-1 * number) : number;
        char buffer[256];
        size_t len;
        if (minusFlag) {
            buffer[0] = '-';
            len = itoa(static_cast<int64_t>(number), &buffer[1], radix);
            len += 1;
        } else {
            len = itoa(static_cast<int64_t>(number), buffer, radix);
        }
        return String::fromASCII(buffer, len);
    } else {
        ASSERT(Value(Value::DoubleToIntConvertibleTestNeeds, number).isDouble());
        NumberObject::RadixBuffer s;
        const char* str = NumberObject::toStringWithRadix(state, s, number, radix);
        return String::fromASCII(str, strnlen(str, sizeof(NumberObject::RadixBuffer)));
    }
}

static Value builtinNumberToLocaleString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    Object* thisObject = thisValue.toObject(state);
    if (!thisObject->isNumberObject()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, ErrorObject::Messages::GlobalObject_ThisNotNumber);
    }

#if defined(ENABLE_ICU) && defined(ENABLE_INTL_NUMBERFORMAT)
    Value locales = argc > 0 ? argv[0] : Value();
    Value options = argc > 1 ? argv[1] : Value();
    Object* numberFormat = IntlNumberFormat::create(state, state.context(), locales, options);
    double x = 0;
    if (thisValue.isNumber()) {
        x = thisValue.asNumber();
    } else if (thisValue.isObject() && thisValue.asObject()->isNumberObject()) {
        x = thisValue.asPointerValue()->asNumberObject()->primitiveValue();
    } else {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, ErrorObject::Messages::GlobalObject_ThisNotNumber);
    }
    auto result = IntlNumberFormat::format(state, numberFormat, x);

    return new UTF16String(result.data(), result.length());
#else

    ObjectGetResult toStrFuncGetResult = thisObject->get(state, ObjectPropertyName(state.context()->staticStrings().toString));
    if (toStrFuncGetResult.hasValue()) {
        Value toStrFunc = toStrFuncGetResult.value(state, thisObject);
        if (toStrFunc.isCallable()) {
            // toLocaleString() ignores the first argument, unlike toString()
            return Object::call(state, toStrFunc, thisObject, 0, argv);
        }
    }
    ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, state.context()->staticStrings().Number.string(), true, state.context()->staticStrings().toLocaleString.string(), ErrorObject::Messages::GlobalObject_ToLocaleStringNotCallable);
    RELEASE_ASSERT_NOT_REACHED();
    return Value();
#endif
}

static Value builtinNumberValueOf(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    if (thisValue.isNumber()) {
        return Value(thisValue);
    } else if (thisValue.isObject() && thisValue.asObject()->isNumberObject()) {
        return Value(Value::DoubleToIntConvertibleTestNeeds, thisValue.asPointerValue()->asNumberObject()->primitiveValue());
    }
    ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, ErrorObject::Messages::GlobalObject_ThisNotNumber);
    RELEASE_ASSERT_NOT_REACHED();
    return Value();
}

static Value builtinNumberIsFinite(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
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

static Value builtinNumberIsInteger(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
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

static Value builtinNumberIsNaN(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
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

static Value builtinNumberIsSafeInteger(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
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

void GlobalObject::initializeNumber(ExecutionState& state)
{
    ObjectPropertyNativeGetterSetterData* nativeData = new ObjectPropertyNativeGetterSetterData(true, false, true, [](ExecutionState& state, Object* self, const Value& receiver, const EncodedValue& privateDataFromObjectPrivateArea) -> Value {
                                                                                                    ASSERT(self->isGlobalObject());
                                                                                                    return self->asGlobalObject()->number(); }, nullptr);

    defineNativeDataAccessorProperty(state, ObjectPropertyName(state.context()->staticStrings().Number), nativeData, Value(Value::EmptyValue));
}

void GlobalObject::installNumber(ExecutionState& state)
{
    ASSERT(!!m_parseInt && !!m_parseFloat);

    const StaticStrings* strings = &state.context()->staticStrings();
    m_number = new NativeFunctionObject(state, NativeFunctionInfo(strings->Number, builtinNumberConstructor, 1), NativeFunctionObject::__ForBuiltinConstructor__);
    m_number->setGlobalIntrinsicObject(state);

    m_numberPrototype = new NumberObject(state, m_objectPrototype, 0);
    m_numberPrototype->setGlobalIntrinsicObject(state, true);
    m_number->setFunctionPrototype(state, m_numberPrototype);

    m_numberPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->constructor), ObjectPropertyDescriptor(m_number, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_numberPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->toString),
                                               ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->toString, builtinNumberToString, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_numberPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->toLocaleString),
                                               ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->toLocaleString, builtinNumberToLocaleString, 0, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_numberPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->toFixed),
                                               ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->toFixed, builtinNumberToFixed, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_numberPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->toExponential),
                                               ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->toExponential, builtinNumberToExponential, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_numberPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->toPrecision),
                                               ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->toPrecision, builtinNumberToPrecision, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // $20.1.3.26 Number.prototype.valueOf
    m_numberPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->valueOf),
                                               ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->valueOf, builtinNumberValueOf, 0, NativeFunctionInfo::Strict)),
                                                                        (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));


    m_number->setFunctionPrototype(state, m_numberPrototype);

    ObjectPropertyDescriptor::PresentAttribute allFalsePresent = (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::NonWritablePresent
                                                                                                              | ObjectPropertyDescriptor::NonEnumerablePresent
                                                                                                              | ObjectPropertyDescriptor::NonConfigurablePresent);

    // $20.1.2.1 Number.EPSILON
    m_number->directDefineOwnProperty(state, strings->EPSILON, ObjectPropertyDescriptor(Value(UnconvertibleDoubleToInt32(std::numeric_limits<double>::epsilon())), allFalsePresent));
    // $20.1.2.2 Number.isFinite
    m_number->directDefineOwnProperty(state, strings->isFinite,
                                      ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->isFinite, builtinNumberIsFinite, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // $20.1.2.3 Number.isInteger
    m_number->directDefineOwnProperty(state, strings->isInteger,
                                      ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->isInteger, builtinNumberIsInteger, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // $20.1.2.3 Number.isNaN
    m_number->directDefineOwnProperty(state, strings->isNaN,
                                      ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->isNaN, builtinNumberIsNaN, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_number->directDefineOwnProperty(state, strings->parseInt,
                                      ObjectPropertyDescriptor(m_parseInt,
                                                               (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_number->directDefineOwnProperty(state, ObjectPropertyName(strings->parseFloat),
                                      ObjectPropertyDescriptor(m_parseFloat,
                                                               (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // $20.1.2.5 Number.isSafeInteger
    m_number->directDefineOwnProperty(state, strings->isSafeInteger,
                                      ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->isSafeInteger, builtinNumberIsSafeInteger, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // $20.1.2.6 Number.MAX_SAFE_INTEGER
    m_number->directDefineOwnProperty(state, strings->MAX_SAFE_INTEGER, ObjectPropertyDescriptor(Value(UnconvertibleDoubleToInt32(9007199254740991.0)), (ObjectPropertyDescriptor::PresentAttribute)allFalsePresent));
    // $20.1.2.7 Number.MAX_VALUE
    m_number->directDefineOwnProperty(state, strings->MAX_VALUE, ObjectPropertyDescriptor(Value(UnconvertibleDoubleToInt32(1.7976931348623157E+308)), (ObjectPropertyDescriptor::PresentAttribute)allFalsePresent));
    // $20.1.2.8 Number.MIN_SAFE_INTEGER
    m_number->directDefineOwnProperty(state, strings->MIN_SAFE_INTEGER, ObjectPropertyDescriptor(Value(UnconvertibleDoubleToInt32(-9007199254740991.0)), (ObjectPropertyDescriptor::PresentAttribute)allFalsePresent));
    // $20.1.2.9 Number.MIN_VALUE
    m_number->directDefineOwnProperty(state, strings->MIN_VALUE, ObjectPropertyDescriptor(Value(UnconvertibleDoubleToInt32(5E-324)), allFalsePresent));
    // $20.1.2.10 Number.NaN
    m_number->directDefineOwnProperty(state, strings->NaN, ObjectPropertyDescriptor(Value(Value::NanInit), allFalsePresent));
    // $20.1.2.11 Number.NEGATIVE_INFINITY
    m_number->directDefineOwnProperty(state, strings->NEGATIVE_INFINITY, ObjectPropertyDescriptor(Value(Value::NegativeInfinityInit), allFalsePresent));
    // $20.1.2.14 Number.POSITIVE_INFINITY
    m_number->directDefineOwnProperty(state, strings->POSITIVE_INFINITY, ObjectPropertyDescriptor(Value(Value::PostiveInfinityInit), allFalsePresent));

    m_numberProxyObject = new NumberObject(state);

    redefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().Number),
                        ObjectPropertyDescriptor(m_number, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}
} // namespace Escargot
