/*
 *  Copyright (C) 2016 Samsung Electronics Co., Ltd. All Rights Reserved
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2007, 2008, 2012 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#include "Escargot.h"
#include "Value.h"
#include "Context.h"
#include "VMInstance.h"
#include "StaticStrings.h"
#include "Symbol.h"
#include "NumberObject.h"
#include "StringObject.h"
#include "SymbolObject.h"
#include "BooleanObject.h"
#include "ErrorObject.h"

// dtoa
#include "ieee.h"
#include "double-conversion.h"

namespace Escargot {

bool Value::isIterable() const
{
    if (isString())
        return true;

    if (!isObject())
        return false;

    Object* object = asObject();

    if (object->isArrayObject())
        return true;
    if (object->isTypedArrayObject())
        return true;

    return false;
}

bool Value::isConstructor() const
{
    if (!isObject() || !asObject()->isConstructor()) {
        return false;
    }
    return true;
}

String* Value::toStringWithoutException(ExecutionState& ec) const
{
    if (LIKELY(isString())) {
        return asString();
    }

    String* result;
    try {
        result = toStringSlowCase(ec);
    } catch (const Value&) {
        result = String::fromASCII("Error while converting to string, but do not throw an exception");
    }
    return result;
}

Value Value::toPropertyKey(ExecutionState& state) const
{
    // Let key be ? ToPrimitive(argument, hint String).
    Value key = toPrimitive(state, Value::PreferString);
    // If Type(key) is Symbol, then
    if (key.isSymbol()) {
        // Return key.
        return key;
    }
    // Return ! ToString(key).
    return key.toString(state);
}

String* Value::toStringSlowCase(ExecutionState& ec) const // $7.1.12 ToString
{
    ASSERT(!isString());
    if (isInt32()) {
        int num = asInt32();
        if (num >= 0 && num < ESCARGOT_STRINGS_NUMBERS_MAX)
            return ec.context()->staticStrings().numbers[num].string();

        return ec.context()->staticStrings().dtoa(num);
    } else if (isNumber()) {
        double d = asNumber();
        if (std::isnan(d))
            return ec.context()->staticStrings().NaN.string();
        if (std::isinf(d)) {
            if (std::signbit(d))
                return ec.context()->staticStrings().NegativeInfinity.string();
            else
                return ec.context()->staticStrings().Infinity.string();
        }
        // convert -0.0 into 0.0
        // in c++, d = -0.0, d == 0.0 is true
        if (d == 0.0)
            d = 0;

        return ec.context()->staticStrings().dtoa(d);
    } else if (isUndefined()) {
        return ec.context()->staticStrings().undefined.string();
    } else if (isNull()) {
        return ec.context()->staticStrings().null.string();
    } else if (isBoolean()) {
        if (asBoolean())
            return ec.context()->staticStrings().stringTrue.string();
        else
            return ec.context()->staticStrings().stringFalse.string();
    } else if (isSymbol()) {
        ErrorObject::throwBuiltinError(ec, ErrorObject::TypeError, "Cannot convert a Symbol value to a string");
        ASSERT_NOT_REACHED();
    } else {
        return toPrimitive(ec, PreferString).toString(ec);
    }
    return nullptr;
}

Object* Value::toObjectSlowCase(ExecutionState& state) const // $7.1.13 ToObject
{
    Object* object = nullptr;
    if (isNumber()) {
        object = new NumberObject(state, toNumber(state));
    } else if (isBoolean()) {
        object = new BooleanObject(state, toBoolean(state));
    } else if (isString()) {
        object = new StringObject(state, toString(state));
    } else if (isSymbol()) {
        object = new SymbolObject(state, asSymbol());
    } else if (isNull()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::Code::TypeError, errorMessage_NullToObject);
    } else {
        ASSERT(isUndefined());
        ErrorObject::throwBuiltinError(state, ErrorObject::Code::TypeError, errorMessage_UndefinedToObject);
    }
    return object;
}

// https://www.ecma-international.org/ecma-262/6.0/#sec-toprimitive
Value Value::ordinaryToPrimitive(ExecutionState& state, PrimitiveTypeHint preferredType) const
{
    // Assert: Type(O) is Object
    // Assert: Type(hint) is String and its value is either "string" or "number".
    ASSERT(isObject());
    ASSERT(preferredType == PreferString || preferredType == PreferNumber);
    const StaticStrings& strings = state.context()->staticStrings();
    Object* input = asObject();

    AtomicString methodName1;
    AtomicString methodName2;
    // If hint is "string", then
    if (preferredType == PreferString) {
        // Let methodNames be «"toString", "valueOf"».
        methodName1 = strings.toString;
        methodName2 = strings.valueOf;
        // Else
    } else { // preferNumber
        // Let methodNames be «"valueOf", "toString"».
        methodName1 = strings.valueOf;
        methodName2 = strings.toString;
    }

    // For each name in methodNames in List order, do
    // Let method be Get(O, name).
    // If IsCallable(method) is true, then
    // Let result be Call(method, O).
    // If Type(result) is not Object, return result.
    Value method1 = input->get(state, ObjectPropertyName(methodName1)).value(state, input);
    if (method1.isCallable()) {
        Value result = Object::call(state, method1, input, 0, nullptr);
        if (!result.isObject()) {
            return result;
        }
    }
    Value method2 = input->get(state, ObjectPropertyName(methodName2)).value(state, input);
    if (method2.isCallable()) {
        Value result = Object::call(state, method2, input, 0, nullptr);
        if (!result.isObject()) {
            return result;
        }
    }

    // Throw a TypeError exception.
    ErrorObject::throwBuiltinError(state, ErrorObject::Code::TypeError, errorMessage_ObjectToPrimitiveValue);
    RELEASE_ASSERT_NOT_REACHED();
}

// https://www.ecma-international.org/ecma-262/6.0/#sec-toprimitive
Value Value::toPrimitiveSlowCase(ExecutionState& state, PrimitiveTypeHint preferredType) const
{
    ASSERT(isObject());
    const StaticStrings& strings = state.context()->staticStrings();
    Object* input = asObject();

    // Let exoticToPrim be GetMethod(input, @@toPrimitive).
    Value exoticToPrim = Object::getMethod(state, input, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().toPrimitive));
    // If exoticToPrim is not undefined, then
    if (!exoticToPrim.isUndefined()) {
        Value hint;
        if (preferredType == PreferNumber) {
            hint = strings.number.string();
        } else if (preferredType == PreferString) {
            hint = strings.string.string();
        } else {
            hint = strings.stringDefault.string();
        }
        // Let result be Call(exoticToPrim, input, «hint»).
        Value result = Object::call(state, exoticToPrim, input, 1, &hint);
        // If Type(result) is not Object, return result.
        if (!result.isObject()) {
            return result;
        }
        // Throw a TypeError exception.
        ErrorObject::throwBuiltinError(state, ErrorObject::Code::TypeError, errorMessage_ObjectToPrimitiveValue);
    }
    // If hint is "default", let hint be "number".
    if (preferredType == PreferDefault) {
        preferredType = PreferNumber;
    }

    return ordinaryToPrimitive(state, preferredType);
}

bool Value::abstractEqualsToSlowCase(ExecutionState& state, const Value& val) const
{
    bool selfIsNumber = isNumber();
    bool valIsNumber = val.isNumber();

    if (selfIsNumber && valIsNumber) {
        double a = asNumber();
        double b = val.asNumber();

        if (std::isnan(a) || std::isnan(b))
            return false;
        else if (a == b)
            return true;

        return false;
    } else {
        bool selfIsUndefinedOrNull = isUndefinedOrNull();
        bool valIsUndefinedOrNull = val.isUndefinedOrNull();
        if (selfIsUndefinedOrNull && valIsUndefinedOrNull)
            return true;

        bool selfIsPointerValue = isPointerValue();
        bool valIsPointerValue = val.isPointerValue();

        bool valIsString = valIsPointerValue ? val.asPointerValue()->isString() : false;
        bool selfIsString = selfIsPointerValue ? asPointerValue()->isString() : false;

        if (selfIsNumber && valIsString) {
            // If Type(x) is Number and Type(y) is String,
            // return the result of the comparison x == ToNumber(y).
            return asNumber() == val.toNumber(state);
        } else if (selfIsString && valIsNumber) {
            // If Type(x) is String and Type(y) is Number,
            // return the result of the comparison ToNumber(x) == y.
            return val.asNumber() == toNumber(state);
        } else if (isBoolean()) {
            // If Type(x) is Boolean, return the result of the comparison ToNumber(x) == y.
            // return the result of the comparison ToNumber(x) == y.
            Value x(toNumber(state));
            return x.abstractEqualsTo(state, val);
        } else if (val.isBoolean()) {
            // If Type(y) is Boolean, return the result of the comparison x == ToNumber(y).
            // return the result of the comparison ToNumber(x) == y.
            return abstractEqualsTo(state, Value(val.toNumber(state)));
        } else if ((selfIsString || selfIsNumber || isSymbol()) && (valIsPointerValue && val.asPointerValue()->isObject())) {
            // If Type(x) is either String, Number, or Symbol and Type(y) is Object, then
            return abstractEqualsTo(state, val.toPrimitive(state));
        } else if ((selfIsPointerValue && asPointerValue()->isObject() && !selfIsString) && (valIsString || valIsNumber || val.isSymbol())) {
            // If Type(x) is Object and Type(y) is either String, Number, or Symbol, then
            return toPrimitive(state).abstractEqualsTo(state, val);
        }

        if (selfIsPointerValue && valIsPointerValue) {
            PointerValue* o = asPointerValue();
            PointerValue* comp = val.asPointerValue();

            if (o->isString() && comp->isString())
                return o->asString()->equals(comp->asString());
            return equalsTo(state, val);
        }
    }
    return false;
}

bool Value::equalsToSlowCase(ExecutionState& state, const Value& val) const
{
    if (isUndefined())
        return val.isUndefined();

    if (isNull())
        return val.isNull();

    if (isBoolean())
        return val.isBoolean() && asBoolean() == val.asBoolean();

    if (isNumber()) {
        if (!val.isNumber())
            return false;
        double a = asNumber();
        double b = val.asNumber();
        if (std::isnan(a) || std::isnan(b))
            return false;
        // we can pass [If x is +0 and y is −0, return true. If x is −0 and y is +0, return true.]
        // because
        // double a = -0.0;
        // double b = 0.0;
        // a == b; is true
        return a == b;
    }

    if (isPointerValue()) {
        PointerValue* o = asPointerValue();
        if (!val.isPointerValue())
            return false;
        PointerValue* o2 = val.asPointerValue();
        if (o->isString()) {
            if (!o2->isString())
                return false;
            return o->asString()->equals(o2->asString());
        }
        return o == o2;
    }
    return false;
}

bool Value::equalsToByTheSameValueAlgorithm(ExecutionState& ec, const Value& val) const
{
    if (isUndefined()) {
        return val.isUndefined();
    }

    if (isNull()) {
        return val.isNull();
    }

    if (isBoolean()) {
        return val.isBoolean() && asBoolean() == val.asBoolean();
    }

    if (isNumber()) {
        if (!val.isNumber()) {
            return false;
        }
        double a = asNumber();
        double b = val.asNumber();
        if (std::isnan(a) && std::isnan(b)) {
            return true;
        }
        if (std::isnan(a) || std::isnan(b)) {
            return false;
        }
        // we can pass [If x is +0 and y is −0, return true. If x is −0 and y is +0, return true.]
        // because
        // double a = -0.0;
        // double b = 0.0;
        // a == b; is true
        return a == b && std::signbit(a) == std::signbit(b);
    }

    if (isPointerValue()) {
        PointerValue* o = asPointerValue();
        if (!val.isPointerValue()) {
            return false;
        }
        PointerValue* o2 = val.asPointerValue();
        if (o->isString()) {
            if (!o2->isString()) {
                return false;
            }
            return *o->asString() == *o2->asString();
        }
        if (o->isSymbol()) {
            if (!o2->isSymbol()) {
                return false;
            }
        }
        return o == o2;
    }
    return false;
}

bool Value::equalsToByTheSameValueZeroAlgorithm(ExecutionState& ec, const Value& val) const
{
    if (isUndefined()) {
        return val.isUndefined();
    }

    if (isNull()) {
        return val.isNull();
    }

    if (isBoolean()) {
        return val.isBoolean() && asBoolean() == val.asBoolean();
    }

    if (isNumber()) {
        if (!val.isNumber()) {
            return false;
        }
        double a = asNumber();
        double b = val.asNumber();
        if (std::isnan(a) && std::isnan(b)) {
            return true;
        }
        if (std::isnan(a) || std::isnan(b)) {
            return false;
        }
        return a == b;
    }

    if (isPointerValue()) {
        PointerValue* o = asPointerValue();
        if (!val.isPointerValue()) {
            return false;
        }
        PointerValue* o2 = val.asPointerValue();
        if (o->isString()) {
            if (!o2->isString()) {
                return false;
            }
            return *o->asString() == *o2->asString();
        }
        if (o->isSymbol()) {
            if (!o2->isSymbol()) {
                return false;
            }
        }
        return o == o2;
    }
    return false;
}

// http://www.ecma-international.org/ecma-262/6.0/index.html#sec-instanceofoperator
bool Value::instanceOf(ExecutionState& state, const Value& other) const
{
    // If Type(C) is not Object, throw a TypeError exception.
    if (!other.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, errorMessage_InstanceOf_NotFunction);
    }
    Object* C = other.asObject();
    // Let instOfHandler be GetMethod(C,@@hasInstance).
    Value instOfHandler = Object::getMethod(state, other, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().hasInstance));
    // If instOfHandler is not undefined, then
    if (!instOfHandler.isUndefined()) {
        // Return ToBoolean(Call(instOfHandler, C, «O»)).
        Value arg[1] = { *this };
        return Object::call(state, instOfHandler, Value(C), 1, arg).toBoolean(state);
    }

    // If IsCallable(C) is false, throw a TypeError exception.
    if (!C->isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, errorMessage_InstanceOf_NotFunction);
    }
    // Return OrdinaryHasInstance(C, O).
    return C->hasInstance(state, *this);
}

// https://www.ecma-international.org/ecma-262/6.0/#sec-tonumber
double Value::toNumberSlowCase(ExecutionState& state) const
{
    ASSERT(isPointerValue());
    PointerValue* o = asPointerValue();
    bool isString = o->isString();
    if (isString || o->isStringObject()) {
        double val;
        String* data;
        if (LIKELY(isString)) {
            data = o->asString();
        } else {
            ASSERT(o->isStringObject());
            data = o->asStringObject()->primitiveValue();
        }

        const auto& bufferAccessData = data->bufferAccessData();

        const size_t len = bufferAccessData.length;

        // A StringNumericLiteral that is empty or contains only white space is converted to +0.
        if (len == 0)
            return 0;

        int end;
        char* buf;

        buf = ALLOCA(len + 1, char, state);
        if (LIKELY(bufferAccessData.has8BitContent)) {
            const LChar* src = (const LChar*)bufferAccessData.buffer;
            for (unsigned i = 0; i < len; i++) {
                LChar c = src[i];
                if (UNLIKELY(c >= 128))
                    c = 0;
                buf[i] = c;
            }
        } else {
            const char16_t* src = (const char16_t*)bufferAccessData.buffer;
            for (unsigned i = 0; i < len; i++) {
                char16_t c = src[i];
                if (c >= 128)
                    c = 0;
                buf[i] = c;
            }
        }

        buf[len] = 0;
        if (UNLIKELY(len >= 3 && (!strncmp("-0x", buf, 3) || !strncmp("+0x", buf, 3)))) { // hex number with Unary Plus or Minus is invalid in JavaScript while it is valid in C
            val = std::numeric_limits<double>::quiet_NaN();
            return val;
        }
        double_conversion::StringToDoubleConverter converter(double_conversion::StringToDoubleConverter::ALLOW_HEX
                                                                 | double_conversion::StringToDoubleConverter::ALLOW_LEADING_SPACES
                                                                 | double_conversion::StringToDoubleConverter::ALLOW_TRAILING_SPACES,
                                                             0.0, double_conversion::Double::NaN(),
                                                             "Infinity", "NaN");
        val = converter.StringToDouble(buf, len, &end);
        if (static_cast<size_t>(end) != len) {
            auto isSpace = [](char16_t c) -> bool {
                switch (c) {
                case 0x0009:
                case 0x000A:
                case 0x000B:
                case 0x000C:
                case 0x000D:
                case 0x0020:
                case 0x00A0:
                case 0x1680:
                case 0x180E:
                case 0x2000:
                case 0x2001:
                case 0x2002:
                case 0x2003:
                case 0x2004:
                case 0x2005:
                case 0x2006:
                case 0x2007:
                case 0x2008:
                case 0x2009:
                case 0x200A:
                case 0x2028:
                case 0x2029:
                case 0x202F:
                case 0x205F:
                case 0x3000:
                case 0xFEFF:
                    return true;
                default:
                    return false;
                }
            };

            unsigned ptr = 0;
            enum State { Initial,
                         ReadingNumber,
                         DoneReadingNumber,
                         Invalid };
            State state = State::Initial;
            for (unsigned i = 0; i < len; i++) {
                char16_t ch;
                if (LIKELY(bufferAccessData.has8BitContent)) {
                    ch = ((LChar*)bufferAccessData.buffer)[i];
                } else {
                    ch = ((char16_t*)bufferAccessData.buffer)[i];
                }
                switch (state) {
                case Initial:
                    if (isSpace(ch))
                        break;
                    else
                        state = State::ReadingNumber;
                // Fall through.
                case ReadingNumber:
                    if (isSpace(ch))
                        state = State::DoneReadingNumber;
                    else {
                        char16_t c = ch;
                        if (c >= 128)
                            c = 0;
                        buf[ptr++] = c;
                    }
                    break;
                case DoneReadingNumber:
                    if (!isSpace(ch))
                        state = State::Invalid;
                    break;
                case Invalid:
                    break;
                }
            }
            if (state != State::Invalid) {
                buf[ptr] = 0;
                val = converter.StringToDouble(buf, ptr, &end);
                if (static_cast<size_t>(end) != ptr) {
                    bool ok = true;
                    uint64_t number = 0;
                    if (ptr >= 3 && buf[0] == '0' && (buf[1] == 'b' || buf[1] == 'B')) {
                        // scan binary literal
                        unsigned scan = 2;

                        while (scan != ptr) {
                            char ch = buf[scan];
                            if (ch != '0' && ch != '1') {
                                ok = false;
                                break;
                            }
                            number = (number << 1) + ch - '0';
                            scan++;
                        }
                    } else if (ptr >= 3 && buf[0] == '0' && (buf[1] == 'o' || buf[1] == 'O')) {
                        // scan octal literal
                        unsigned scan = 2;

                        while (scan != ptr) {
                            char ch = buf[scan];
                            if (!(ch >= '0' && ch <= '7')) {
                                ok = false;
                                break;
                            } else {
                                number = (number << 3) + ch - '0';
                                scan++;
                            }
                        }
                    } else {
                        ok = false;
                    }

                    if (ok) {
                        val = number;
                    } else {
                        val = std::numeric_limits<double>::quiet_NaN();
                    }
                }
            } else {
                val = std::numeric_limits<double>::quiet_NaN();
            }
        }
        return val;
    } else if (isSymbol()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Cannot convert a Symbol value to a number");
        ASSERT_NOT_REACHED();
    } else {
        // Let primValue be ToPrimitive(argument, hint Number).
        // Return ToNumber(primValue).
        return toPrimitive(state, PreferNumber).toNumber(state);
    }
    return 0;
}

int32_t Value::toInt32SlowCase(ExecutionState& state) const // $7.1.5 ToInt32
{
    double num = toNumber(state);
    int64_t bits = bitwise_cast<int64_t>(num);
    int32_t exp = (static_cast<int32_t>(bits >> 52) & 0x7ff) - 0x3ff;

    // If exponent < 0 there will be no bits to the left of the decimal point
    // after rounding; if the exponent is > 83 then no bits of precision can be
    // left in the low 32-bit range of the result (IEEE-754 doubles have 52 bits
    // of fractional precision).
    // Note this case handles 0, -0, and all infinte, NaN, & denormal value.
    if (exp < 0 || exp > 83)
        return 0;

    // Select the appropriate 32-bits from the floating point mantissa. If the
    // exponent is 52 then the bits we need to select are already aligned to the
    // lowest bits of the 64-bit integer representation of tghe number, no need
    // to shift. If the exponent is greater than 52 we need to shift the value
    // left by (exp - 52), if the value is less than 52 we need to shift right
    // accordingly.
    int32_t result = (exp > 52)
        ? static_cast<int32_t>(bits << (exp - 52))
        : static_cast<int32_t>(bits >> (52 - exp));

    // IEEE-754 double precision values are stored omitting an implicit 1 before
    // the decimal point; we need to reinsert this now. We may also the shifted
    // invalid bits into the result that are not a part of the mantissa (the sign
    // and exponent bits from the floatingpoint representation); mask these out.
    if (exp < 32) {
        int32_t missingOne = 1 << exp;
        result &= missingOne - 1;
        result += missingOne;
    }

    // If the input value was negative (we could test either 'number' or 'bits',
    // but testing 'bits' is likely faster) invert the result appropriately.
    return bits < 0 ? -result : result;
}

Value::ValueIndex Value::tryToUseAsIndexSlowCase(ExecutionState& ec) const
{
    if (isSymbol()) {
        return Value::InvalidIndexValue;
    }
    return toString(ec)->tryToUseAsIndex();
}

uint32_t Value::tryToUseAsArrayIndexSlowCase(ExecutionState& ec) const
{
    if (isSymbol()) {
        return Value::InvalidArrayIndexValue;
    }
    return toString(ec)->tryToUseAsArrayIndex();
}
}
