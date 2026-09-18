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

class PointerValue;
class ExecutionState;
class Object;
class Symbol;
class String;
class BigInt;
class FunctionObject;
class ExtendedNativeFunctionObject;
class ArrayObject;
class ObjectStructure;
class VMInstance;

union ValueDescriptor {
    int64_t asInt64;
#ifdef ESCARGOT_32
    double asDouble;
#elif ESCARGOT_64
    uintptr_t asPointerBits;
#endif
    struct {
#if defined(ESCARGOT_LITTLE_ENDIAN)
        int32_t payload;
        uint32_t tag;
#else
        // The tag must stay in the high half on both endians: it overlaps the
        // sign/exponent half of asDouble (that is what makes the tag range
        // work), and keeping the halves at fixed positions lets every immediate
        // test be a single 64-bit compare against a valueBits() constant.
        uint32_t tag;
        int32_t payload;
#endif
    } asBits;
};

template <typename ToType, typename FromType>
inline ToType bitwise_cast(FromType from)
{
    ASSERT(sizeof(FromType) == sizeof(ToType));
    union {
        FromType from;
        ToType to;
    } u = { from };
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

// Heap allocations used by Value are at least 8-byte aligned. Object
// pointers stay canonical; less common pointer kinds use an even low-bit tag.
constexpr uintptr_t PointerKindMask = 0x7;
constexpr uintptr_t ObjectPointerKind = 0x0;
constexpr uintptr_t OtherPointerKind = 0x4;
// A dedicated kind avoids a heap type-tag load on every non-object decode.
constexpr uintptr_t NumberPointerKind = 0x2;
constexpr uintptr_t OpaquePointerKind = OtherPointerKind;

#ifdef ESCARGOT_32
// Non-empty immediates share OtherPointerTag with non-object pointers on
// 32-bit builds. These deliberately invalid, aligned payloads are never
// dereferenced; keeping them canonical lets EncodedValue use one Other kind.
constexpr uintptr_t ValueFalsePayload = 0x100;
constexpr uintptr_t ValueTruePayload = 0x108;
constexpr uintptr_t ValueNullPayload = 0x110;
constexpr uintptr_t ValueUndefinedPayload = 0x118;

// Bit 3 selects within a pair (false/true, null/undefined), bit 4 selects the
// pair itself. Masking the don't-care bit out turns every "is it one of these
// payloads" test below into a single compare.
constexpr unsigned ImmediatePayloadSelectShift = 3;
constexpr uintptr_t ImmediatePayloadSelectBit = 1u << ImmediatePayloadSelectShift;
constexpr uintptr_t ImmediatePayloadPairBit = 0x10;
constexpr uintptr_t ImmediatePayloadMask = ImmediatePayloadPairBit | ImmediatePayloadSelectBit;

COMPILE_ASSERT(ValueTruePayload == (ValueFalsePayload | ImmediatePayloadSelectBit), "");
COMPILE_ASSERT(ValueNullPayload == (ValueFalsePayload | ImmediatePayloadPairBit), "");
COMPILE_ASSERT(ValueUndefinedPayload == (ValueNullPayload | ImmediatePayloadSelectBit), "");

ALWAYS_INLINE bool isImmediatePayload(uintptr_t payload)
{
    return (payload & ~ImmediatePayloadMask) == ValueFalsePayload;
}

// True for the null and undefined payloads only. Callers that already know the
// tag is OtherPointerTag use this instead of isUndefinedOrNull() so that the
// answer costs two integer ops on the payload half alone.
ALWAYS_INLINE bool isNullishPayload(uintptr_t payload)
{
    return (payload | ImmediatePayloadSelectBit) == ValueUndefinedPayload;
}

// EncodedValue keeps immediates as tagPointer(payload, OtherPointerKind), so
// the kind bits can be folded into the mask and the untag skipped.
ALWAYS_INLINE bool isEncodedImmediatePayload(uintptr_t bits)
{
    return (bits & ~(ImmediatePayloadMask | PointerKindMask)) == ValueFalsePayload;
}

// The whole bit pattern of a Value, composed from its {tag, payload} pair. The
// tag always occupies the high half (see ValueDescriptor), which turns the
// immediate tests into one 64-bit compare instead of a tag compare followed by
// a payload compare.
constexpr uint64_t valueBits(uint32_t tag, uint32_t payload)
{
    return (static_cast<uint64_t>(tag) << 32) | static_cast<uint64_t>(payload);
}

// True iff the {tag, payload} pair is a double holding NaN: its magnitude bits
// sit above the +Inf pattern. The caller must have established that the tag is
// in the double range -- the immediate tags live in the NaN space too.
ALWAYS_INLINE bool isNaNValueBits(uint32_t tag, uintptr_t payload)
{
    const uint32_t absHigh = tag & 0x7fffffffu;
    return absHigh > 0x7ff00000u || (absHigh == 0x7ff00000u && payload != 0);
}
#endif

ALWAYS_INLINE uintptr_t pointerBits(const void* ptr)
{
    return reinterpret_cast<uintptr_t>(ptr);
}

ALWAYS_INLINE uintptr_t pointerKind(uintptr_t bits)
{
    return bits & PointerKindMask;
}

ALWAYS_INLINE uintptr_t tagPointer(const void* ptr, uintptr_t kind)
{
    const uintptr_t bits = pointerBits(ptr);
    ASSERT((bits & PointerKindMask) == 0);
    ASSERT((kind & ~PointerKindMask) == 0);
    ASSERT((kind & 1) == 0);
    return bits | kind;
}

ALWAYS_INLINE void* untagPointer(uintptr_t bits)
{
    return reinterpret_cast<void*>(bits & ~PointerKindMask);
}

#ifdef ESCARGOT_64
#define CellPayloadOffset 0
#else
#define CellPayloadOffset PayloadOffset
#endif

struct UnconvertibleDoubleToInt32 {
    double value;
    UnconvertibleDoubleToInt32(double&& v);
    operator double() const noexcept { return value; }
};

// basic class for representing ECMAScript Value
// Int32 convertible double value can exist rarely(for interpreter performance)
// but Int32 convertible double case is not exist with EncodedValue
// so, public API users should not care the case
class Value {
public:
    static constexpr const double MinusZeroIndex = std::numeric_limits<double>::min();
    static constexpr const double UndefinedIndex = std::numeric_limits<double>::max();
#ifdef ESCARGOT_32
    /*
     * [32-bit Value Tagging Layout & Remaining Slots]
     * Tag range: [LowestTag (0xfffffff1), 0xffffffff] (total 15 slots)
     * - Any tag < LowestTag is considered a Double value.
     * - Any tag >= LowestTag is an immediate value.
     *
     * Current allocation:
     *   0xffffffff : EmptyValueTag
     *   0xfffffffc : OtherPointerTag (non-object pointers and non-empty immediates)
     *   0xfffffffb : reserved. Keeping it unallocated leaves the two pointer
     *                tags in one unsigned range, so isPointerValue() stays a
     *                single range check.
     *   0xfffffffa : ObjectPointerTag
     *   0xfffffff2 : Int32Tag. Sitting directly on top of the double range
     *                collapses isNumber() into one unsigned compare
     *                (tag() <= Int32Tag), so any tag added later has to stay
     *                above it.
     *   0xfffffff1 : reserved. It falls inside the isNumber() range above, so
     *                it must not be handed to a non-number tag.
     *
     * Remaining slots (9 slots):
     *   0xfffffffe, 0xfffffffd, 0xfffffff9, 0xfffffff8, 0xfffffff7,
     *   0xfffffff6, 0xfffffff5, 0xfffffff4, 0xfffffff3
     */
    enum : uint32_t { EmptyValueTag = ~ValueEmpty };
    enum : uint32_t { LowestTag = 0xfffffff1 };

    // Any value which last bit is not set
    enum { Int32Tag = 0xfffffffe - 12 };
    enum { OtherPointerTag = 0xfffffffe - 2 };
    enum { ObjectPointerTag = 0xfffffffe - 4 };

    COMPILE_ASSERT(static_cast<uint32_t>(ObjectPointerTag) + (OtherPointerKind >> 1) == static_cast<uint32_t>(OtherPointerTag), "");

    COMPILE_ASSERT((size_t)LowestTag < (size_t)ObjectPointerTag, "");
    // isNumber() relies on Int32Tag being the lowest immediate tag.
    COMPILE_ASSERT((size_t)LowestTag <= (size_t)Int32Tag, "");
    COMPILE_ASSERT((size_t)Int32Tag < (size_t)ObjectPointerTag, "");

    // Distance between the two pointer tags. It doubles as the tag -> pointer
    // kind conversion factor (OtherPointerKind >> 1), which keeps both the
    // isPointerValue() range check and the EncodedValue store path branch-free.
    enum : uint32_t { PointerTagSpan = static_cast<uint32_t>(OtherPointerTag) - static_cast<uint32_t>(ObjectPointerTag) };
    COMPILE_ASSERT(PointerTagSpan << 1 == OtherPointerKind, "");

    // Whole-Value bit patterns of the immediates, so that testing for one is a
    // single 64-bit compare.
    enum : uint64_t {
        UndefinedValueBits = valueBits(OtherPointerTag, ValueUndefinedPayload),
        NullValueBits = valueBits(OtherPointerTag, ValueNullPayload),
        TrueValueBits = valueBits(OtherPointerTag, ValueTruePayload),
        FalseValueBits = valueBits(OtherPointerTag, ValueFalsePayload),
        // Bit 3 of the payload selects within a pair (false/true, null/undefined).
        ImmediateSelectValueBit = valueBits(0, ImmediatePayloadSelectBit),
        // Tag plus the sign bit of the payload: an int32 is a uint32 iff masking
        // with this yields the bare Int32Tag.
        UInt32ValueBitsMask = valueBits(0xffffffffu, 0x80000000u),
        UInt32ValueBits = valueBits(Int32Tag, 0),
    };

    COMPILE_ASSERT(TrueValueBits == (FalseValueBits | ImmediateSelectValueBit), "");
    COMPILE_ASSERT(UndefinedValueBits == (NullValueBits | ImmediateSelectValueBit), "");
#endif

    enum NullInitTag { Null };
    enum UndefinedInitTag { Undefined };
    enum EmptyValueInitTag { EmptyValue };
    enum TrueInitTag { True };
    enum FalseInitTag { False };
    enum EncodeAsDoubleTag { EncodeAsDouble };
    enum ForceUninitializedTag { ForceUninitialized };
    // The argument must be the base of an 8-byte-aligned BDWGC allocation.
    enum FromPayloadTag { FromPayload };
    enum FromEncodedPayloadTag { FromEncodedPayload };
#ifdef ESCARGOT_32
    // The argument is an already-canonical, non-null object payload.
    enum FromObjectEncodedPayloadTag { FromObjectEncodedPayload };
    // The argument carries OtherPointerKind; skips recomputing the kind when
    // the caller has already established it.
    enum FromOtherEncodedPayloadTag { FromOtherEncodedPayload };
#endif

    Value();
    explicit Value(ForceUninitializedTag);
    explicit Value(NullInitTag);
    explicit Value(UndefinedInitTag);
    explicit Value(EmptyValueInitTag);
    explicit Value(TrueInitTag);
    explicit Value(FalseInitTag);
    explicit Value(FromPayloadTag, intptr_t ptr);
    explicit Value(FromEncodedPayloadTag, intptr_t bits);
#ifdef ESCARGOT_32
    explicit Value(FromObjectEncodedPayloadTag, intptr_t bits);
    explicit Value(FromOtherEncodedPayloadTag, intptr_t bits);
#endif
    Value(PointerValue* ptr);
    Value(const PointerValue* ptr);
    Value(Object* ptr);
    Value(const Object* ptr);
    Value(String* ptr);
    Value(const String* ptr);
    Value(Symbol* ptr);
    Value(const Symbol* ptr);
    Value(BigInt* ptr);
    Value(const BigInt* ptr);
    template <typename T, typename std::enable_if<std::is_convertible<T*, PointerValue*>::value && !std::is_same<PointerValue, typename std::remove_cv<T>::type>::value, int>::type = 0>
    Value(T* ptr);

    // Numbers
    Value(EncodeAsDoubleTag, const double&);
    enum NaNInitTag { NanInit };
    explicit Value(NaNInitTag);
    enum PostiveInfinityInitTag { PostiveInfinityInit };
    enum NegativeInfinityInitTag { NegativeInfinityInit };
    explicit Value(PostiveInfinityInitTag);
    explicit Value(NegativeInfinityInitTag);

    enum DoubleToIntConvertibleTestNeedsTag { DoubleToIntConvertibleTestNeeds };
    // if you want to init Value with constant double value,
    // please use Value(UnconvertibleDoubleToInt32) or Value(int32_t)
    explicit Value(DoubleToIntConvertibleTestNeedsTag, double);
    // You can use this function with only !isInt32ConvertibleDouble value
    explicit Value(UnconvertibleDoubleToInt32 v)
        : Value(EncodeAsDouble, v.value)
    {
    }
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
    // True iff this is a double holding NaN. Decided from the raw bits, never
    // through asDouble(): callers that only need the NaN answer must not be
    // forced to touch an FP register (see equalsTo()).
    bool isNaNDouble() const;
    bool isTrue() const;
    bool isFalse() const;

    int32_t asInt32() const;
    uint32_t asUInt32() const;
    double asDouble() const;
    bool asBoolean() const;
    double asNumber() const;
    uint64_t asRawData() const;
    inline PointerValue* asPointerValue() const;
    inline void* asOpaquePointer() const;
    inline Object* asObject() const;
    inline FunctionObject* asFunctionObject() const;
    inline ExtendedNativeFunctionObject* asExtendedNativeFunctionObject() const;
    inline ArrayObject* asArrayObject() const;
    inline String* asString() const;
    inline Symbol* asSymbol() const;
    inline BigInt* asBigInt() const;

    // Querying the type.
    inline bool isEmpty() const;
    inline bool isFunctionObject() const;
    inline bool isExtendedNativeFunctionObject() const;
    inline bool isArrayObject() const;
    inline bool isUndefined() const;
    inline bool isNull() const;
    inline bool isUndefinedOrNull() const;
    bool isBoolean() const;
    bool isNumber() const;
    bool isString() const;
    bool isSymbol() const;
    bool isBigInt() const;
    inline bool isPrimitive() const;
    bool isGetterSetter() const;
    bool isCustomGetterSetter() const;
    inline bool isPointerValue() const;
    inline bool isOpaquePointer() const;
#ifdef ESCARGOT_64
    // Pointer stored with OtherPointerKind: String, Symbol, BigInt or an opaque
    // pointer. Never an Object, and never an immediate.
    bool hasOtherPointerKind() const;
#endif
    bool isObject() const;
    bool isCallable() const;
    bool isConstructor() const;
    bool canBeHeldWeakly(VMInstance* vm) const;

    enum PrimitiveTypeHint { PreferString,
                             PreferNumber,
                             PreferDefault };
    Value toPrimitive(ExecutionState& ec, PrimitiveTypeHint = PreferDefault) const; // $7.1.1 ToPrimitive
    Value ordinaryToPrimitive(ExecutionState& state, PrimitiveTypeHint preferredType) const;
    inline bool toBoolean() const; // $7.1.2 ToBoolean
    double toNumber(ExecutionState& ec) const; // $7.1.3 ToNumber
    std::pair<Value, bool> /* <Value, isBigInt> */
    toNumeric(ExecutionState& ec) const; // https://www.ecma-international.org/ecma-262/#sec-tonumeric
    double toInteger(ExecutionState& ec) const; // $7.1.4 ToInteger
    double toIntegerIfIntergral(ExecutionState& state) const; // https://tc39.es/ecma402/#sec-tointegerifintegral
    int64_t toIntegerWithTruncation(ExecutionState& state) const; // https://tc39.es/proposal-temporal/#sec-tointegerwithtruncation
    int64_t toPostiveIntegerWithTruncation(ExecutionState& state) const; // https://tc39.es/proposal-temporal/#sec-topositiveintegerwithtruncation
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

    // https://tc39.es/ecma262/multipage/keyed-collections.html#sec-canonicalizekeyedcollectionkey
    Value toCanonicalizeKeyedCollectionKey(ExecutionState& state) const;

    inline bool abstractEqualsTo(ExecutionState& ec, const Value& val) const;
    bool abstractEqualsToSlowCase(ExecutionState& ec, const Value& val) const;
    inline bool equalsTo(ExecutionState& ec, const Value& val) const;
    bool equalsToSlowCase(ExecutionState& ec, const Value& val) const;
    // Both sides are numbers but not bitwise identical. Kept out of line
    // because it is the only part of equalsTo()/abstractEqualsTo() that needs
    // an FP register; inlining it makes the compiler hold an FP image of both
    // operands across the whole function.
    NEVER_INLINE bool numberEqualsToSlowCase(const Value& val) const;
    bool equalsToByTheSameValueAlgorithm(ExecutionState& ec, const Value& val) const;
    inline bool equalsToByTheSameValueZeroAlgorithm(ExecutionState& ec, const Value& val) const;
    bool equalsToByTheSameValueZeroAlgorithmSlowCase(ExecutionState& ec, const Value& val) const;
    bool instanceOf(ExecutionState& ec, const Value& other) const;


#ifdef ESCARGOT_32
    uint32_t tag() const;
#elif ESCARGOT_64
    /*
     * [64-bit Value Tagging Layout & Remaining Slots]
     * Format:
     * - If high 16 bits are 0x0000:
     *   - If bit 1 (TagBitTypeOther = 0x2) is 0: standard 48-bit pointer.
     *   - If bit 1 is 1: Immediate/other value of form TagBitTypeOther | (X << TagTypeShift) (where TagTypeShift = 2)
     *
     * Current allocation:
     *   X = 0 (ValueFalse, value 0x2)
     *   X = 1 (ValueTrue, value 0x6)
     *   X = 2 (ValueNull, value 0xa)
     *   X = 3 (ValueUndefined, value 0xe)
     *   (ValueEmpty is 0x0)
     *
     * Remaining slots:
     *   X can be any value in range [4, 2^46 - 1].
     *   This yields 2^46 - 4 (approx. 70 trillion) remaining slots.
     */
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
// Shift that moves the number tag down to the low 16 bits. The number
// predicates read the tag as `bits >> NumberTagShift` instead of masking with
// TagTypeNumber so that no 64-bit immediate is needed.
#define NumberTagShift 48

// TagMask is used to check for all types of immediate values (either number or 'other').
#define TagMask (TagTypeNumber | TagBitTypeOther)
#endif

    // Raw Value bit pattern. Untag before dereferencing a stored pointer.
    intptr_t payload() const;
    intptr_t rawPayload() const;
    static constexpr double maximumLength();

    ATTRIBUTE_NO_SANITIZE_FLOAT_CAST_OVERFLOW static bool isInt32ConvertibleDouble(const double& d);
    ATTRIBUTE_NO_SANITIZE_FLOAT_CAST_OVERFLOW static bool isInt32ConvertibleDouble(const double& d, int32_t& asInt32);

    // Shared helper for the two common "unsafe by the letter of the
    // standard, safe by construction" truncation patterns seen across the
    // codebase: (1) truncate then range-check against small bounds (e.g.
    // fractionDigits-style [0, 100] arguments), where a NaN/Infinity/huge
    // input truncates to some fixed hardware sentinel that always fails
    // the check; (2) speculatively truncate as a fast path and compare
    // the result back against the original double, falling back to a
    // slower, well-defined conversion on mismatch. Either way d can be
    // out of int32_t's representable range, which is UB for a plain
    // static_cast even though x86 hardware truncates it to a fixed
    // sentinel (INT32_MIN) instead of trapping -- isolate that cast here
    // instead of adding a redundant explicit check at every call site.
    ATTRIBUTE_NO_SANITIZE_FLOAT_CAST_OVERFLOW static int32_t truncateDoubleToInt32Unchecked(double d);

private:
    ValueDescriptor u;
    double toNumberSlowCase(ExecutionState& ec) const; // $7.1.3 ToNumber
    std::pair<Value, bool> toNumericSlowCase(ExecutionState& ec) const;
    String* toStringSlowCase(ExecutionState& ec) const; // $7.1.12 ToString
    Object* toObjectSlowCase(ExecutionState& ec) const; // $7.1.13 ToObject
    Value toPrimitiveSlowCase(ExecutionState& ec, PrimitiveTypeHint) const; // $7.1.1 ToPrimitive
    bool toBooleanSlowCase() const; // $7.1.2 ToBoolean
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
