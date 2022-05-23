/*
 *  Copyright (C) 2016 Samsung Electronics Co., Ltd. All Rights Reserved
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2007, 2008, 2012 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
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

#ifndef __EscargotValue__
#define __EscargotValue__

#include "util/Vector.h"

namespace Escargot {

namespace EncodedValueImpl {

const int kApiAlignSize = 4;
const int kApiIntSize = sizeof(int);
const int kApiInt64Size = sizeof(int64_t);

// Tag information for small immediate. Other values are heap objects.
const int kSmiTag = 1;
const int kSmiTagSize = 1;
const intptr_t kSmiTagMask = (1 << kSmiTagSize) - 1;

template <size_t ptr_size>
struct SmiTagging;

template <int kSmiShiftSize>
inline int32_t IntToSmiT(int value)
{
    int smi_shift_bits = kSmiTagSize + kSmiShiftSize;
    uintptr_t tagged_value = (static_cast<uintptr_t>(value) << smi_shift_bits) | kSmiTag;
    return (int32_t)(tagged_value);
}

// Smi constants for 32-bit systems.
template <>
struct SmiTagging<4> {
    enum {
        kSmiShiftSize = 0,
        kSmiValueSize = 31
    };
    static int SmiShiftSize()
    {
        return kSmiShiftSize;
    }
    static int SmiValueSize()
    {
        return kSmiValueSize;
    }
    inline static int SmiToInt(intptr_t value)
    {
        int shift_bits = kSmiTagSize + kSmiShiftSize;
        // Throw away top 32 bits and shift down (requires >> to be sign extending).
        return static_cast<int>(value >> shift_bits);
    }
    inline static int32_t IntToSmi(int value)
    {
        return IntToSmiT<kSmiShiftSize>(value);
    }
    inline static bool IsValidSmi(intptr_t value)
    {
        // To be representable as an tagged small integer, the two
        // most-significant bits of 'value' must be either 00 or 11 due to
        // sign-extension. To check this we add 01 to the two
        // most-significant bits, and check if the most-significant bit is 0
        //
        // CAUTION: The original code below:
        // bool result = ((value + 0x40000000) & 0x80000000) == 0;
        // may lead to incorrect results according to the C language spec, and
        // in fact doesn't work correctly with gcc4.1.1 in some cases: The
        // compiler may produce undefined results in case of signed integer
        // overflow. The computation must be done w/ unsigned ints.
        return static_cast<uintptr_t>(value + 0x40000000U) < 0x80000000U;
    }
};

typedef SmiTagging<kApiAlignSize> PlatformSmiTagging;
const int kSmiShiftSize = PlatformSmiTagging::kSmiShiftSize;
const int kSmiValueSize = PlatformSmiTagging::kSmiValueSize;

#if defined(ESCARGOT_64) && defined(ESCARGOT_USE_32BIT_IN_64BIT)
#define HAS_SMI_TAG(value) \
    ((static_cast<intptr_t>((long int)value) & ::Escargot::EncodedValueImpl::kSmiTagMask) == ::Escargot::EncodedValueImpl::kSmiTag)
#else
#define HAS_SMI_TAG(value) \
    ((static_cast<intptr_t>(value) & ::Escargot::EncodedValueImpl::kSmiTagMask) == ::Escargot::EncodedValueImpl::kSmiTag)
#endif
} // namespace EncodedValueImpl

struct ValueData {
    intptr_t payload;
    ValueData()
        : payload(0)
    {
    }

    explicit ValueData(void* ptr)
        : payload((intptr_t)ptr)
    {
    }
};

class PointerValue;
class ExecutionState;
class Object;
class Symbol;
class String;
class BigInt;
class FunctionObject;
class ExtendedNativeFunctionObject;

union ValueDescriptor {
    int64_t asInt64;
#ifdef ESCARGOT_32
    double asDouble;
#elif ESCARGOT_64
    PointerValue* ptr;
#endif
    struct {
        int32_t payload;
        uint32_t tag;
    } asBits;
};

template <typename ToType, typename FromType>
inline ToType bitwise_cast(FromType from)
{
    ASSERT(sizeof(FromType) == sizeof(ToType));
    union {
        FromType from;
        ToType to;
    } u;
    u.from = from;
    return u.to;
}

// All non-numeric (bool, null, undefined) immediates have bit 2 set.

#ifdef ESCARGOT_32
#define TagBitTypeOther 0x2u
#else
#define TagBitTypeOther 0x2ll
#endif

#define TagTypeShift 2

// Combined integer value for non-numeric immediates.
#define ValueFalse (TagBitTypeOther | (0 << TagTypeShift))
#define ValueTrue (TagBitTypeOther | (1 << TagTypeShift))
#define ValueNull (TagBitTypeOther | (2 << TagTypeShift))
#define ValueUndefined (TagBitTypeOther | (3 << TagTypeShift))
#define ValueLast ValueUndefined

// This special value is never visible to JavaScript code. Empty represents
// Array holes, and uninitialized Values, and maps to NULL pointer.
#define ValueEmpty 0x0u

#ifdef ESCARGOT_64
#define CellPayloadOffset 0
#else
#define CellPayloadOffset PayloadOffset
#endif

// basic class for representing ECMAScript Value
// Int32 convertible double value can exist rarely(for interpreter performance)
// but Int32 convertible double case is not exist with EncodedValue
// so, public API users should not care the case
class Value {
public:
    static constexpr const double MinusZeroIndex = std::numeric_limits<double>::min();
    static constexpr const double UndefinedIndex = std::numeric_limits<double>::max();
#ifdef ESCARGOT_32
    enum { EmptyValueTag = ~ValueEmpty };
    enum { BooleanFalseTag = ~ValueFalse };
    enum { BooleanTrueTag = ~ValueTrue };
    enum { NullTag = ~ValueNull };
    enum { UndefinedTag = ~ValueUndefined };
    enum { LowestTag = UndefinedTag };

    // Any value which last bit is not set
    enum { Int32Tag = 0xfffffffe - 0 };
    enum { OtherPointerTag = 0xfffffffe - 2 };
    enum { ObjectPointerTag = 0xfffffffe - 4 };

    COMPILE_ASSERT((size_t)LowestTag < (size_t)ObjectPointerTag, "");
#endif

    enum NullInitTag { Null };
    enum UndefinedInitTag { Undefined };
    enum EmptyValueInitTag { EmptyValue };
    enum TrueInitTag { True };
    enum FalseInitTag { False };
    enum EncodeAsDoubleTag { EncodeAsDouble };
    enum ForceUninitializedTag { ForceUninitialized };
    enum FromPayloadTag { FromPayload };
    enum FromTagTag { FromTag };

    Value();
    explicit Value(ForceUninitializedTag);
    explicit Value(NullInitTag);
    explicit Value(UndefinedInitTag);
    explicit Value(EmptyValueInitTag);
    explicit Value(TrueInitTag);
    explicit Value(FalseInitTag);
    explicit Value(FromPayloadTag, intptr_t ptr);

    explicit Value(PointerValue* ptr);
    Value(const PointerValue* ptr);
    /*
#ifdef ESCARGOT_64
    explicit Value(PointerValue* ptr);
    Value(const PointerValue* ptr);
#else
    Value(PointerValue* ptr);
    Value(const PointerValue* ptr);
    enum FromNonObjectPointerTag { FromNonObjectPointer };
    Value(const PointerValue* ptr, FromNonObjectPointerTag);
    Value(Object* ptr);
    Value(const Object* ptr);
    Value(String* ptr);
    Value(const String* ptr);
    explicit Value(FromTagTag, uint32_t tag);
#endif
     */
    // Numbers
    Value(EncodeAsDoubleTag, const double&);
    explicit Value(const double&);
    explicit Value(bool);
    explicit Value(char);
    explicit Value(unsigned char);
    explicit Value(short);
    explicit Value(unsigned short);
    explicit Value(int);
    explicit Value(unsigned);
    explicit Value(long);
    explicit Value(unsigned long);
    explicit Value(long long);
    explicit Value(unsigned long long);

    inline bool operator==(const Value& other) const;
    inline bool operator!=(const Value& other) const;

    bool isInt32() const;
    bool isUInt32() const;
    bool isDouble() const;
    bool isTrue() const;
    bool isFalse() const;

    int32_t asInt32() const;
    uint32_t asUInt32() const;
    double asDouble() const;
    bool asBoolean() const;
    double asNumber() const;
    intptr_t asRawData() const;
    inline PointerValue* asPointerValue() const;
    inline Object* asObject() const;
    inline FunctionObject* asFunction() const;
    inline ExtendedNativeFunctionObject* asExtendedNativeFunctionObject() const;
    inline String* asString() const;
    inline Symbol* asSymbol() const;
    inline BigInt* asBigInt() const;

    // Querying the type.
    inline bool isEmpty() const;
    inline bool isFunction() const;
    inline bool isExtendedNativeFunctionObject() const;
    inline bool isUndefined() const;
    inline bool isNull() const;
    inline bool isUndefinedOrNull() const
    {
        return isUndefined() || isNull();
    }
    bool isBoolean() const;
    bool isNumber() const;
    bool isString() const;
    bool isSymbol() const;
    bool isBigInt() const;
    inline bool isPrimitive() const;
    bool isGetterSetter() const;
    bool isCustomGetterSetter() const;
    inline bool isHeapValue() const;
    inline bool isPointerValue() const;
    bool isObject() const;
    bool isCallable() const;
    bool isConstructor() const;

    enum PrimitiveTypeHint { PreferString,
                             PreferNumber,
                             PreferDefault };
    Value toPrimitive(ExecutionState& ec, PrimitiveTypeHint = PreferDefault) const; // $7.1.1 ToPrimitive
    Value ordinaryToPrimitive(ExecutionState& state, PrimitiveTypeHint preferredType) const;
    inline bool toBoolean(ExecutionState& ec) const; // $7.1.2 ToBoolean
    double toNumber(ExecutionState& ec) const; // $7.1.3 ToNumber
    std::pair<Value, bool> /* <Value, isBigInt> */
    toNumeric(ExecutionState& ec) const; // https://www.ecma-international.org/ecma-262/#sec-tonumeric
    double toInteger(ExecutionState& ec) const; // $7.1.4 ToInteger
    bool isInteger(ExecutionState& ec) const; // $7.1.4 ToInteger
    uint64_t toLength(ExecutionState& ec) const;
    int32_t toInt32(ExecutionState& ec) const; // $7.1.5 ToInt32
    uint32_t toUint32(ExecutionState& ec) const; // http://www.ecma-international.org/ecma-262/5.1/#sec-9.6
    String* toString(ExecutionState& ec) const // $7.1.12 ToString
    {
        if (LIKELY(isString())) {
            return asString();
        }
        return toStringSlowCase(ec);
    }
    String* toStringWithoutException(ExecutionState& ec) const;
    Object* toObject(ExecutionState& ec) const; // $7.1.13 ToObject
    BigInt* toBigInt(ExecutionState& ec) const;
    Value toPropertyKey(ExecutionState& state) const;

    enum : uint64_t { InvalidIndexValue = std::numeric_limits<uint64_t>::max() };
    typedef uint64_t ValueIndex;
    ValueIndex toIndex(ExecutionState& ec) const;
    inline ValueIndex tryToUseAsIndex(ExecutionState& ec) const;

    enum : uint32_t {
        InvalidIndex32Value = std::numeric_limits<uint32_t>::max(),
        InvalidIndexPropertyValue = InvalidIndex32Value
    };
    inline uint32_t toIndex32(ExecutionState& ec) const;
    inline uint32_t tryToUseAsIndex32(ExecutionState& ec) const;
    inline uint32_t tryToUseAsIndexProperty(ExecutionState& ec) const;

    inline bool abstractEqualsTo(ExecutionState& ec, const Value& val) const;
    bool abstractEqualsToSlowCase(ExecutionState& ec, const Value& val) const;
    inline bool equalsTo(ExecutionState& ec, const Value& val) const;
    bool equalsToSlowCase(ExecutionState& ec, const Value& val) const;
    bool equalsToByTheSameValueAlgorithm(const Value& val) const;
    bool equalsToByTheSameValueZeroAlgorithm(const Value& val) const;
    bool instanceOf(ExecutionState& ec, const Value& other) const;

    static constexpr double maximumLength();

    static bool isInt32ConvertibleDouble(const double& d);
    static bool isInt32ConvertibleDouble(const double& d, int32_t& asInt32);
private:
    const uint8_t readPointerValueTag() const
    {
        return *(reinterpret_cast<size_t*>(m_data.payload) + 1);
    }
    ValueData m_data;
    double toNumberSlowCase(ExecutionState& ec) const; // $7.1.3 ToNumber
    std::pair<Value, bool> toNumericSlowCase(ExecutionState& ec) const;
    String* toStringSlowCase(ExecutionState& ec) const; // $7.1.12 ToString
    Object* toObjectSlowCase(ExecutionState& ec) const; // $7.1.13 ToObject
    Value toPrimitiveSlowCase(ExecutionState& ec, PrimitiveTypeHint) const; // $7.1.1 ToPrimitive
    bool toBooleanSlowCase(ExecutionState& ec) const; // $7.1.2 ToBoolean
    int32_t toInt32SlowCase(ExecutionState& ec) const; // $7.1.5 ToInt32
    ValueIndex tryToUseAsIndexSlowCase(ExecutionState& ec) const;
    uint32_t tryToUseAsIndex32SlowCase(ExecutionState& ec) const;
#if defined(ESCARGOT_ENABLE_TEST)
    bool checkIfObjectWithIsHTMLDDA() const;
#endif
};

typedef Vector<Value, CustomAllocator<Value>> ValueVector;
typedef VectorWithInlineStorage<32, Value, CustomAllocator<Value>> ValueVectorWithInlineStorage;
typedef VectorWithInlineStorage<32, Value, CustomAllocator<Value>> ValueVectorWithInlineStorage32;
typedef VectorWithInlineStorage<64, Value, CustomAllocator<Value>> ValueVectorWithInlineStorage64;
} // namespace Escargot

namespace std {

template <>
struct is_fundamental<Escargot::Value> : public true_type {
};
} // namespace std

#include "runtime/ValueInlines.h"

#endif
