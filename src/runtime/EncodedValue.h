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
#else
COMPILE_ASSERT(sizeof(EncodedValueData) == 8, "");
#endif

class DoubleInEncodedValue : public PointerValue {
    friend class EncodedValue;
    friend class EncodedSmallValue;

public:
    explicit DoubleInEncodedValue(double v)
        : m_value(v)
    {
    }

    void* operator new(size_t size)
    {
        return GC_MALLOC_ATOMIC(size);
    }
    void* operator new[](size_t size) = delete;

    double value()
    {
        return m_value;
    }

private:
    double m_value;
};

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
    ((reinterpret_cast<intptr_t>((long int)value) & ::Escargot::EncodedValueImpl::kSmiTagMask) == ::Escargot::EncodedValueImpl::kSmiTag)
#else
#define HAS_SMI_TAG(value) \
    ((reinterpret_cast<intptr_t>(value) & ::Escargot::EncodedValueImpl::kSmiTagMask) == ::Escargot::EncodedValueImpl::kSmiTag)
#endif
}


// EncodedValue turns int, double values into pointer or odd value
// so there is no conservative gc leak(there is no even value looks like pointer without pointers)
// developers should use this class if want to save some Value on Heap
// developers should not copy this value because this class changes DoubleInEncodedValue without copy it
// just convert into Value and use it.
class EncodedValue {
public:
    enum ForceUninitializedTag { ForceUninitialized };
    enum EmptyValueInitTag { EmptyValue };

    EncodedValue(ForceUninitializedTag)
    {
    }

    EncodedValue(EmptyValueInitTag)
    {
        m_data.payload = (intptr_t)(ValueEmpty);
    }

    EncodedValue()
    {
        m_data.payload = (intptr_t)(ValueUndefined);
    }

    EncodedValue(const EncodedValue& from)
    {
        m_data.payload = from.m_data.payload;
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
        m_data.payload = (intptr_t)v;
    }

    bool isStoredInHeap()
    {
        if (HAS_SMI_TAG(m_data.payload)) {
            return false;
        }

        PointerValue* v = (PointerValue*)m_data.payload;
        return ((size_t)v) > ValueLast;
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

    ALWAYS_INLINE operator Value() const
    {
        if (HAS_SMI_TAG(m_data.payload)) {
            int32_t value = EncodedValueImpl::PlatformSmiTagging::SmiToInt(m_data.payload);
            return Value(value);
        }

        PointerValue* v = (PointerValue*)m_data.payload;
        if (((size_t)v) <= ValueLast) {
#ifdef ESCARGOT_32
            return Value(Value::FromTag, ~((uint32_t)v));
#else
            return Value(v);
#endif
        } else if (v->isDoubleInEncodedValue()) {
            return Value(v->asDoubleInEncodedValue()->value());
        }
        return Value(v);
    }

    bool isInt32()
    {
        return HAS_SMI_TAG(m_data.payload);
    }

    uint32_t asInt32()
    {
        ASSERT(HAS_SMI_TAG(m_data.payload));
        int32_t value = EncodedValueImpl::PlatformSmiTagging::SmiToInt(m_data.payload);
        return (uint32_t)value;
    }

    uint32_t asUint32()
    {
        ASSERT(HAS_SMI_TAG(m_data.payload));
        int32_t value = EncodedValueImpl::PlatformSmiTagging::SmiToInt(m_data.payload);
        return (uint32_t)value;
    }

    uint32_t toUint32(ExecutionState& state)
    {
        if (LIKELY(HAS_SMI_TAG(m_data.payload))) {
            int32_t value = EncodedValueImpl::PlatformSmiTagging::SmiToInt(m_data.payload);
            return (uint32_t)value;
        }

        return operator Escargot::Value().toUint32(state);
    }

    void operator=(PointerValue* from)
    {
        ASSERT(from);
        m_data.payload = (intptr_t)from;
    }

    bool operator==(const EncodedValue& other) const
    {
        return m_data.payload == other.payload();
    }

    ALWAYS_INLINE void operator=(const Value& from)
    {
        if (from.isPointerValue()) {
#ifdef ESCARGOT_32
            ASSERT(!from.isEmpty());
#endif
            m_data.payload = (intptr_t)from.asPointerValue();
            return;
        }

        int32_t i32;
        if (from.isInt32() && EncodedValueImpl::PlatformSmiTagging::IsValidSmi(i32 = from.asInt32())) {
            m_data.payload = EncodedValueImpl::PlatformSmiTagging::IntToSmi(i32);
            return;
        }

        if (from.isNumber()) {
            auto payload = m_data.payload;

            if (!HAS_SMI_TAG(payload) && ((size_t)payload > (size_t)ValueLast)) {
                PointerValue* v = (PointerValue*)payload;
                if (v->isDoubleInEncodedValue()) {
                    ((DoubleInEncodedValue*)m_data.payload)->m_value = from.asNumber();
                    return;
                }
            }
            m_data.payload = reinterpret_cast<intptr_t>(new DoubleInEncodedValue(from.asNumber()));
            return;
        }

#ifdef ESCARGOT_32
        m_data.payload = ~from.tag();
#else
        m_data.payload = from.payload();
#endif
    }

protected:
    void fromValueForCtor(const Value& from)
    {
        if (from.isPointerValue()) {
#ifdef ESCARGOT_32
            ASSERT(!from.isEmpty());
#endif
            m_data.payload = (intptr_t)from.asPointerValue();
        } else {
            int32_t i32;
            if (from.isInt32() && EncodedValueImpl::PlatformSmiTagging::IsValidSmi(i32 = from.asInt32())) {
                m_data.payload = EncodedValueImpl::PlatformSmiTagging::IntToSmi(i32);
            } else if (from.isNumber()) {
                m_data.payload = reinterpret_cast<intptr_t>(new DoubleInEncodedValue(from.asNumber()));
            } else {
#ifdef ESCARGOT_32
                m_data.payload = ~from.tag();
#else
                m_data.payload = from.payload();
#endif
            }
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

    EncodedSmallValue(EmptyValueInitTag)
    {
        m_data.payload = (intptr_t)(ValueEmpty);
    }

    EncodedSmallValue()
    {
        m_data.payload = (intptr_t)(ValueUndefined);
    }

    EncodedSmallValue(const EncodedSmallValue& from)
    {
        m_data.payload = from.m_data.payload;
    }

    EncodedSmallValue(const Value& from)
    {
        if (from.isPointerValue()) {
            m_data = EncodedSmallValueData(from.asPointerValue());
        } else {
            int32_t i32;
            if (from.isInt32() && EncodedValueImpl::PlatformSmiTagging::IsValidSmi(i32 = from.asInt32())) {
                m_data.payload = EncodedValueImpl::PlatformSmiTagging::IntToSmi(i32);
            } else if (from.isNumber()) {
                m_data = EncodedSmallValueData(new DoubleInEncodedValue(from.asNumber()));
            } else {
                m_data.payload = from.payload();
            }
        }
    }

    EncodedSmallValue(const EncodedValue& from)
    {
        ASSERT(from.payload() <= std::numeric_limits<uint32_t>::max());
        m_data.payload = from.payload();
    }

    ALWAYS_INLINE operator EncodedValue() const
    {
        return EncodedValue::fromPayload(reinterpret_cast<void*>(m_data.payload));
    }

    static EncodedSmallValue fromPayload(void* p)
    {
        EncodedSmallValue v;
        v.m_data = EncodedSmallValueData(p);
        return v;
    }

    uint32_t payload() const
    {
        return m_data.payload;
    }

    ALWAYS_INLINE operator Value() const
    {
        if (HAS_SMI_TAG(m_data.payload)) {
            // we cannot use EncodedValueImpl::PlatformSmiTagging::SmiToInt
            // here because when convert negative from int32 to intptr_t loses
            // negative value
            int32_t value = (int)(m_data.payload) >> 1;
            return Value(value);
        }

        PointerValue* v = reinterpret_cast<PointerValue*>(m_data.payload);
        if (((size_t)v) <= ValueLast) {
            return Value(v);
        } else if (v->isDoubleInEncodedValue()) {
            return Value(v->asDoubleInEncodedValue()->value());
        }
        return Value(v);
    }

    bool isEmpty() const
    {
        return m_data.payload == ValueEmpty;
    }

    bool isInt32()
    {
        return HAS_SMI_TAG(m_data.payload);
    }

    bool isStoredInHeap()
    {
        if (HAS_SMI_TAG(m_data.payload)) {
            return false;
        }

        const auto& v = m_data.payload;
        return v > ValueLast;
    }

    void operator=(PointerValue* from)
    {
        m_data = EncodedSmallValueData(from);
    }

    bool operator==(const EncodedSmallValue& other) const
    {
        return m_data.payload == other.payload();
    }

    ALWAYS_INLINE void operator=(const EncodedValue& from)
    {
        ASSERT(from.payload() <= std::numeric_limits<uint32_t>::max());
        m_data.payload = from.payload();
    }

    ALWAYS_INLINE void operator=(const Value& from)
    {
        if (from.isPointerValue()) {
            m_data = EncodedSmallValueData(from.asPointerValue());
            return;
        }

        int32_t i32;
        if (from.isInt32() && EncodedValueImpl::PlatformSmiTagging::IsValidSmi(i32 = from.asInt32())) {
            m_data.payload = EncodedValueImpl::PlatformSmiTagging::IntToSmi(i32);
            return;
        }

        if (from.isNumber()) {
            auto payload = m_data.payload;

            if (!HAS_SMI_TAG(payload) && ((size_t)payload > (size_t)ValueLast)) {
                PointerValue* v = reinterpret_cast<PointerValue*>(m_data.payload);
                if (v->isDoubleInEncodedValue()) {
                    ((DoubleInEncodedValue*)v)->m_value = from.asNumber();
                    return;
                }
            }
            m_data = EncodedSmallValueData(new DoubleInEncodedValue(from.asNumber()));
            return;
        }
        m_data.payload = from.payload();
    }

private:
    EncodedSmallValueData m_data;
};
#endif

#if defined(ESCARGOT_64) && defined(ESCARGOT_USE_32BIT_IN_64BIT)
using ObjectPropertyValue = EncodedSmallValue;
typedef ObjectPropertyValue EncodedValueVectorElement;
typedef Vector<EncodedValueVectorElement, CustomAllocator<EncodedValueVectorElement>> EncodedValueVector;
typedef TightVector<EncodedValueVectorElement, CustomAllocator<EncodedValueVectorElement>> EncodedValueTightVector;
#else
using ObjectPropertyValue = EncodedValue;
typedef ObjectPropertyValue EncodedValueVectorElement;
typedef Vector<EncodedValueVectorElement, GCUtil::gc_malloc_allocator<EncodedValueVectorElement>> EncodedValueVector;
typedef TightVector<EncodedValueVectorElement, GCUtil::gc_malloc_allocator<EncodedValueVectorElement>> EncodedValueTightVector;
#endif
}

namespace std {

template <>
struct is_fundamental<Escargot::EncodedValue> : public true_type {
};

#if defined(ESCARGOT_64) && defined(ESCARGOT_USE_32BIT_IN_64BIT)
template <>
struct is_fundamental<Escargot::EncodedSmallValue> : public true_type {
};
#endif
}

#include "runtime/EncodedSmallValueVectors.h"

#endif
