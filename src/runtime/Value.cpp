/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
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

#if defined(ESCARGOT_ENABLE_PROMISE) || defined(ESCARGOT_ENABLE_TYPEDARRAY)
bool Value::isIterable() const
{
    if (isString())
        return true;

    if (!isObject())
        return false;

    Object* object = asObject();

    if (object->isArrayObject())
        return true;
#ifdef ESCARGOT_ENABLE_TYPEDARRAY
    if (object->isTypedArrayObject())
        return true;
#endif

    return false;
}
#endif

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

Value Value::ordinaryToPrimitive(ExecutionState& state, PrimitiveTypeHint preferredType) const
{
    ASSERT(!isPrimitive());
    Object* obj = asObject();
    if (preferredType == PrimitiveTypeHint::PreferString) {
        Value toString = obj->get(state, ObjectPropertyName(state.context()->staticStrings().toString)).value(state, obj);
        if (toString.isFunction()) {
            Value ret = toString.asFunction()->call(state, obj, 0, NULL);
            if (ret.isPrimitive()) {
                return ret;
            }
        }

        Value valueOf = obj->get(state, ObjectPropertyName(state.context()->staticStrings().valueOf)).value(state, obj);
        if (valueOf.isFunction()) {
            Value ret = valueOf.asFunction()->call(state, obj, 0, NULL);
            if (ret.isPrimitive()) {
                return ret;
            }
        }
    } else { // preferNumber
        Value valueOf = obj->get(state, ObjectPropertyName(state.context()->staticStrings().valueOf)).value(state, obj);
        if (valueOf.isFunction()) {
            Value ret = valueOf.asFunction()->call(state, obj, 0, NULL);
            if (ret.isPrimitive()) {
                return ret;
            }
        }

        Value toString = obj->get(state, ObjectPropertyName(state.context()->staticStrings().toString)).value(state, obj);
        if (toString.isFunction()) {
            Value ret = toString.asFunction()->call(state, obj, 0, NULL);
            if (ret.isPrimitive()) {
                return ret;
            }
        }
    }

    ErrorObject::throwBuiltinError(state, ErrorObject::Code::TypeError, errorMessage_ObjectToPrimitiveValue);
    RELEASE_ASSERT_NOT_REACHED();
}

Value Value::toPrimitiveSlowCase(ExecutionState& state, PrimitiveTypeHint preferredType) const // $7.1.1 ToPrimitive
{
    // http://www.ecma-international.org/ecma-262/7.0/index.html#sec-toprimitive
    // If PreferredType was not passed, let hint be "default".
    // Else if PreferredType is hint String, let hint be "string".
    // Else PreferredType is hint Number, let hint be "number".
    ASSERT(!isPrimitive());
    Object* obj = asObject();

    // Let exoticToPrim be ? GetMethod(input, @@toPrimitive).
    Value exoticToPrim = obj->get(state, ObjectPropertyName(state, state.context()->vmInstance()->globalSymbols().toPrimitive)).value(state, obj);
    // If exoticToPrim is not undefined, then
    if (!exoticToPrim.isUndefined()) {
        // Let result be ? Call(exoticToPrim, input, « hint »).
        Value hint;
        if (preferredType == PreferNumber) {
            hint = state.context()->staticStrings().number.string();
        } else if (preferredType == PreferString) {
            hint = state.context()->staticStrings().string.string();
        } else {
            hint = state.context()->staticStrings().stringDefault.string();
        }
        Value result = FunctionObject::call(state, exoticToPrim, obj, 1, &hint);
        // If Type(result) is not Object, return result.
        if (!result.isObject()) {
            return result;
        }
        // Throw a TypeError exception.
        ErrorObject::throwBuiltinError(state, ErrorObject::Code::TypeError, errorMessage_ObjectToPrimitiveValue);
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
            if (val.asPointerValue()->isDateObject())
                return abstractEqualsTo(state, val.toPrimitive(state, Value::PreferString));
            else
                return abstractEqualsTo(state, val.toPrimitive(state));
        } else if ((selfIsPointerValue && asPointerValue()->isObject() && !selfIsString) && (valIsString || valIsNumber || val.isSymbol())) {
            // If Type(x) is Object and Type(y) is either String, Number, or Symbol, then
            if (asPointerValue()->isDateObject())
                return toPrimitive(state, Value::PreferString).abstractEqualsTo(state, val);
            else
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
        if (std::isnan(a) && std::isnan(b))
            return true;
        if (std::isnan(a) || std::isnan(b))
            return false;
        // we can pass [If x is +0 and y is −0, return true. If x is −0 and y is +0, return true.]
        // because
        // double a = -0.0;
        // double b = 0.0;
        // a == b; is true
        return a == b && std::signbit(a) == std::signbit(b);
    }

    if (isPointerValue()) {
        PointerValue* o = asPointerValue();
        if (!val.isPointerValue())
            return false;
        PointerValue* o2 = val.asPointerValue();
        if (o->isString()) {
            if (!o2->isString())
                return false;
            return *o->asString() == *o2->asString();
        }
        if (o == o2)
            return o == o2;
    }
    return false;
}

bool Value::equalsToByTheSameValueZeroAlgorithm(ExecutionState& ec, const Value& val) const
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
        if (std::isnan(a) && std::isnan(b))
            return true;
        if (std::isnan(a) || std::isnan(b))
            return false;
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
            return *o->asString() == *o2->asString();
        }
        if (o == o2)
            return o == o2;
    }
    return false;
}

double Value::toNumberSlowCase(ExecutionState& state) const // $7.1.3 ToNumber
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

        auto bufferAccessData = data->bufferAccessData();

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
                if (static_cast<size_t>(end) != ptr)
                    val = std::numeric_limits<double>::quiet_NaN();
            } else
                val = std::numeric_limits<double>::quiet_NaN();
        }
        return val;
    } else if (isSymbol()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Cannot convert a Symbol value to a number");
        ASSERT_NOT_REACHED();
    } else {
        return toPrimitive(state).toNumber(state);
    }
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
