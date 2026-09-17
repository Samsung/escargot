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
#include "runtime/Symbol.h"
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

// Immediates are written as one 64-bit store: the tag and the payload are both
// compile-time constants, so there is no reason to store the halves separately.
inline Value::Value()
{
    u.asInt64 = static_cast<int64_t>(UndefinedValueBits);
}

inline Value::Value(NullInitTag)
{
    u.asInt64 = static_cast<int64_t>(NullValueBits);
}

inline Value::Value(UndefinedInitTag)
{
    u.asInt64 = static_cast<int64_t>(UndefinedValueBits);
}

inline Value::Value(EmptyValueInitTag)
{
    u.asInt64 = static_cast<int64_t>(valueBits(EmptyValueTag, 0));
}

inline Value::Value(TrueInitTag)
{
    u.asInt64 = static_cast<int64_t>(TrueValueBits);
}

inline Value::Value(FalseInitTag)
{
    u.asInt64 = static_cast<int64_t>(FalseValueBits);
}

inline Value::Value(bool b)
{
    // Only the select bit differs between the two boolean patterns.
    u.asInt64 = static_cast<int64_t>(FalseValueBits | (static_cast<uint64_t>(b) * ImmediateSelectValueBit));
}

inline Value::Value(FromPayloadTag, intptr_t ptr)
{
    u.asBits.tag = OtherPointerTag;
    u.asBits.payload = static_cast<int32_t>(ptr);
}

inline Value::Value(FromEncodedPayloadTag, intptr_t bits)
{
    const uintptr_t kind = pointerKind(static_cast<uintptr_t>(bits));
    ASSERT(kind == ObjectPointerKind || kind == OtherPointerKind);
    u.asBits.tag = static_cast<uint32_t>(ObjectPointerTag) + static_cast<uint32_t>(kind >> 1);
    u.asBits.payload = static_cast<int32_t>(reinterpret_cast<uintptr_t>(untagPointer(static_cast<uintptr_t>(bits))));
}

inline Value::Value(FromObjectEncodedPayloadTag, intptr_t bits)
{
    ASSERT(bits != ValueEmpty);
    ASSERT(pointerKind(static_cast<uintptr_t>(bits)) == ObjectPointerKind);
    u.asBits.tag = ObjectPointerTag;
    u.asBits.payload = static_cast<int32_t>(bits);
}

inline Value::Value(FromOtherEncodedPayloadTag, intptr_t bits)
{
    ASSERT(pointerKind(static_cast<uintptr_t>(bits)) == OtherPointerKind);
    u.asBits.tag = OtherPointerTag;
    u.asBits.payload = static_cast<int32_t>(static_cast<uintptr_t>(bits) & ~PointerKindMask);
}

inline Value::Value(PointerValue* ptr)
{
    // PointerValue* is an erased static type at several native/API boundaries.
    // Preserve object semantics there; typed constructors below remain load-free.
    ASSERT(ptr);
    u.asBits.tag = ptr->isObject() ? static_cast<uint32_t>(ObjectPointerTag) : static_cast<uint32_t>(OtherPointerTag);
    u.asBits.payload = reinterpret_cast<int32_t>(ptr);
}

inline Value::Value(const PointerValue* ptr)
    : Value(const_cast<PointerValue*>(ptr))
{
}

inline Value::Value(Object* ptr)
{
    u.asBits.tag = ObjectPointerTag;
    u.asBits.payload = reinterpret_cast<int32_t>(ptr);
}

inline Value::Value(const Object* ptr)
    : Value(const_cast<Object*>(ptr))
{
}
inline Value::Value(String* ptr)
{
    u.asBits.tag = OtherPointerTag;
    u.asBits.payload = reinterpret_cast<int32_t>(ptr);
}
inline Value::Value(const String* ptr)
    : Value(const_cast<String*>(ptr))
{
}
inline Value::Value(Symbol* ptr)
{
    u.asBits.tag = OtherPointerTag;
    u.asBits.payload = reinterpret_cast<int32_t>(ptr);
}
inline Value::Value(const Symbol* ptr)
    : Value(const_cast<Symbol*>(ptr))
{
}
inline Value::Value(BigInt* ptr)
{
    u.asBits.tag = OtherPointerTag;
    u.asBits.payload = reinterpret_cast<int32_t>(ptr);
}
inline Value::Value(const BigInt* ptr)
    : Value(const_cast<BigInt*>(ptr))
{
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

inline intptr_t Value::payload() const
{
    return rawPayload();
}

inline intptr_t Value::rawPayload() const
{
    return static_cast<uint32_t>(u.asBits.payload);
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
    // Only bit 3 differs between the two boolean payloads.
    return (rawPayload() & ImmediatePayloadSelectBit) != 0;
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
    // Int32Tag is the lowest immediate tag and every double sits below it, so
    // both kinds of number are covered by a single unsigned compare.
    return tag() <= static_cast<uint32_t>(Int32Tag);
}

inline bool Value::isPointerValue() const
{
    // The two pointer tags are two apart by construction (see the
    // COMPILE_ASSERT in Value.h) and the slot in between stays reserved, so
    // both -- and nothing else -- fall in a single unsigned range. Heap
    // pointers can never take one of the immediate payloads, which lets the
    // immediate test be shared by both tags instead of branching on the tag
    // twice.
    return (tag() - static_cast<uint32_t>(ObjectPointerTag)) <= static_cast<uint32_t>(PointerTagSpan) && !isImmediatePayload(rawPayload());
}

inline bool Value::isOpaquePointer() const
{
    return tag() == OtherPointerTag && !isImmediatePayload(rawPayload());
}

// Tag and payload are both fixed for an immediate, so these are single 64-bit
// compares rather than a tag test followed by a payload test. The pair tests
// mask the select bit out first, exactly like the 64-bit build does.
inline bool Value::isUndefined() const
{
    return static_cast<uint64_t>(u.asInt64) == UndefinedValueBits;
}

inline bool Value::isNull() const
{
    return static_cast<uint64_t>(u.asInt64) == NullValueBits;
}

inline bool Value::isUndefinedOrNull() const
{
    return (static_cast<uint64_t>(u.asInt64) | ImmediateSelectValueBit) == UndefinedValueBits;
}

inline bool Value::isBoolean() const
{
    return (static_cast<uint64_t>(u.asInt64) | ImmediateSelectValueBit) == TrueValueBits;
}

inline bool Value::isTrue() const
{
    return static_cast<uint64_t>(u.asInt64) == TrueValueBits;
}

inline bool Value::isFalse() const
{
    return static_cast<uint64_t>(u.asInt64) == FalseValueBits;
}

inline PointerValue* Value::asPointerValue() const
{
    ASSERT(isPointerValue());
    return reinterpret_cast<PointerValue*>(u.asBits.payload);
}

inline bool Value::isString() const
{
    return tag() == OtherPointerTag && !isImmediatePayload(rawPayload()) && asPointerValue()->isString();
}

inline bool Value::isSymbol() const
{
    return tag() == OtherPointerTag && !isImmediatePayload(rawPayload()) && asPointerValue()->isSymbol();
}

inline bool Value::isBigInt() const
{
    return tag() == OtherPointerTag && !isImmediatePayload(rawPayload()) && asPointerValue()->isBigInt();
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

inline void* Value::asOpaquePointer() const
{
    ASSERT(isOpaquePointer());
    return reinterpret_cast<void*>(u.asBits.payload);
}

inline bool Value::isObject() const
{
    return tag() == ObjectPointerTag;
}

inline Object* Value::asObject() const
{
    ASSERT(isObject());
    return reinterpret_cast<Object*>(u.asBits.payload);
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
    u.asPointerBits = tagPointer(reinterpret_cast<void*>(ptr), OpaquePointerKind);
}

inline Value::Value(FromEncodedPayloadTag, intptr_t bits)
{
    u.asPointerBits = static_cast<uintptr_t>(bits);
}

inline Value::Value(bool b)
{
    u.asInt64 = (TagBitTypeOther | (b << TagTypeShift));
}

inline Value::Value(PointerValue* ptr)
{
    // PointerValue* is an erased static type at several native/API boundaries.
    // Preserve object semantics there; typed constructors below remain load-free.
    ASSERT(ptr);
    u.asPointerBits = tagPointer(ptr, ptr->isObject() ? ObjectPointerKind : OtherPointerKind);
}

inline Value::Value(const PointerValue* ptr)
    : Value(const_cast<PointerValue*>(ptr))
{
}

inline Value::Value(Object* ptr) { u.asPointerBits = tagPointer(ptr, ObjectPointerKind); }
inline Value::Value(const Object* ptr)
    : Value(const_cast<Object*>(ptr))
{
}
inline Value::Value(String* ptr) { u.asPointerBits = tagPointer(ptr, OtherPointerKind); }
inline Value::Value(const String* ptr)
    : Value(const_cast<String*>(ptr))
{
}
inline Value::Value(Symbol* ptr) { u.asPointerBits = tagPointer(ptr, OtherPointerKind); }
inline Value::Value(const Symbol* ptr)
    : Value(const_cast<Symbol*>(ptr))
{
}
inline Value::Value(BigInt* ptr) { u.asPointerBits = tagPointer(ptr, OtherPointerKind); }
inline Value::Value(const BigInt* ptr)
    : Value(const_cast<BigInt*>(ptr))
{
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
    // Perform the offset addition in unsigned arithmetic: the bit pattern of a
    // double reinterpreted as int64_t can be negative (e.g. NaN payloads with the
    // sign bit set), and adding DoubleEncodeOffset to it can overflow int64_t,
    // which is UB for signed integers. Unsigned overflow wraps around by
    // definition and yields the same bit pattern we want here.
    u.asInt64 = static_cast<int64_t>(static_cast<uint64_t>(reinterpretDoubleToInt64(d)) + static_cast<uint64_t>(DoubleEncodeOffset));
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

// The number predicates below shift the tag down instead of reading it through
// `(unsigned short*)&u.asInt64`. Taking the address of the payload makes the
// Value addressable, which can spill a Value the interpreter was holding in a
// register; ARM has no memory-operand compare, so the load-halfword form was
// never cheaper there either. The shift form is also endian-independent (no
// #if) and free of the strict-aliasing violation.
ALWAYS_INLINE bool Value::isInt32() const
{
    return (static_cast<uint64_t>(u.asInt64) >> NumberTagShift) == 0xffff;
}

inline bool Value::isDouble() const
{
    // One read of the number tag: non-zero means number, all-ones means int32.
    const uint64_t t = static_cast<uint64_t>(u.asInt64) >> NumberTagShift;
    return t && t != 0xffff;
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
    // See the comment in the EncodeAsDoubleTag constructor: do the inverse
    // subtraction in unsigned arithmetic to avoid signed-overflow UB.
    return reinterpretInt64ToDouble(static_cast<int64_t>(static_cast<uint64_t>(u.asInt64) - static_cast<uint64_t>(DoubleEncodeOffset)));
}

inline bool Value::isEmpty() const
{
    return u.asInt64 == ValueEmpty;
}

ALWAYS_INLINE bool Value::isNumber() const
{
    return (static_cast<uint64_t>(u.asInt64) >> NumberTagShift) != 0;
}

// String/Symbol/BigInt are always stored with OtherPointerKind, so gating on the
// kind is both cheaper than isPointerValue() and rejects objects without loading
// their type tag at all. Mirrors what the 32-bit build does with OtherPointerTag.
ALWAYS_INLINE bool Value::hasOtherPointerKind() const
{
    return (u.asPointerBits & (static_cast<uintptr_t>(TagTypeNumber) | PointerKindMask)) == OtherPointerKind;
}

inline bool Value::isString() const
{
    return hasOtherPointerKind() && asPointerValue()->isString();
}

inline bool Value::isSymbol() const
{
    return hasOtherPointerKind() && asPointerValue()->isSymbol();
}

inline bool Value::isBigInt() const
{
    return hasOtherPointerKind() && asPointerValue()->isBigInt();
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

// Every immediate has TagBitTypeOther set and every number has a non-zero
// number tag, so the two pointer kinds in use (Object = 0, Other = 4) are
// exactly the patterns that clear TagMask -- no kind compare needed. Empty is
// the only non-pointer that survives the mask, hence the extra test.
inline bool Value::isPointerValue() const
{
    const uintptr_t bits = u.asPointerBits;
    return bits != ValueEmpty && !(bits & TagMask);
}

inline bool Value::isOpaquePointer() const
{
    // OpaquePointerKind is OtherPointerKind, so this cannot distinguish an
    // opaque pointer from a String/Symbol/BigInt -- it never could.
    COMPILE_ASSERT(OpaquePointerKind == OtherPointerKind, "");
    return hasOtherPointerKind();
}

inline bool Value::isUndefined() const
{
    return u.asInt64 == ValueUndefined;
}

inline bool Value::isNull() const
{
    return u.asInt64 == ValueNull;
}

inline bool Value::isUndefinedOrNull() const
{
    COMPILE_ASSERT(ValueUndefined == (ValueNull | (1 << TagTypeShift)), "");

    return (u.asInt64 | (1 << TagTypeShift)) == ValueUndefined;
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
    return reinterpret_cast<PointerValue*>(untagPointer(u.asPointerBits));
}

inline void* Value::asOpaquePointer() const
{
    ASSERT(isOpaquePointer());
    return untagPointer(u.asPointerBits);
}

inline bool Value::isObject() const
{
    // Same folding as isPointerValue(), with the kind pinned to Object. Empty is
    // again the only leftover: 0x8 is the one other pattern that would pass the
    // mask while being <= ValueLast, and no Value ever holds it.
    const uintptr_t bits = u.asPointerBits;
    return bits != ValueEmpty && !(bits & (static_cast<uintptr_t>(TagTypeNumber) | PointerKindMask));
}

inline Object* Value::asObject() const
{
    ASSERT(isObject());
    return reinterpret_cast<Object*>(u.asPointerBits);
}

inline intptr_t Value::payload() const
{
    return rawPayload();
}

inline intptr_t Value::rawPayload() const
{
    return u.asInt64;
}

#endif

// ==============================================================================
// ===common architecture========================================================
// ==============================================================================

template <typename T, typename std::enable_if<std::is_convertible<T*, PointerValue*>::value && !std::is_same<PointerValue, typename std::remove_cv<T>::type>::value, int>::type>
inline Value::Value(T* ptr)
{
    typedef typename std::remove_cv<T>::type RawType;
    const uintptr_t kind = std::is_base_of<Object, RawType>::value ? ObjectPointerKind : OtherPointerKind;
#ifdef ESCARGOT_32
    u.asBits.tag = kind == ObjectPointerKind ? static_cast<uint32_t>(ObjectPointerTag) : static_cast<uint32_t>(OtherPointerTag);
    u.asBits.payload = reinterpret_cast<int32_t>(ptr);
#else
    u.asPointerBits = tagPointer(ptr, kind);
#endif
}


inline UnconvertibleDoubleToInt32::UnconvertibleDoubleToInt32(double&& v)
    : value(std::forward<double>(v))
{
    ASSERT(!Value::isInt32ConvertibleDouble(v));
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

ALWAYS_INLINE int32_t Value::truncateDoubleToInt32Unchecked(double d)
{
    return static_cast<int32_t>(d);
}

inline Value::Value(NaNInitTag)
{
    *this = Value(EncodeAsDouble, std::numeric_limits<double>::quiet_NaN());
}

inline Value::Value(PostiveInfinityInitTag)
{
    *this = Value(EncodeAsDouble, std::numeric_limits<double>::infinity());
}

inline Value::Value(NegativeInfinityInitTag)
{
    *this = Value(EncodeAsDouble, -std::numeric_limits<double>::infinity());
}

inline Value::Value(DoubleToIntConvertibleTestNeedsTag, double d)
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
    *this = Value(static_cast<int>(i));
}

inline Value::Value(unsigned char i)
{
    *this = Value(static_cast<int>(i));
}

inline Value::Value(short i)
{
    *this = Value(static_cast<int>(i));
}

inline Value::Value(unsigned short i)
{
    *this = Value(static_cast<int>(i));
}

inline Value::Value(unsigned i)
{
    const int32_t asInt32 = static_cast<int32_t>(i);
    if (UNLIKELY(asInt32 < 0)) {
        *this = Value(EncodeAsDouble, static_cast<double>(i));
        return;
    }
    *this = Value(static_cast<int>(asInt32));
}

inline Value::Value(long i)
{
    const int32_t asInt32 = static_cast<int32_t>(i);
    if (UNLIKELY(asInt32 != i)) {
        *this = Value(EncodeAsDouble, static_cast<double>(i));
        return;
    }
    *this = Value(static_cast<int>(asInt32));
}

inline Value::Value(unsigned long i)
{
    const uint32_t asUInt32 = static_cast<uint32_t>(i);
    if (UNLIKELY(asUInt32 != i)) {
        *this = Value(EncodeAsDouble, static_cast<double>(i));
        return;
    }
    *this = Value(static_cast<unsigned int>(asUInt32));
}

inline Value::Value(long long i)
{
    const int32_t asInt32 = static_cast<int32_t>(i);
    if (UNLIKELY(asInt32 != i)) {
        *this = Value(EncodeAsDouble, static_cast<double>(i));
        return;
    }
    *this = Value(static_cast<int>(asInt32));
}

inline Value::Value(unsigned long long i)
{
    const uint32_t asUInt32 = static_cast<uint32_t>(i);
    if (UNLIKELY(asUInt32 != i)) {
        *this = Value(EncodeAsDouble, static_cast<double>(i));
        return;
    }
    *this = Value(static_cast<unsigned int>(asUInt32));
}

inline bool Value::isUInt32() const
{
    // The int32 tag and the sign bit of the payload live in one word, so the
    // array-index fast paths (toIndex32/tryToUseAsIndex*) pay a single
    // mask-and-compare instead of a tag test plus a sign test.
#ifdef ESCARGOT_32
    return (static_cast<uint64_t>(u.asInt64) & UInt32ValueBitsMask) == UInt32ValueBits;
#else
    return (static_cast<uint64_t>(u.asInt64) & (static_cast<uint64_t>(TagTypeNumber) | 0x80000000ull)) == static_cast<uint64_t>(TagTypeNumber);
#endif
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
    return !isObject();
#endif
}

inline bool Value::isCallable() const
{
    // Every callable is Object-derived (FunctionObject, BoundFunctionObject,
    // WrappedFunctionObject, ProxyObject), so the cheaper object test is
    // equivalent here -- same reasoning as isFunctionObject() below.
    return isObject() && asPointerValue()->isCallable();
}

// https://www.ecma-international.org/ecma-262/6.0/#sec-tonumber
inline double Value::toNumber(ExecutionState& state) const
{
// there is optimizer bug on clang-cl with below block
#if defined(ESCARGOT_64) && !defined(COMPILER_CLANG_CL)
    auto n = u.asInt64 & TagTypeNumber;
    if (LIKELY(n)) {
        if (n == TagTypeNumber) {
            return FastI2D(asInt32());
        } else {
            return asDouble();
        }
    }
#else
    // One tag read splits int32 from double: Int32Tag sits directly on top of
    // the double range (see the layout note in Value.h).
    const uint32_t numberTag = tag();
    if (LIKELY(numberTag <= static_cast<uint32_t>(Int32Tag))) {
        return numberTag == static_cast<uint32_t>(Int32Tag) ? FastI2D(asInt32()) : asDouble();
    }
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
    if (LIKELY(isNumber())) {
        return std::make_pair(*this, false);
    }
#endif
    else if (isUndefined()) {
        return std::make_pair(Value(Value::NanInit), false);
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
    if (u.asInt64 == val.u.asInt64) {
        if (UNLIKELY(isDouble())) {
            double d = asDouble();
            return !std::isnan(d);
        }
        return true;
    }

    if (isNumber() && val.isNumber()) {
        return asNumber() == val.asNumber();
    }

    if (isUndefinedOrNull() || val.isUndefinedOrNull()) {
#if defined(ESCARGOT_ENABLE_TEST)
        // Under spec compliance test environments (like test262), we must support historical
        // quirks such as IsHTMLDDA objects (e.g. document.all), where document.all == null/undefined
        // must observably evaluate to true. Since document.all is an Object (PointerValue),
        // we fall back to the out-of-line slow case handler to resolve this specific spec exception.
        if (UNLIKELY(isPointerValue() || val.isPointerValue())) {
            return abstractEqualsToSlowCase(state, val);
        }
#endif
        return isUndefinedOrNull() && val.isUndefinedOrNull();
    }

    if (isObject() && val.isObject()) {
        return false;
    }

    return abstractEqualsToSlowCase(state, val);
}

inline bool Value::equalsTo(ExecutionState& state, const Value& val) const
{
    if (u.asInt64 == val.u.asInt64) {
        if (UNLIKELY(isDouble())) {
            double d = asDouble();
            return !std::isnan(d);
        }
        return true;
    }

    // Numbers first: if both sides are numbers neither can be a pointer, so the
    // pointer-ness test below could never have fired -- and it is the more
    // expensive of the two.
    if (isNumber() && val.isNumber()) {
        return asNumber() == val.asNumber();
    }

    if (isPointerValue() != val.isPointerValue()) {
        return false;
    }

    if (isObject() && val.isObject()) {
        return false;
    }

    return equalsToSlowCase(state, val);
}

inline bool Value::equalsToByTheSameValueZeroAlgorithm(ExecutionState& state, const Value& val) const
{
    // fast path: two SMIs never need the double round-trip in the slow case
    // below (also sidesteps the isPointerValue/isNumber checks entirely,
    // since an int32-tagged Value can never be a pointer). Inlined at every
    // call site so the common Set/Map small-integer-key case pays for a
    // single compare instead of a function call.
    if (LIKELY(isInt32() && val.isInt32())) {
        return asInt32() == val.asInt32();
    }
    return equalsToByTheSameValueZeroAlgorithmSlowCase(state, val);
}

inline bool Value::toBoolean() const // $7.1.2 ToBoolean
{
    if (isBoolean())
        return asBoolean();

    if (isInt32())
        return asInt32();

    if (isUndefinedOrNull())
        return false;

    if (isObject()) {
#if defined(ESCARGOT_ENABLE_TEST)
        if (UNLIKELY(checkIfObjectWithIsHTMLDDA())) {
            return false;
        }
#endif
        return true;
    }

    return toBooleanSlowCase();
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
    Value::ValueIndex index = Value(Value::DoubleToIntConvertibleTestNeeds, integerIndex).toLength(ec);
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

inline Value Value::toCanonicalizeKeyedCollectionKey(ExecutionState&) const
{
    if (isDouble()) {
        double d = asDouble();
        // convert -0.0 into 0.0
        // in c++, d = -0.0, d == 0.0 is true
        if (d == 0.0) {
            return Value(0);
        }
    }
    return *this;
}

inline bool Value::isArrayObject() const
{
    return isObject() && asPointerValue()->hasArrayObjectTag();
}

inline ArrayObject* Value::asArrayObject() const
{
    return asPointerValue()->asArrayObject();
}

// Every object-derived type is stored with the object tag/kind, so gating these
// on isObject() instead of isPointerValue() is both cheaper and equivalent.
inline bool Value::isFunctionObject() const
{
    return isObject() && asPointerValue()->isFunctionObject();
}

inline bool Value::isExtendedNativeFunctionObject() const
{
    return isObject() && asPointerValue()->isExtendedNativeFunctionObject();
}

inline FunctionObject* Value::asFunctionObject() const
{
    return asPointerValue()->asFunctionObject();
}

inline ExtendedNativeFunctionObject* Value::asExtendedNativeFunctionObject() const
{
    return asPointerValue()->asExtendedNativeFunctionObject();
}

} // namespace Escargot

#endif
