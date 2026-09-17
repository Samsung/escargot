// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
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

#ifndef __EscargotEncodedValue__
#define __EscargotEncodedValue__

#include "runtime/EncodedValueData.h"
#include "runtime/Value.h"
#include "util/Vector.h"
#include "util/TightVector.h"

namespace Escargot {

class PointerValue;

#ifdef ESCARGOT_32
COMPILE_ASSERT(sizeof(EncodedValueData) == 4, "");
COMPILE_ASSERT(sizeof(Value) == 8, "");
#else
COMPILE_ASSERT(sizeof(EncodedValueData) == 8, "");
#endif

#pragma pack(push, 1)
// NumberInEncodedValue stores its tag in `this + sizeof(size_t)`
// the location is same with PointerValues
// store double
class NumberInEncodedValue : public gc {
public:
    explicit NumberInEncodedValue(const Value& v)
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

    Value value() const
    {
#ifdef ESCARGOT_32
        // Both words are overwritten right away; the default ctor would only
        // store an undefined pattern for nothing. (Word-wise copy, so this is
        // endian independent.)
        Value ret(Value::ForceUninitialized);
        uint32_t* buf = reinterpret_cast<uint32_t*>(&ret);
        buf[0] = m_buffer[0];
        buf[1] = m_buffer[2];
        return ret;
#else
        return m_value;
#endif
    }

    void setValue(const Value& v)
    {
        ASSERT(v.isNumber());
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
    Value m_value;
    size_t m_typeTag;
#endif
};
COMPILE_ASSERT(sizeof(NumberInEncodedValue) >= sizeof(size_t) * 2, "NumberInEncodedValue must contain the type-tag word read at offset sizeof(size_t)");
#pragma pack(pop)

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


// EncodedValue turns int, double values into pointer or odd value
// so there is no conservative gc leak(there is no even value looks like pointer without pointers)
// developers should use this class if want to save some Value on Heap
// developers should not copy this value because this class changes NumberInEncodedValue without copy it
// just convert into Value and use it.
class EncodedValue {
public:
    enum ForceUninitializedTag { ForceUninitialized };
    enum EmptyValueInitTag { EmptyValue };
    COMPILE_ASSERT(EncodedValue::EmptyValue == 0, "");
    friend class EncodedSmallValue;

    EncodedValue(ForceUninitializedTag)
    {
    }

    EncodedValue(EmptyValueInitTag)
    {
        m_data.payload = (intptr_t)(ValueEmpty);
    }

    EncodedValue()
    {
#ifdef ESCARGOT_32
        m_data.payload = static_cast<intptr_t>(tagPointer(reinterpret_cast<void*>(ValueUndefinedPayload), OtherPointerKind));
#else
        m_data.payload = (intptr_t)(ValueUndefined);
#endif
    }

    explicit EncodedValue(const uint32_t from)
    {
        if (LIKELY(EncodedValueImpl::PlatformSmiTagging::IsValidSmi(from))) {
            m_data.payload = EncodedValueImpl::PlatformSmiTagging::IntToSmi(from);
        } else {
            fromValueForCtor(Value(from));
        }
    }

    EncodedValue(const Value& from)
    {
        fromValueForCtor(from);
    }

    explicit EncodedValue(PointerValue* v)
    {
        fromValueForCtor(Value(v));
    }

    template <typename T, typename std::enable_if<std::is_convertible<T*, PointerValue*>::value && !std::is_same<PointerValue, typename std::remove_cv<T>::type>::value, int>::type = 0>
    explicit EncodedValue(T* v)
    {
        fromValueForCtor(Value(v));
    }

    bool isUndefined() const
    {
#ifdef ESCARGOT_32
        return m_data.payload == static_cast<intptr_t>(tagPointer(reinterpret_cast<void*>(ValueUndefinedPayload), OtherPointerKind));
#else
        return m_data.payload == (intptr_t)(ValueUndefined);
#endif
    }

    bool isStoredInHeap() const
    {
        if (HAS_SMI_TAG(m_data.payload)) {
            return false;
        }

#ifdef ESCARGOT_32
        return m_data.payload != ValueEmpty && !isEncodedImmediatePayload(static_cast<uintptr_t>(m_data.payload));
#else
        PointerValue* v = (PointerValue*)m_data.payload;
        return ((size_t)v) > ValueLast;
#endif
    }

    intptr_t payload() const
    {
        return m_data.payload;
    }

    static EncodedValue fromPayload(const void* p)
    {
        return EncodedValue(EncodedValueData((void*)p));
    }

    bool isEmpty() const
    {
        return m_data.payload == ValueEmpty;
    }

    // fast path for Set/Map storage scans: compares against a Value without
    // materializing `this` into a full Value first when this slot is
    // SMI-tagged and the search key is also int32 -- skips the toValue()
    // conversion (SmiToInt + Value construction) on the common small-integer
    // key case. Falls back to the general algorithm otherwise.
    ALWAYS_INLINE bool equalsToByTheSameValueZeroAlgorithm(ExecutionState& state, const Value& other) const
    {
        if (LIKELY(HAS_SMI_TAG(m_data.payload) && other.isInt32())) {
            return EncodedValueImpl::PlatformSmiTagging::SmiToInt(m_data.payload) == other.asInt32();
        }
        return toValue().equalsToByTheSameValueZeroAlgorithm(state, other);
    }

    ALWAYS_INLINE bool equalsToByTheSameValueZeroAlgorithm(ExecutionState& state, const EncodedValue& other) const
    {
        if (LIKELY(HAS_SMI_TAG(m_data.payload) && HAS_SMI_TAG(other.m_data.payload))) {
            return m_data.payload == other.m_data.payload;
        }
        return toValue().equalsToByTheSameValueZeroAlgorithm(state, other.toValue());
    }

    template <const bool shouldTreatEmptyAsUndefined = false, const bool checkEmpty = true>
    ALWAYS_INLINE Value toValue() const
    {
        const uintptr_t bits = static_cast<uintptr_t>(m_data.payload);
#ifdef ESCARGOT_32
        const uintptr_t kind = pointerKind(bits);
        if (LIKELY(kind == ObjectPointerKind)) {
            if (checkEmpty && UNLIKELY(bits == ValueEmpty)) {
                if (shouldTreatEmptyAsUndefined)
                    return Value();
                return Value(Value::EmptyValue);
            }
            return Value(Value::FromObjectEncodedPayload, static_cast<intptr_t>(bits));
        }

        if (LIKELY(HAS_SMI_TAG(bits))) {
            return Value(EncodedValueImpl::PlatformSmiTagging::SmiToInt(bits));
        }

        if (UNLIKELY(kind == NumberPointerKind)) {
            return reinterpret_cast<NumberInEncodedValue*>(untagPointer(bits))->value();
        }
        ASSERT(kind == OtherPointerKind);
        return Value(Value::FromOtherEncodedPayload, static_cast<intptr_t>(bits));
#else
        if (LIKELY(HAS_SMI_TAG(bits))) {
            return Value(EncodedValueImpl::PlatformSmiTagging::SmiToInt(bits));
        }

        // ValueFalse/ValueNull happen to carry NumberPointerKind, which is why
        // the range test is needed at all -- but only inside this branch, so the
        // dominant pointer path pays one test instead of two.
        if (UNLIKELY(pointerKind(bits) == NumberPointerKind) && LIKELY(bits > ValueLast)) {
            return reinterpret_cast<NumberInEncodedValue*>(untagPointer(bits))->value();
        }

        // Empty decodes to the very same bit pattern through FromEncodedPayload,
        // so it only has to be recognised when the caller wants undefined.
        if (shouldTreatEmptyAsUndefined && checkEmpty && UNLIKELY(bits == ValueEmpty)) {
            return Value();
        }
        ASSERT(bits <= ValueLast || pointerKind(bits) == ObjectPointerKind || pointerKind(bits) == OtherPointerKind);
        return Value(Value::FromEncodedPayload, static_cast<intptr_t>(bits));
#endif
    }

    // Only for structure-guarded property IC slots that cannot contain Empty.
    ALWAYS_INLINE Value toValueKnownNotEmpty() const
    {
        ASSERT(!isEmpty());
        return toValue<false, false>();
    }

    ALWAYS_INLINE operator Value() const
    {
        return toValue<>();
    }

    // const: a const slot has to be able to take the cheap int32 path instead of
    // being forced through toValue().
    bool isInt32() const
    {
        return HAS_SMI_TAG(m_data.payload);
    }

    bool isUInt32() const
    {
        // Note. use only 31 bits to represent unsigned integer value.
        // Its because we just store signed integer value.
        return isInt32() && asInt32() >= 0;
    }

    int32_t asInt32() const
    {
        ASSERT(HAS_SMI_TAG(m_data.payload));
        return EncodedValueImpl::PlatformSmiTagging::SmiToInt(m_data.payload);
    }

    uint32_t asUInt32() const
    {
        ASSERT(HAS_SMI_TAG(m_data.payload));
        int32_t value = EncodedValueImpl::PlatformSmiTagging::SmiToInt(m_data.payload);
        return (uint32_t)value;
    }

    uint32_t toUInt32(ExecutionState& state) const
    {
        if (LIKELY(HAS_SMI_TAG(m_data.payload))) {
            int32_t value = EncodedValueImpl::PlatformSmiTagging::SmiToInt(m_data.payload);
            return (uint32_t)value;
        }

        return operator Escargot::Value().toUint32(state);
    }

    const EncodedValue& operator=(PointerValue* from)
    {
        return operator=(Value(from));
    }

    template <typename T, typename std::enable_if<std::is_convertible<T*, PointerValue*>::value && !std::is_same<PointerValue, typename std::remove_cv<T>::type>::value, int>::type = 0>
    const EncodedValue& operator=(T* from)
    {
        return operator=(Value(from));
    }

    bool operator==(const EncodedValue& other) const
    {
        return m_data.payload == other.payload();
    }

    ALWAYS_INLINE const EncodedValue& operator=(const Value& from)
    {
        if (LIKELY(storeNonNumber(from))) {
            return *this;
        }

        // Only numbers are left.
        int32_t i32;
        if (from.isInt32() && EncodedValueImpl::PlatformSmiTagging::IsValidSmi(i32 = from.asInt32())) {
            m_data.payload = EncodedValueImpl::PlatformSmiTagging::IntToSmi(i32);
            return *this;
        }

        Value mutableFrom(from);
        if (UNLIKELY(Value::isInt32ConvertibleDouble(mutableFrom.asNumber(), i32))) {
            mutableFrom = Value(i32);
        }
        const intptr_t payload = m_data.payload;

        if (!HAS_SMI_TAG(payload) && ((size_t)payload > (size_t)ValueLast)) {
            const uintptr_t bits = static_cast<uintptr_t>(payload);
            if (pointerKind(bits) == NumberPointerKind) {
                reinterpret_cast<NumberInEncodedValue*>(untagPointer(bits))->setValue(mutableFrom);
                return *this;
            }
        }
        m_data.payload = static_cast<intptr_t>(tagPointer(new NumberInEncodedValue(mutableFrom), NumberPointerKind));
        return *this;
    }

protected:
    // Stores everything that is not a number -- pointers, opaque pointers,
    // immediates and Empty -- and reports whether it did. Splitting it out this
    // way keeps the four store paths (this class and EncodedSmallValue, ctor and
    // assignment) in sync.
    ALWAYS_INLINE bool storeNonNumber(const Value& from)
    {
#ifdef ESCARGOT_32
        // The two pointer tags are adjacent and their pointer kinds are 0 and 4,
        // so tag -> kind is one subtract and one shift. Immediates ride along:
        // they live under OtherPointerTag and get stored kind-tagged, which is
        // exactly what toValue() expects to find.
        const uint32_t tagDelta = from.tag() - static_cast<uint32_t>(Value::ObjectPointerTag);
        if (LIKELY(tagDelta <= static_cast<uint32_t>(Value::PointerTagSpan))) {
            ASSERT((static_cast<uintptr_t>(from.rawPayload()) & PointerKindMask) == 0);
            m_data.payload = from.rawPayload() | static_cast<intptr_t>(tagDelta << 1);
            return true;
        }
        if (from.isNumber()) {
            return false;
        }
        // No other tag exists outside the pointer range.
        ASSERT(from.isEmpty());
        m_data.payload = ValueEmpty;
        return true;
#else
        // Pointers, immediates and Empty are all stored as the raw bit pattern,
        // so a single test covers them. (isOpaquePointer() used to be OR-ed into
        // the pointer test here: OpaquePointerKind is OtherPointerKind, so
        // isPointerValue() already answered true for it.)
        if (LIKELY(!from.isNumber())) {
            m_data.payload = from.rawPayload();
            return true;
        }
        return false;
#endif
    }

    void fromValueForCtor(const Value& from)
    {
        if (LIKELY(storeNonNumber(from))) {
            return;
        }

        int32_t i32;
        if (from.isInt32() && EncodedValueImpl::PlatformSmiTagging::IsValidSmi(i32 = from.asInt32())) {
            m_data.payload = EncodedValueImpl::PlatformSmiTagging::IntToSmi(i32);
        } else if (UNLIKELY(Value::isInt32ConvertibleDouble(from.asNumber(), i32) && EncodedValueImpl::PlatformSmiTagging::IsValidSmi(i32))) {
            m_data.payload = EncodedValueImpl::PlatformSmiTagging::IntToSmi(i32);
        } else {
            m_data.payload = static_cast<intptr_t>(tagPointer(new NumberInEncodedValue(from), NumberPointerKind));
        }
    }

    explicit EncodedValue(EncodedValueData v)
        : m_data(v)
    {
    }

private:
    EncodedValueData m_data;
};

#if defined(ESCARGOT_64) && defined(ESCARGOT_USE_32BIT_IN_64BIT)
class EncodedSmallValue {
public:
    enum EmptyValueInitTag { EmptyValue };
    COMPILE_ASSERT(EncodedSmallValue::EmptyValue == 0, "");

    EncodedSmallValue(EmptyValueInitTag)
    {
        setPayload((intptr_t)(ValueEmpty));
    }

    EncodedSmallValue()
    {
        setPayload((intptr_t)(ValueUndefined));
    }

    EncodedSmallValue(const Value& from)
    {
        if (LIKELY(storeNonNumber(from))) {
            return;
        }

        int32_t i32;
        if (from.isInt32() && EncodedValueImpl::PlatformSmiTagging::IsValidSmi(i32 = from.asInt32())) {
            setPayload(EncodedValueImpl::PlatformSmiTagging::IntToSmi(i32));
        } else if (UNLIKELY(Value::isInt32ConvertibleDouble(from.asNumber(), i32) && EncodedValueImpl::PlatformSmiTagging::IsValidSmi(i32))) {
            setPayload(EncodedValueImpl::PlatformSmiTagging::IntToSmi(i32));
        } else {
            setPayload(static_cast<intptr_t>(tagPointer(new NumberInEncodedValue(from), NumberPointerKind)));
        }
    }

    EncodedSmallValue(const EncodedValue& from)
    {
        setPayload(from.payload());
    }

    ALWAYS_INLINE operator EncodedValue() const
    {
        return EncodedValue::fromPayload(reinterpret_cast<void*>(payload()));
    }

    static EncodedSmallValue fromPayload(void* p)
    {
        EncodedSmallValue v;
        v.setPayload(reinterpret_cast<intptr_t>(p));
        return v;
    }

    intptr_t payload() const
    {
        // A SMI payload carries a sign; anything else is a 32-bit heap address
        // or immediate that has to be zero extended. Branchless: widen the sign
        // only when the slot is SMI-tagged *and* negative.
        const uint32_t raw = static_cast<uint32_t>(m_data.payload);
        const uint64_t signBit = static_cast<uint64_t>(raw & EncodedValueImpl::kSmiTag) & static_cast<uint64_t>(raw >> 31);
        return static_cast<intptr_t>(static_cast<uint64_t>(raw) | ((0ull - signBit) << 32));
    }

    template <const bool shouldTreatEmptyAsUndefined = false, const bool checkEmpty = true>
    ALWAYS_INLINE Value toValue() const
    {
        // Read the slot once and test the SMI tag once: going through payload()
        // would branch on that very bit and then have it re-tested here.
        const int32_t raw = m_data.payload;
        if (LIKELY(raw & EncodedValueImpl::kSmiTag)) {
            // Arithmetic shift, so a negative SMI keeps its sign.
            return Value(static_cast<int>(raw >> 1));
        }

        const uintptr_t bits = static_cast<uint32_t>(raw);
        // See EncodedValue::toValue(): the range test only matters for the two
        // immediates that share NumberPointerKind.
        if (UNLIKELY(pointerKind(bits) == NumberPointerKind) && LIKELY(bits > ValueLast)) {
            return reinterpret_cast<NumberInEncodedValue*>(untagPointer(bits))->value();
        }

        if (shouldTreatEmptyAsUndefined && checkEmpty && UNLIKELY(bits == ValueEmpty)) {
            return Value();
        }
        ASSERT(bits <= ValueLast || pointerKind(bits) == ObjectPointerKind || pointerKind(bits) == OtherPointerKind);
        return Value(Value::FromEncodedPayload, static_cast<intptr_t>(bits));
    }

    ALWAYS_INLINE Value toValueKnownNotEmpty() const
    {
        ASSERT(!isEmpty());
        return toValue<false, false>();
    }

    ALWAYS_INLINE operator Value() const
    {
        return toValue<>();
    }

    bool isEmpty() const
    {
        return m_data.payload == ValueEmpty;
    }

    void operator=(PointerValue* from)
    {
        operator=(Value(from));
    }

    template <typename T, typename std::enable_if<std::is_convertible<T*, PointerValue*>::value && !std::is_same<PointerValue, typename std::remove_cv<T>::type>::value, int>::type = 0>
    void operator=(T* from)
    {
        operator=(Value(from));
    }

    bool operator==(const EncodedSmallValue& other) const
    {
        // Raw compare: payload()'s sign extension is a pure function of these
        // bits, so it can neither merge two different slots nor split two equal
        // ones.
        return m_data.payload == other.m_data.payload;
    }

    ALWAYS_INLINE const EncodedSmallValue& operator=(const EncodedValue& from)
    {
        setPayload(from.payload());
        return *this;
    }

    ALWAYS_INLINE const EncodedSmallValue& operator=(const Value& from)
    {
        if (LIKELY(storeNonNumber(from))) {
            return *this;
        }

        int32_t i32;
        if (from.isInt32() && EncodedValueImpl::PlatformSmiTagging::IsValidSmi(i32 = from.asInt32())) {
            setPayload(EncodedValueImpl::PlatformSmiTagging::IntToSmi(i32));
            return *this;
        }

        Value mutableFrom(from);
        if (UNLIKELY(Value::isInt32ConvertibleDouble(mutableFrom.asNumber(), i32))) {
            mutableFrom = Value(i32);
        }
        if (!isSMI()) {
            const uintptr_t bits = static_cast<uint32_t>(m_data.payload);
            if (bits > static_cast<uintptr_t>(ValueLast) && pointerKind(bits) == NumberPointerKind) {
                reinterpret_cast<NumberInEncodedValue*>(untagPointer(bits))->setValue(mutableFrom);
                return *this;
            }
        }
        setPayload(static_cast<intptr_t>(tagPointer(new NumberInEncodedValue(mutableFrom), NumberPointerKind)));
        return *this;
    }

private:
    // Same contract as EncodedValue::storeNonNumber(): a compressed slot holds
    // pointers, immediates and Empty as their raw bit pattern, so one test
    // covers every non-number Value.
    ALWAYS_INLINE bool storeNonNumber(const Value& from)
    {
        if (LIKELY(!from.isNumber())) {
            setPayload(from.rawPayload());
            return true;
        }
        return false;
    }

    ALWAYS_INLINE bool isSMI() const
    {
        return HAS_SMI_TAG(m_data.payload);
    }

    ALWAYS_INLINE void setPayload(intptr_t v)
    {
#ifndef NDEBUG
        if (HAS_SMI_TAG(v)) {
            // value may be negative integer
            ASSERT(std::numeric_limits<int32_t>::min() <= v && v <= std::numeric_limits<int32_t>::max());
        } else {
            // otherwise, use only lower 32bits
            ASSERT((size_t)v <= std::numeric_limits<uint32_t>::max());
        }
#endif
        m_data.payload = static_cast<int32_t>(v);
    }

    EncodedSmallValueData m_data;
};
#endif

#if defined(ESCARGOT_64) && defined(ESCARGOT_USE_32BIT_IN_64BIT)
using ObjectPropertyValue = EncodedSmallValue;
typedef ObjectPropertyValue EncodedValueVectorElement;
typedef Vector<EncodedValueVectorElement, CustomAllocator<EncodedValueVectorElement>> EncodedValueVector;
typedef TightVector<EncodedValueVectorElement, CustomAllocator<EncodedValueVectorElement>> EncodedValueTightVector;
typedef TightVectorWithNoSizeUseGCRealloc<EncodedSmallValue, CustomAllocator<EncodedSmallValue>> ObjectPropertyValueVector;
#else
using ObjectPropertyValue = EncodedValue;
typedef ObjectPropertyValue EncodedValueVectorElement;
typedef Vector<EncodedValueVectorElement, GCUtil::gc_malloc_allocator<EncodedValueVectorElement>> EncodedValueVector;
typedef TightVector<EncodedValueVectorElement, GCUtil::gc_malloc_allocator<EncodedValueVectorElement>> EncodedValueTightVector;
typedef TightVectorWithNoSizeUseGCRealloc<EncodedValue, GCUtil::gc_malloc_allocator<EncodedValue>> ObjectPropertyValueVector;
#endif
} // namespace Escargot

namespace std {

template <>
struct is_fundamental<Escargot::EncodedValue> : public true_type {
};

#if defined(ESCARGOT_64) && defined(ESCARGOT_USE_32BIT_IN_64BIT)
template <>
struct is_fundamental<Escargot::EncodedSmallValue> : public true_type {
};
#endif
} // namespace std

#endif
