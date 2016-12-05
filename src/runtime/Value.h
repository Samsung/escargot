/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2007, 2008, 2012 Apple Inc. All rights reserved.
 *  Copyright (C) 2016 Samsung Electronics Co., Ltd
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
        int32_t tag;
    } asBits;
};

template<typename ToType, typename FromType>
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

#ifdef ESCARGOT_64
#define CellPayloadOffset 0
#else
#define CellPayloadOffset PayloadOffset
#endif

class Value {
public:
#ifdef ESCARGOT_32
    enum { Int32Tag =        0xffffffff };
    enum { BooleanTag =      0xfffffffe };
    enum { NullTag =         0xfffffffd };
    enum { UndefinedTag =    0xfffffffc };
    enum { PointerTag =      0xfffffffb };
    enum { EmptyValueTag =   0xfffffffa };
    enum { DeletedValueTag = 0xfffffff9 };
    enum { LowestTag =  DeletedValueTag };
#endif

    enum NullInitTag { Null };
    enum UndefinedInitTag { Undefined };
    enum EmptyValueInitTag { EmptyValue };
    enum DeletedValueInitTag { DeletedValue };
    enum TrueInitTag { True };
    enum FalseInitTag { False };
    enum EncodeAsDoubleTag { EncodeAsDouble };
    enum ForceUninitializedTag { ForceUninitialized };

    Value();
    Value(ForceUninitializedTag);
    Value(NullInitTag);
    Value(UndefinedInitTag);
    Value(EmptyValueInitTag);
    Value(DeletedValueInitTag);
    Value(TrueInitTag);
    Value(FalseInitTag);
    Value(PointerValue* ptr);
    Value(const PointerValue* ptr);

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

    // Querying the type.
    inline bool isEmpty() const;
    inline bool isDeleted() const;
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

    enum PrimitiveTypeHint { PreferString, PreferNumber };
    Value toPrimitive(ExecutionState& ec, PrimitiveTypeHint = PreferNumber) const; // $7.1.1 ToPrimitive
    inline bool toBoolean(ExecutionState& ec) const; // $7.1.2 ToBoolean
    double toNumber(ExecutionState& ec) const; // $7.1.3 ToNumber
    double toInteger(ExecutionState& ec) const; // $7.1.4 ToInteger
    int32_t toInt32(ExecutionState& ec) const; // $7.1.5 ToInt32
    uint32_t toUint32(ExecutionState& ec) const; // http://www.ecma-international.org/ecma-262/5.1/#sec-9.6
    String* toString(ExecutionState& ec) const // $7.1.12 ToString
    {
        if (isString()) {
            return asString();
        }
        return toStringSlowCase(ec);
    }
    Object* toObject(ExecutionState& ec) const; // $7.1.13 ToObject
    Object* toTransientObject(ExecutionState& ec) const; // ES5 $8.7.2 transient object
    Object* toTransientObjectSlowPath(ExecutionState& ec) const; // ES5 $8.7.2 transient object
    double toLength(ExecutionState& ec) const; // $7.1.15 ToLength

    inline bool abstractEqualsTo(ExecutionState& ec, const Value& val) const;
    bool abstractEqualsToSlowCase(ExecutionState& ec, const Value& val) const;
    bool equalsTo(ExecutionState& ec, const Value& val) const;
    inline bool equalsToByTheSameValueAlgorithm(ExecutionState& ec, const Value& val) const;


#ifdef ESCARGOT_32
    uint32_t tag() const;
    int32_t payload() const;
#elif ESCARGOT_64
    // These values are #defines since using static const integers here is a ~1% regression!

    // This value is 2^48, used to encode doubles such that the encoded value will begin
    // with a 16-bit pattern within the range 0x0001..0xFFFE.
#define DoubleEncodeOffset 0x1000000000000ll
    // If all bits in the mask are set, this indicates an integer number,
    // if any but not all are set this value is a double precision number.
#define TagTypeNumber 0xffff000000000000ll

    // All non-numeric (bool, null, undefined) immediates have bit 2 set.
#define TagBitTypeOther 0x2ll
#define TagBitBool      0x4ll
#define TagBitUndefined 0x8ll
    // Combined integer value for non-numeric immediates.
#define ValueFalse     (TagBitTypeOther | TagBitBool | false)
#define ValueTrue      (TagBitTypeOther | TagBitBool | true)
#define ValueUndefined (TagBitTypeOther | TagBitUndefined)
#define ValueNull      (TagBitTypeOther)

    // TagMask is used to check for all types of immediate values (either number or 'other').
#define TagMask (TagTypeNumber | TagBitTypeOther)

    // These special values are never visible to JavaScript code; Empty is used to represent
    // Array holes, and for uninitialized ESValues. Deleted is used in hash table code.
    // These values would map to cell types in the ESValue encoding, but not valid GC cell
    // pointer should have either of these values (Empty is null, deleted is at an invalid
    // alignment for a GC cell, and in the zero page).
#define ValueEmpty   0x0ll
#define ValueDeleted 0x4ll
#endif

private:
    ValueDescriptor u;

    double toNumberSlowCase(ExecutionState& ec) const; // $7.1.3 ToNumber
    String* toStringSlowCase(ExecutionState& ec) const; // $7.1.12 ToString
    Object* toObjectSlowCase(ExecutionState& ec) const; // $7.1.13 ToObject
    Value toPrimitiveSlowCase(ExecutionState& ec, PrimitiveTypeHint) const; // $7.1.1 ToPrimitive
    int32_t toInt32SlowCase(ExecutionState& ec) const; // $7.1.5 ToInt32
};

}

#include "runtime/PointerValue.h"
#include "runtime/String.h"
#include "runtime/Object.h"
#include "runtime/ValueInlines.h"

namespace Escargot {
typedef Vector<Value, gc_malloc_ignore_off_page_allocator<Value>> ValueVector;
}

#endif
