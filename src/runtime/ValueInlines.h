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

#ifndef __EscargotValueInlines__
#define __EscargotValueInlines__

#include "runtime/Object.h"
#include "runtime/String.h"
#include "runtime/BigInt.h"

namespace Escargot {

// The fast double-to-(unsigned-)int conversion routine does not guarantee
// rounding towards zero.
// The result is unspecified if x is infinite or NaN, or if the rounded
// integer value is outside the range of type int.
inline int FastD2I(double x)
{
    return static_cast<int32_t>(x);
}


inline double FastI2D(int x)
{
    // There is no rounding involved in converting an integer to a
    // double, so this code should compile to a few instructions without
    // any FPU pipeline stalls.
    return static_cast<double>(x);
}

constexpr double Value::maximumLength()
{
    // 2^53 - 1
    return 9007199254740991.0;
}

// ==============================================================================
// ===32-bit architecture========================================================
// ==============================================================================

#ifdef ESCARGOT_32

inline Value::Value(ForceUninitializedTag)
{
}

inline Value::Value()
{
    u.asBits.tag = UndefinedTag;
    u.asBits.payload = 0;
}

inline Value::Value(NullInitTag)
{
    u.asBits.tag = NullTag;
    u.asBits.payload = 0;
}

inline Value::Value(UndefinedInitTag)
{
    u.asBits.tag = UndefinedTag;
    u.asBits.payload = 0;
}

inline Value::Value(EmptyValueInitTag)
{
    u.asBits.tag = EmptyValueTag;
    u.asBits.payload = 0;
}

inline Value::Value(TrueInitTag)
{
    u.asBits.tag = BooleanTrueTag;
    u.asBits.payload = 0;
}

inline Value::Value(FalseInitTag)
{
    u.asBits.tag = BooleanFalseTag;
    u.asBits.payload = 0;
}

inline Value::Value(bool b)
{
    u.asBits.tag = BooleanFalseTag ^ (((uint32_t)b) << TagTypeShift);
    u.asBits.payload = 0;
}

inline Value::Value(FromPayloadTag, intptr_t ptr)
{
    u.asBits.tag = OtherPointerTag;
#if defined(COMPILER_MSVC)
    u.asBits.payload = (int32_t)(ptr);
#else
    u.asBits.payload = reinterpret_cast<int32_t>(ptr);
#endif
}

inline Value::Value(PointerValue* ptr)
{
    // other type of PointerValue(Object) has pointer in first data area
    if (ptr->getTypeTag() & (POINTER_VALUE_NOT_OBJECT_TAG_IN_DATA)) {
        u.asBits.tag = OtherPointerTag;
    } else {
        u.asBits.tag = ObjectPointerTag;
    }
    u.asBits.payload = reinterpret_cast<int32_t>(const_cast<PointerValue*>(ptr));
}

inline Value::Value(const PointerValue* ptr)
{
    if (ptr->isObject()) {
        u.asBits.tag = ObjectPointerTag;
    } else {
        u.asBits.tag = OtherPointerTag;
    }
    u.asBits.payload = reinterpret_cast<int32_t>(const_cast<PointerValue*>(ptr));
}

inline Value::Value(const PointerValue* ptr, FromNonObjectPointerTag)
{
    ASSERT(!ptr->isObject());
    u.asBits.tag = OtherPointerTag;
    u.asBits.payload = reinterpret_cast<int32_t>(const_cast<PointerValue*>(ptr));
}

inline Value::Value(String* ptr)
{
    ASSERT(ptr != NULL);
    u.asBits.tag = OtherPointerTag;
    u.asBits.payload = reinterpret_cast<int32_t>(ptr);
}

inline Value::Value(const String* ptr)
{
    ASSERT(ptr != NULL);
    u.asBits.tag = OtherPointerTag;
    u.asBits.payload = reinterpret_cast<int32_t>(const_cast<String*>(ptr));
}

inline Value::Value(Object* ptr)
{
    u.asBits.tag = ObjectPointerTag;
    u.asBits.payload = reinterpret_cast<int32_t>(const_cast<Object*>(ptr));
}

inline Value::Value(const Object* ptr)
{
    u.asBits.tag = ObjectPointerTag;
    u.asBits.payload = reinterpret_cast<int32_t>(const_cast<Object*>(ptr));
}

inline Value::Value(FromTagTag, uint32_t tag)
{
    ASSERT(tag == BooleanFalseTag || tag == BooleanTrueTag || tag == NullTag
           || tag == UndefinedTag || tag == EmptyValueTag);

    u.asBits.tag = tag;
    u.asBits.payload = 0;
}

inline Value::Value(EncodeAsDoubleTag, const double& d)
{
    u.asDouble = d;
}

inline Value::Value(int i)
{
    u.asBits.tag = Int32Tag;
    u.asBits.payload = i;
}

inline bool Value::operator==(const Value& other) const
{
    return u.asInt64 == other.u.asInt64;
}

inline bool Value::operator!=(const Value& other) const
{
    return u.asInt64 != other.u.asInt64;
}

inline uint32_t Value::tag() const
{
    return u.asBits.tag;
}

inline int32_t Value::payload() const
{
    return u.asBits.payload;
}

ALWAYS_INLINE bool Value::isInt32() const
{
    return tag() == Int32Tag;
}

ALWAYS_INLINE bool Value::isDouble() const
{
    return tag() < LowestTag;
}

inline int32_t Value::asInt32() const
{
    ASSERT(isInt32());
    return u.asBits.payload;
}

inline bool Value::asBoolean() const
{
    ASSERT(isBoolean());
    return u.asBits.tag == BooleanTrueTag;
}

inline double Value::asDouble() const
{
    ASSERT(isDouble());
    return u.asDouble;
}

inline bool Value::isEmpty() const
{
    return tag() == EmptyValueTag;
}

ALWAYS_INLINE bool Value::isNumber() const
{
    return isInt32() || isDouble();
}

inline bool Value::isPointerValue() const
{
    return (tag() == ObjectPointerTag) || (tag() == OtherPointerTag);
}

inline bool Value::isUndefined() const
{
    return tag() == UndefinedTag;
}

inline bool Value::isNull() const
{
    return tag() == NullTag;
}

inline bool Value::isBoolean() const
{
    /* BooleanTrueTag and BooleanFalseTag are inverted values
       of ValueTrue and ValueFalse respectively. */
    COMPILE_ASSERT(BooleanFalseTag == (BooleanTrueTag | (1 << TagTypeShift)), "");

    return (tag() | (1 << TagTypeShift)) == BooleanFalseTag;
}

inline bool Value::isTrue() const
{
    return tag() == BooleanTrueTag;
}

inline bool Value::isFalse() const
{
    return tag() == BooleanFalseTag;
}

inline PointerValue* Value::asPointerValue() const
{
    ASSERT(isPointerValue());
    return reinterpret_cast<PointerValue*>(u.asBits.payload);
}

inline bool Value::isString() const
{
    return tag() == OtherPointerTag && asPointerValue()->isString();
}

inline bool Value::isSymbol() const
{
    return tag() == OtherPointerTag && asPointerValue()->isSymbol();
}

inline bool Value::isBigInt() const
{
    return tag() == OtherPointerTag && asPointerValue()->isBigInt();
}

inline String* Value::asString() const
{
    ASSERT(isString());
    return asPointerValue()->asString();
}

inline Symbol* Value::asSymbol() const
{
    ASSERT(isSymbol());
    return asPointerValue()->asSymbol();
}

inline BigInt* Value::asBigInt() const
{
    ASSERT(isBigInt());
    return asPointerValue()->asBigInt();
}

inline bool Value::isObject() const
{
    return tag() == ObjectPointerTag;
}

inline Object* Value::asObject() const
{
    return asPointerValue()->asObject();
}

inline bool Value::isFunction() const
{
    return isObject() && asPointerValue()->isFunctionObject();
}

inline bool Value::isExtendedNativeFunctionObject() const
{
    return isObject() && asPointerValue()->isExtendedNativeFunctionObject();
}

inline FunctionObject* Value::asFunction() const
{
    return asPointerValue()->asFunctionObject();
}

#else

// ==============================================================================
// ===64-bit architecture========================================================
// ==============================================================================


inline Value::Value()
{
    u.asInt64 = ValueUndefined;
}

inline Value::Value(ForceUninitializedTag)
{
}

inline Value::Value(NullInitTag)
{
    u.asInt64 = ValueNull;
}

inline Value::Value(UndefinedInitTag)
{
    u.asInt64 = ValueUndefined;
}

inline Value::Value(EmptyValueInitTag)
{
    u.asInt64 = ValueEmpty;
}

inline Value::Value(TrueInitTag)
{
    u.asInt64 = ValueTrue;
}

inline Value::Value(FalseInitTag)
{
    u.asInt64 = ValueFalse;
}

inline Value::Value(FromPayloadTag, intptr_t ptr)
{
    u.ptr = (PointerValue*)ptr;
}

inline Value::Value(bool b)
{
    u.asInt64 = (TagBitTypeOther | (b << TagTypeShift));
}

inline Value::Value(PointerValue* ptr)
{
    u.ptr = ptr;
}

inline Value::Value(const PointerValue* ptr)
{
    u.ptr = const_cast<PointerValue*>(ptr);
}

inline int64_t reinterpretDoubleToInt64(double value)
{
    return bitwise_cast<int64_t>(value);
}
inline double reinterpretInt64ToDouble(int64_t value)
{
    return bitwise_cast<double>(value);
}

inline Value::Value(EncodeAsDoubleTag, const double& d)
{
    u.asInt64 = reinterpretDoubleToInt64(d) + DoubleEncodeOffset;
}

inline Value::Value(int i)
{
    u.asInt64 = TagTypeNumber | static_cast<uint32_t>(i);
}

inline bool Value::operator==(const Value& other) const
{
    return u.asInt64 == other.u.asInt64;
}

inline bool Value::operator!=(const Value& other) const
{
    return u.asInt64 != other.u.asInt64;
}

ALWAYS_INLINE bool Value::isInt32() const
{
#ifdef ESCARGOT_LITTLE_ENDIAN
    ASSERT(sizeof(short) == 2);
    unsigned short* firstByte = (unsigned short*)&u.asInt64;
    return firstByte[3] == 0xffff;
#else
    return (u.asInt64 & TagTypeNumber) == TagTypeNumber;
#endif
}

inline bool Value::isDouble() const
{
    return isNumber() && !isInt32();
}

inline int32_t Value::asInt32() const
{
    ASSERT(isInt32());
    return static_cast<int32_t>(u.asInt64);
}

inline bool Value::asBoolean() const
{
    ASSERT(isBoolean());
    return u.asInt64 == ValueTrue;
}

inline double Value::asDouble() const
{
    ASSERT(isDouble());
    return reinterpretInt64ToDouble(u.asInt64 - DoubleEncodeOffset);
}

inline bool Value::isEmpty() const
{
    return u.asInt64 == ValueEmpty;
}

ALWAYS_INLINE bool Value::isNumber() const
{
#ifdef ESCARGOT_LITTLE_ENDIAN
    ASSERT(sizeof(short) == 2);
    unsigned short* firstByte = (unsigned short*)&u.asInt64;
    return firstByte[3];
#else
    return u.asInt64 & TagTypeNumber;
#endif
}

inline bool Value::isString() const
{
    return isPointerValue() && asPointerValue()->isString();
}

inline bool Value::isSymbol() const
{
    return isPointerValue() && asPointerValue()->isSymbol();
}

inline bool Value::isBigInt() const
{
    return isPointerValue() && asPointerValue()->isBigInt();
}

inline String* Value::asString() const
{
    ASSERT(isString());
    return asPointerValue()->asString();
}

inline Symbol* Value::asSymbol() const
{
    ASSERT(isSymbol());
    return asPointerValue()->asSymbol();
}

inline BigInt* Value::asBigInt() const
{
    ASSERT(isBigInt());
    return asPointerValue()->asBigInt();
}

inline bool Value::isPointerValue() const
{
    return !(u.asInt64 & TagMask);
}

inline bool Value::isUndefined() const
{
    return u.asInt64 == ValueUndefined;
}

inline bool Value::isNull() const
{
    return u.asInt64 == ValueNull;
}

inline bool Value::isBoolean() const
{
    COMPILE_ASSERT(ValueTrue == (ValueFalse | (1 << TagTypeShift)), "");

    return (u.asInt64 | (1 << TagTypeShift)) == ValueTrue;
}

inline bool Value::isTrue() const
{
    return u.asInt64 == ValueTrue;
}

inline bool Value::isFalse() const
{
    return u.asInt64 == ValueFalse;
}

inline PointerValue* Value::asPointerValue() const
{
    ASSERT(isPointerValue());
    return u.ptr;
}

inline bool Value::isObject() const
{
    return isPointerValue() && asPointerValue()->isObject();
}

inline Object* Value::asObject() const
{
    return asPointerValue()->asObject();
}

inline bool Value::isFunction() const
{
    return isPointerValue() && asPointerValue()->isFunctionObject();
}

inline FunctionObject* Value::asFunction() const
{
    return asPointerValue()->asFunctionObject();
}

inline ExtendedNativeFunctionObject* Value::asExtendedNativeFunctionObject() const
{
    return asPointerValue()->asExtendedNativeFunctionObject();
}

inline intptr_t Value::payload() const
{
    return u.asInt64;
}

#endif

// ==============================================================================
// ===common architecture========================================================
// ==============================================================================

ALWAYS_INLINE bool Value::isInt32ConvertibleDouble(const double& d)
{
    int32_t asInt32 = static_cast<int32_t>(d);
    if (LIKELY(LIKELY(asInt32 != d) || UNLIKELY(!asInt32 && std::signbit(d)))) { // true for -0.0
        return false;
    }
    return true;
}

ALWAYS_INLINE bool Value::isInt32ConvertibleDouble(const double& d, int32_t& asInt32)
{
    asInt32 = static_cast<int32_t>(d);
    if (LIKELY(LIKELY(asInt32 != d) || UNLIKELY(!asInt32 && std::signbit(d)))) { // true for -0.0
        return false;
    }
    return true;
}

inline Value::Value(const double& d)
{
    int32_t asInt32;
    if (UNLIKELY(isInt32ConvertibleDouble(d, asInt32))) {
        *this = Value(asInt32);
        return;
    }
#ifdef ESCARGOT_64
    if (UNLIKELY((bitwise_cast<int64_t>(d) & DoubleInvalidBeginning) == DoubleInvalidBeginning)) {
        *this = Value(EncodeAsDouble, std::numeric_limits<double>::quiet_NaN());
        return;
    }
#endif
    *this = Value(EncodeAsDouble, d);
}

inline Value::Value(char i)
{
    *this = Value(static_cast<int32_t>(i));
}

inline Value::Value(unsigned char i)
{
    *this = Value(static_cast<int32_t>(i));
}

inline Value::Value(short i)
{
    *this = Value(static_cast<int32_t>(i));
}

inline Value::Value(unsigned short i)
{
    *this = Value(static_cast<int32_t>(i));
}

inline Value::Value(unsigned i)
{
    const int32_t asInt32 = static_cast<int32_t>(i);
    if (UNLIKELY(asInt32 < 0)) {
        *this = Value(EncodeAsDouble, static_cast<double>(i));
        return;
    }
    *this = Value(asInt32);
}

inline Value::Value(long i)
{
    const int32_t asInt32 = static_cast<int32_t>(i);
    if (UNLIKELY(asInt32 != i)) {
        *this = Value(EncodeAsDouble, static_cast<double>(i));
        return;
    }
    *this = Value(asInt32);
}

inline Value::Value(unsigned long i)
{
    const uint32_t asInt32 = static_cast<uint32_t>(i);
    if (UNLIKELY(asInt32 != i)) {
        *this = Value(EncodeAsDouble, static_cast<double>(i));
        return;
    }
    *this = Value(static_cast<uint32_t>(i));
}

inline Value::Value(long long i)
{
    const int32_t asInt32 = static_cast<int32_t>(i);
    if (UNLIKELY(asInt32 != i)) {
        *this = Value(EncodeAsDouble, static_cast<double>(i));
        return;
    }
    *this = Value(asInt32);
}

inline Value::Value(unsigned long long i)
{
    const uint32_t asInt32 = static_cast<uint32_t>(i);
    if (UNLIKELY(asInt32 != i)) {
        *this = Value(EncodeAsDouble, static_cast<double>(i));
        return;
    }
    *this = Value(asInt32);
}

inline bool Value::isUInt32() const
{
    return isInt32() && asInt32() >= 0;
}

inline uint32_t Value::asUInt32() const
{
    ASSERT(isUInt32());
    return asInt32();
}

ALWAYS_INLINE double Value::asNumber() const
{
    ASSERT(isNumber());
    return isInt32() ? asInt32() : asDouble();
}

inline bool Value::isPrimitive() const
{
#ifdef ESCARGOT_32
    return tag() != ObjectPointerTag;
#else
    return isUndefined() || isNull() || isNumber() || isString() || isBoolean() || isSymbol() || isBigInt();
#endif
}

inline bool Value::isCallable() const
{
    if (UNLIKELY(!isPointerValue() || !asPointerValue()->isCallable())) {
        return false;
    }
    return true;
}

// https://www.ecma-international.org/ecma-262/6.0/#sec-tonumber
inline double Value::toNumber(ExecutionState& state) const
{
#ifdef ESCARGOT_64
    auto n = u.asInt64 & TagTypeNumber;
    if (LIKELY(n)) {
        if (n == TagTypeNumber) {
            return FastI2D(asInt32());
        } else {
            return asDouble();
        }
    }
#else
    if (LIKELY(isInt32()))
        return FastI2D(asInt32());
    else if (isDouble())
        return asDouble();
#endif
    else if (isUndefined())
        return std::numeric_limits<double>::quiet_NaN();
    else if (isNull())
        return 0;
    else if (isBoolean())
        return asBoolean() ? 1 : 0;
    else {
        return toNumberSlowCase(state);
    }
}

inline std::pair<Value, bool> Value::toNumeric(ExecutionState& state) const // <Value, isBigInt>
{
// fast path
#ifdef ESCARGOT_64
    auto n = u.asInt64 & TagTypeNumber;
    if (LIKELY(n)) {
        return std::make_pair(*this, false);
    }
#else
    if (LIKELY(isInt32())) {
        return std::make_pair(*this, false);
    } else if (isDouble()) {
        return std::make_pair(*this, false);
    }
#endif
    else if (isUndefined()) {
        return std::make_pair(Value(std::numeric_limits<double>::quiet_NaN()), false);
    } else if (isNull()) {
        return std::make_pair(Value(0), false);
    } else if (isBoolean()) {
        return std::make_pair(Value(asBoolean() ? 1 : 0), false);
    } else {
        return toNumericSlowCase(state);
    }
}

ALWAYS_INLINE Object* Value::toObject(ExecutionState& ec) const // $7.1.13 ToObject
{
    if (LIKELY(isObject())) {
        return asObject();
    } else {
        return toObjectSlowCase(ec);
    }
}

inline Value Value::toPrimitive(ExecutionState& ec, PrimitiveTypeHint preferredType) const
{
    if (UNLIKELY(!isPrimitive())) {
        return toPrimitiveSlowCase(ec, preferredType);
    } else {
        return *this;
    }
}

inline bool Value::abstractEqualsTo(ExecutionState& state, const Value& val) const
{
    if (isInt32() && val.isInt32()) {
#ifdef ESCARGOT_64
        return u.asInt64 == val.u.asInt64;
#else
        return u.asBits.payload == val.u.asBits.payload;
#endif
    } else {
        return abstractEqualsToSlowCase(state, val);
    }
}

inline bool Value::equalsTo(ExecutionState& state, const Value& val) const
{
    if (isInt32() && val.isInt32()) {
#ifdef ESCARGOT_64
        return u.asInt64 == val.u.asInt64;
#else
        return u.asBits.payload == val.u.asBits.payload;
#endif
    } else {
        return equalsToSlowCase(state, val);
    }
}

inline bool Value::toBoolean(ExecutionState& ec) const // $7.1.2 ToBoolean
{
    if (isBoolean())
        return asBoolean();

    if (isInt32())
        return asInt32();

    return toBooleanSlowCase(ec);
}

inline int32_t Value::toInt32(ExecutionState& state) const // $7.1.5 ToInt3
{
    // consume fast case
    if (LIKELY(isInt32()))
        return asInt32();

    return toInt32SlowCase(state);
}

inline uint32_t Value::toUint32(ExecutionState& state) const // http://www.ecma-international.org/ecma-262/5.1/#sec-9.6
{
    return toInt32(state);
}

inline Value::ValueIndex Value::toIndex(ExecutionState& ec) const
{
    // https://www.ecma-international.org/ecma-262/10.0/#sec-toindex
    // RangeError exception should be checked by caller of toIndex
    int32_t i;
    if (LIKELY(isInt32() && (i = asInt32()) >= 0)) {
        return i;
    }

    if (UNLIKELY(isUndefined())) {
        return 0;
    }

    auto integerIndex = toInteger(ec);
    Value::ValueIndex index = Value(integerIndex).toLength(ec);
    if (UNLIKELY(integerIndex < 0 || integerIndex != index)) {
        return Value::InvalidIndexValue;
    }
    return index;
}

inline uint32_t Value::toIndex32(ExecutionState& state) const
{
    if (LIKELY(isUInt32())) {
        return asUInt32();
    } else {
        uint32_t newLen = toUint32(state);
        if (newLen != toNumber(state)) {
            return Value::InvalidIndex32Value;
        } else {
            return newLen;
        }
    }
}

Value::ValueIndex Value::tryToUseAsIndex(ExecutionState& ec) const
{
    if (LIKELY(isUInt32())) {
        return asUInt32();
    } else {
        return tryToUseAsIndexSlowCase(ec);
    }
}

uint32_t Value::tryToUseAsIndex32(ExecutionState& ec) const
{
    if (LIKELY(isUInt32())) {
        return asUInt32();
    } else {
        return tryToUseAsIndex32SlowCase(ec);
    }
}

uint32_t Value::tryToUseAsIndexProperty(ExecutionState& ec) const
{
    return tryToUseAsIndex32(ec);
}

inline double Value::toInteger(ExecutionState& state) const
{
    if (LIKELY(isInt32())) {
        return asInt32();
    }

    double d = toNumber(state);
    if (std::isnan(d) || d == 0) {
        return 0;
    }
    if (d == std::numeric_limits<double>::infinity() || d == -std::numeric_limits<double>::infinity()) {
        return d;
    }
    return (d < 0 ? -1 : 1) * std::floor(std::abs(d));
}

inline bool Value::isInteger(ExecutionState& state) const
{
    if (LIKELY(isInt32())) {
        return true;
    }

    double d = toNumber(state);
    if (std::isnan(d) || d == std::numeric_limits<double>::infinity() || d == -std::numeric_limits<double>::infinity()) {
        return false;
    }
    return std::floor(std::abs(d)) == std::abs(d);
}

inline uint64_t Value::toLength(ExecutionState& state) const
{
    double len = toInteger(state);
    if (UNLIKELY(len <= 0.0)) {
        return 0;
    }
    return std::min(len, maximumLength());
}
} // namespace Escargot

#endif
