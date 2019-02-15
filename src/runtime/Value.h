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

#ifndef __EscargotValue__
#define __EscargotValue__

#include "runtime/ExecutionState.h"

namespace Escargot {

class PointerValue;
class String;
class Object;
class FunctionObject;

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

class Value {
public:
#ifdef ESCARGOT_32
    enum { EmptyValueTag = ~ValueEmpty };
    enum { Int32Tag = 0xfffffffe };
    enum { BooleanFalseTag = ~ValueFalse };
    enum { BooleanTrueTag = ~ValueTrue };
    enum { NullTag = ~ValueNull };
    enum { UndefinedTag = ~ValueUndefined };
    enum { OtherPointerTag = ~(TagBitTypeOther | (4 << TagTypeShift)) };
    enum { ObjectPointerTag = ~(TagBitTypeOther | (5 << TagTypeShift)) };
    enum { LowestTag = ObjectPointerTag };
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
#ifdef ESCARGOT_64
    explicit Value(PointerValue* ptr);
    Value(const PointerValue* ptr);
#else
    Value(PointerValue* ptr);
    Value(const PointerValue* ptr);
    Value(Object* ptr);
    Value(const Object* ptr);
    Value(String* ptr);
    Value(const String* ptr);
    explicit Value(FromTagTag, uint32_t tag);
#endif

    // Numbers
    Value(EncodeAsDoubleTag, double);
    explicit Value(double);
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

    inline operator bool() const;
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
    uint64_t asRawData() const;
    inline PointerValue* asPointerValue() const;
    inline Object* asObject() const;
    inline FunctionObject* asFunction() const;
    inline String* asString() const;
    inline Symbol* asSymbol() const;

    // Querying the type.
    inline bool isEmpty() const;
    inline bool isFunction() const;
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
    inline bool isPrimitive() const;
    bool isGetterSetter() const;
    bool isCustomGetterSetter() const;
    inline bool isPointerValue() const;
    bool isObject() const;
    bool isIterable() const;

    enum PrimitiveTypeHint { PreferString,
                             PreferNumber,
                             PreferDefault };
    Value toPrimitive(ExecutionState& ec, PrimitiveTypeHint = PreferNumber) const; // $7.1.1 ToPrimitive
    Value ordinaryToPrimitive(ExecutionState& ec, PrimitiveTypeHint) const;
    inline bool toBoolean(ExecutionState& ec) const; // $7.1.2 ToBoolean
    double toNumber(ExecutionState& ec) const; // $7.1.3 ToNumber
    double toInteger(ExecutionState& ec) const; // $7.1.4 ToInteger
    double toLength(ExecutionState& ec) const;
    int32_t toInt32(ExecutionState& ec) const; // $7.1.5 ToInt32
    uint32_t toUint32(ExecutionState& ec) const; // http://www.ecma-international.org/ecma-262/5.1/#sec-9.6
    String* toString(ExecutionState& ec) const // $7.1.12 ToString
    {
        if (LIKELY(isString())) {
            return asString();
        }
        return toStringSlowCase(ec);
    }
    Object* toObject(ExecutionState& ec) const; // $7.1.13 ToObject
    Value toPropertyKey(ExecutionState& state) const;

    enum : uint32_t { InvalidIndexValue = std::numeric_limits<uint32_t>::max() };
    typedef uint64_t ValueIndex;
    ValueIndex toIndex(ExecutionState& ec) const;
    inline ValueIndex tryToUseAsIndex(ExecutionState& ec) const;
    enum : uint32_t { InvalidArrayIndexValue = std::numeric_limits<uint32_t>::max() };
    inline uint64_t toArrayIndex(ExecutionState& ec) const;
    inline uint32_t tryToUseAsArrayIndex(ExecutionState& ec) const;

    inline bool abstractEqualsTo(ExecutionState& ec, const Value& val) const;
    bool abstractEqualsToSlowCase(ExecutionState& ec, const Value& val) const;
    inline bool equalsTo(ExecutionState& ec, const Value& val) const;
    bool equalsToSlowCase(ExecutionState& ec, const Value& val) const;
    bool equalsToByTheSameValueAlgorithm(ExecutionState& ec, const Value& val) const;
    bool equalsToByTheSameValueZeroAlgorithm(ExecutionState& ec, const Value& val) const;


#ifdef ESCARGOT_32
    uint32_t tag() const;
    int32_t payload() const;
#elif ESCARGOT_64
// These values are #defines since using static const integers here is a ~1% regression!

// This value is 2^48, used to encode doubles such that the encoded value will begin
// with a 16-bit pattern within the range 0x0001..0xFFFE.
#define DoubleEncodeOffset 0x1000000000000ll
// DoubleEncodeOffset assumes that double value will begin with 0x0000..0xFFFD,
// but there can be invalid inputs start with 0xFFFE or 0xFFFF.
// So it is used to filter those values.
#define DoubleInvalidBeginning 0xfffe000000000000ll
// If all bits in the mask are set, this indicates an integer number,
// if any but not all are set this value is a double precision number.
#define TagTypeNumber 0xffff000000000000ll

// TagMask is used to check for all types of immediate values (either number or 'other').
#define TagMask (TagTypeNumber | TagBitTypeOther)

    intptr_t payload() const;
#endif

private:
    ValueDescriptor u;
    double toNumberSlowCase(ExecutionState& ec) const; // $7.1.3 ToNumber
    String* toStringSlowCase(ExecutionState& ec) const; // $7.1.12 ToString
    Object* toObjectSlowCase(ExecutionState& ec) const; // $7.1.13 ToObject
    Value toPrimitiveSlowCase(ExecutionState& ec, PrimitiveTypeHint) const; // $7.1.1 ToPrimitive
    int32_t toInt32SlowCase(ExecutionState& ec) const; // $7.1.5 ToInt32
    ValueIndex tryToUseAsIndexSlowCase(ExecutionState& ec) const;
    uint32_t tryToUseAsArrayIndexSlowCase(ExecutionState& ec) const;
};

typedef Vector<Value, CustomAllocator<Value>> ValueVector;
}

namespace std {

template <>
struct is_fundamental<Escargot::Value> : public true_type {
};
}


#include "runtime/Object.h"
#include "runtime/PointerValue.h"
#include "runtime/String.h"
#include "runtime/ValueInlines.h"

#endif
