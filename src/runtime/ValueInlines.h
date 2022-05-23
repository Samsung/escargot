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

#pragma pack(push, 1)
// NumberInEncodedValue stores its tag in `this + sizeof(size_t)`
// the location is same with PointerValues
// store double
class DoubleInValue : public gc {
public:
    explicit DoubleInValue(const double& v)
#ifdef ESCARGOT_32
    {
        m_buffer[1] = POINTER_VALUE_NUMBER_TAG_IN_DATA;
        setValue(v);
    }
#else
        : m_value(v)
        , m_typeTag(POINTER_VALUE_NUMBER_TAG_IN_DATA)
    {
    }
#endif

    void* operator new(size_t size)
    {
        return GC_MALLOC_ATOMIC(size);
    }
    void* operator new[](size_t size) = delete;

    double value() const
    {
#ifdef ESCARGOT_32
        double ret;
        uint32_t* buf = reinterpret_cast<uint32_t*>(&ret);
        buf[0] = m_buffer[0];
        buf[1] = m_buffer[2];
        return ret;
#else
        return m_value;
#endif
    }

    void setValue(const double& v)
    {
#ifdef ESCARGOT_32
        const uint32_t* buf = reinterpret_cast<const uint32_t*>(&v);
        m_buffer[0] = buf[0];
        m_buffer[2] = buf[1];
#else
        m_value = v;
#endif
    }

private:
#ifdef ESCARGOT_32
    uint32_t m_buffer[3];
#else
    double m_value;
    size_t m_typeTag;
#endif
};
#pragma pack(pop)

// ==============================================================================
// ===32-bit architecture========================================================
// ==============================================================================

#ifdef ESCARGOT_32

#else

// ==============================================================================
// ===64-bit architecture========================================================
// ==============================================================================

#endif

// ==============================================================================
// ===common architecture========================================================
// ==============================================================================

inline Value::Value(ForceUninitializedTag)
{
}

inline Value::Value()
{
    m_data.payload = ValueUndefined;
}

inline Value::Value(NullInitTag)
{
    m_data.payload =  ValueNull;
}

inline Value::Value(UndefinedInitTag)
{
    m_data.payload = ValueUndefined;
}

inline Value::Value(EmptyValueInitTag)
{
    m_data.payload = ValueEmpty;
}

inline Value::Value(TrueInitTag)
{
    m_data.payload = ValueTrue;
}

inline Value::Value(FalseInitTag)
{
    m_data.payload = ValueFalse;
}

inline Value::Value(bool b)
{
    m_data.payload = b ? ValueTrue : ValueFalse;
}

inline Value::Value(FromPayloadTag, intptr_t ptr)
{
    m_data.payload = ptr;
}

inline Value::Value(PointerValue* ptr)
{
    m_data.payload = reinterpret_cast<intptr_t>(ptr);
}

inline Value::Value(const PointerValue* ptr)
{
    m_data.payload = reinterpret_cast<intptr_t>(ptr);
}

inline Value::Value(int i)
{
    if (LIKELY(EncodedValueImpl::PlatformSmiTagging::IsValidSmi(i))) {
        m_data.payload = EncodedValueImpl::PlatformSmiTagging::IntToSmi(i);
    } else {
        *this = Value(Value::EncodeAsDouble, static_cast<double>(i));
    }
}

inline Value::Value(EncodeAsDoubleTag, const double& v)
{
    m_data.payload = reinterpret_cast<intptr_t>(new DoubleInValue(v));
}

inline intptr_t Value::asRawData() const
{
    return m_data.payload;
}

inline bool Value::operator==(const Value& other) const
{
    return equalsToByTheSameValueAlgorithm(other);
}

inline bool Value::operator!=(const Value& other) const
{
    return !equalsToByTheSameValueAlgorithm(other);
}

ALWAYS_INLINE bool Value::isInt32() const
{
    return HAS_SMI_TAG(m_data.payload);
}

ALWAYS_INLINE bool Value::isDouble() const
{
    return isHeapValue() && readPointerValueTag() == POINTER_VALUE_NUMBER_TAG_IN_DATA;
}

inline int32_t Value::asInt32() const
{
    ASSERT(isInt32());
    return EncodedValueImpl::PlatformSmiTagging::SmiToInt(m_data.payload);
}

inline bool Value::asBoolean() const
{
    ASSERT(isBoolean());
    return m_data.payload == ValueTrue;
}

inline double Value::asDouble() const
{
    ASSERT(isDouble());
    return reinterpret_cast<DoubleInValue*>(m_data.payload)->value();
}

inline bool Value::isEmpty() const
{
    return m_data.payload == ValueEmpty;
}

ALWAYS_INLINE bool Value::isNumber() const
{
    return isInt32() || isDouble();
}

inline bool Value::isHeapValue() const
{
    return !(m_data.payload & (EncodedValueImpl::kSmiTagMask | TagBitTypeOther));
}

inline bool Value::isPointerValue() const
{
    return isHeapValue() && readPointerValueTag() != POINTER_VALUE_NUMBER_TAG_IN_DATA;
}

inline bool Value::isUndefined() const
{
    return m_data.payload == ValueUndefined;
}

inline bool Value::isNull() const
{
    return m_data.payload == ValueNull;
}

inline bool Value::isBoolean() const
{
    return (m_data.payload | (1 << TagTypeShift)) == ValueTrue;
}

inline bool Value::isTrue() const
{
    return m_data.payload == ValueTrue;
}

inline bool Value::isFalse() const
{
    return m_data.payload == ValueFalse;
}

inline PointerValue* Value::asPointerValue() const
{
    ASSERT(isPointerValue());
    return reinterpret_cast<PointerValue*>(m_data.payload);
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

inline bool Value::isObject() const
{
    if (LIKELY(isHeapValue())) {
        if (LIKELY(!(readPointerValueTag() & POINTER_VALUE_NOT_OBJECT_TAG_IN_DATA))) {
            return true;
        }
    }
    return false;
}

inline Object* Value::asObject() const
{
    return asPointerValue()->asObject();
}

inline bool Value::isFunction() const
{
    return isPointerValue() && asPointerValue()->isFunctionObject();
}

inline bool Value::isExtendedNativeFunctionObject() const
{
    return isPointerValue() && asPointerValue()->isExtendedNativeFunctionObject();
}

inline FunctionObject* Value::asFunction() const
{
    return asPointerValue()->asFunctionObject();
}

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
    return isUndefined() || isNull() || isNumber() || isString() || isBoolean() || isSymbol() || isBigInt();
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
    if (LIKELY(isInt32()))
        return FastI2D(asInt32());
    else if (isDouble())
        return asDouble();
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
    if (LIKELY(isInt32())) {
        return std::make_pair(*this, false);
    } else if (isDouble()) {
        return std::make_pair(*this, false);
    } else if (isUndefined()) {
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
        return m_data.payload == val.m_data.payload;
    } else {
        return abstractEqualsToSlowCase(state, val);
    }
}

inline bool Value::equalsTo(ExecutionState& state, const Value& val) const
{
    if (isInt32() && val.isInt32()) {
        return m_data.payload == val.m_data.payload;
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
