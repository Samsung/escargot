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
COMPILE_ASSERT(sizeof(Value) == 4, "");
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
        Value ret;
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
#pragma pack(pop)

class EncodedValue {
public:
    enum ForceUninitializedTag { ForceUninitialized };
    enum EmptyValueInitTag { EmptyValue };
    COMPILE_ASSERT(EncodedValue::EmptyValue == 0, "");
    friend class EncodedSmallValue;

    EncodedValue(const Value& v = Value())
        : m_value(v)
    {
    }

    explicit EncodedValue(PointerValue* v)
        : m_value(v)
    {
    }

    static EncodedValue fromPayload(const void* p)
    {
        return Value(Value::FromPayload, reinterpret_cast<intptr_t>(p));
    }

    explicit EncodedValue(const uint32_t from)
        : m_value(Value(from))
    {
    }

    EncodedValue(ForceUninitializedTag)
    {
    }

    EncodedValue(EmptyValueInitTag)
    {
        m_value = Value(Value::EmptyValue);
    }

    const EncodedValue& operator=(PointerValue* from)
    {
        ASSERT(from);
        m_value = Value(from);
        return *this;
    }

    bool operator==(const EncodedValue& other) const
    {
        return Value(m_value) == Value(other);
    }

    bool isStoredInHeap() const
    {
        return m_value.isHeapValue();
    }

    intptr_t payload() const
    {
        return m_value.asRawData();
    }

    bool isEmpty() const
    {
        return m_value.isEmpty();
    }

    bool isInt32()
    {
        return m_value.isInt32();
    }

    bool isUInt32()
    {
        return m_value.isUInt32();
    }

    int32_t asInt32()
    {
        return m_value.asInt32();
    }

    uint32_t asUInt32()
    {
        return m_value.asUInt32();
    }

    ALWAYS_INLINE operator Value&()
    {
        return m_value;
    }

    ALWAYS_INLINE operator const Value&() const
    {
        return m_value;
    }
private:
    Value m_value;
};

/*
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
        m_data.payload = (intptr_t)(ValueUndefined);
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

    bool isStoredInHeap() const
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

    template <const bool shouldTreatEmptyAsUndefined = false>
    ALWAYS_INLINE Value toValue() const
    {
        if (HAS_SMI_TAG(m_data.payload)) {
            int32_t value = EncodedValueImpl::PlatformSmiTagging::SmiToInt(m_data.payload);
            return Value(value);
        }

        void* ptr = reinterpret_cast<void*>(m_data.payload);
        if (UNLIKELY(((size_t)ptr) <= ValueLast)) {
            if (shouldTreatEmptyAsUndefined && UNLIKELY(ptr == ValueEmpty)) {
                return Value();
            }
#ifdef ESCARGOT_32
            return Value(Value::FromTag, ~((uint32_t)ptr));
#else
            return Value(reinterpret_cast<PointerValue*>(ptr));
#endif
        }

#ifdef ESCARGOT_32
        const uint8_t tag = *(reinterpret_cast<size_t*>(ptr) + 1);
        if (LIKELY(!(tag & POINTER_VALUE_NOT_OBJECT_TAG_IN_DATA))) {
            return Value(reinterpret_cast<Object*>(ptr));
        } else if (UNLIKELY(tag == POINTER_VALUE_NUMBER_TAG_IN_DATA)) {
            return reinterpret_cast<NumberInEncodedValue*>(ptr)->value();
        } else {
            return Value(reinterpret_cast<PointerValue*>(ptr), Value::FromNonObjectPointer);
        }
#else
        if (UNLIKELY(readPointerIsNumberEncodedValue(ptr))) {
            return reinterpret_cast<NumberInEncodedValue*>(ptr)->value();
        } else {
            return Value(reinterpret_cast<PointerValue*>(ptr));
        }
#endif
    }

    ALWAYS_INLINE operator Value() const
    {
        return toValue<>();
    }

    bool isInt32()
    {
        return HAS_SMI_TAG(m_data.payload);
    }

    bool isUInt32()
    {
        // Note. use only 31 bits to represent unsigned integer value.
        // Its because we just store signed integer value.
        return isInt32() && asInt32() >= 0;
    }

    int32_t asInt32()
    {
        ASSERT(HAS_SMI_TAG(m_data.payload));
        return EncodedValueImpl::PlatformSmiTagging::SmiToInt(m_data.payload);
    }

    uint32_t asUInt32()
    {
        ASSERT(HAS_SMI_TAG(m_data.payload));
        int32_t value = EncodedValueImpl::PlatformSmiTagging::SmiToInt(m_data.payload);
        return (uint32_t)value;
    }

    uint32_t toUInt32(ExecutionState& state)
    {
        if (LIKELY(HAS_SMI_TAG(m_data.payload))) {
            int32_t value = EncodedValueImpl::PlatformSmiTagging::SmiToInt(m_data.payload);
            return (uint32_t)value;
        }

        return operator Escargot::Value().toUint32(state);
    }

    const EncodedValue& operator=(PointerValue* from)
    {
        ASSERT(from);
        m_data.payload = (intptr_t)from;
        return *this;
    }

    bool operator==(const EncodedValue& other) const
    {
        return m_data.payload == other.payload();
    }

    ALWAYS_INLINE const EncodedValue& operator=(const Value& from)
    {
        if (from.isPointerValue()) {
#ifdef ESCARGOT_32
            ASSERT(!from.isEmpty());
#endif
            m_data.payload = (intptr_t)from.asPointerValue();
            return *this;
        }

        int32_t i32;
        if (from.isInt32() && EncodedValueImpl::PlatformSmiTagging::IsValidSmi(i32 = from.asInt32())) {
            m_data.payload = EncodedValueImpl::PlatformSmiTagging::IntToSmi(i32);
            return *this;
        }

        if (from.isNumber()) {
            Value mutableFrom(from);
            if (UNLIKELY(Value::isInt32ConvertibleDouble(mutableFrom.asNumber(), i32))) {
                mutableFrom = Value(i32);
            }
            intptr_t payload = m_data.payload;

            if (!HAS_SMI_TAG(payload) && ((size_t)payload > (size_t)ValueLast)) {
                void* v = (void*)payload;
                if (readPointerIsNumberEncodedValue(v)) {
                    ((NumberInEncodedValue*)m_data.payload)->setValue(mutableFrom);
                    return *this;
                }
            }
            m_data.payload = reinterpret_cast<intptr_t>(new NumberInEncodedValue(mutableFrom));
            ASSERT(readPointerIsNumberEncodedValue((void*)m_data.payload));
            return *this;
        }

#ifdef ESCARGOT_32
        m_data.payload = ~from.tag();
#else
        m_data.payload = from.payload();
#endif
        return *this;
    }

protected:
    static ALWAYS_INLINE bool readPointerIsNumberEncodedValue(void* ptr)
    {
        const uint8_t* b = reinterpret_cast<const uint8_t*>(reinterpret_cast<size_t*>(ptr) + 1);
        return *b == POINTER_VALUE_NUMBER_TAG_IN_DATA;
    }

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
                if (UNLIKELY(Value::isInt32ConvertibleDouble(from.asNumber(), i32) && EncodedValueImpl::PlatformSmiTagging::IsValidSmi(i32))) {
                    m_data.payload = EncodedValueImpl::PlatformSmiTagging::IntToSmi(i32);
                } else {
                    m_data.payload = reinterpret_cast<intptr_t>(new NumberInEncodedValue(from));
                }
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
*/

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
        if (from.isPointerValue()) {
            setPayload(reinterpret_cast<intptr_t>(from.asPointerValue()));
        } else {
            int32_t i32;
            if (from.isInt32() && EncodedValueImpl::PlatformSmiTagging::IsValidSmi(i32 = from.asInt32())) {
                setPayload(EncodedValueImpl::PlatformSmiTagging::IntToSmi(i32));
            } else if (from.isNumber()) {
                if (UNLIKELY(Value::isInt32ConvertibleDouble(from.asNumber(), i32) && EncodedValueImpl::PlatformSmiTagging::IsValidSmi(i32))) {
                    setPayload(EncodedValueImpl::PlatformSmiTagging::IntToSmi(i32));
                } else {
                    setPayload(reinterpret_cast<intptr_t>(new NumberInEncodedValue(from)));
                }
            } else {
                setPayload(from.payload());
            }
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
        // we should consider negative integer value at here
        if (!isSMI()) {
            return static_cast<uint32_t>(m_data.payload);
        }
        return m_data.payload;
    }

    template <const bool shouldTreatEmptyAsUndefined = false>
    ALWAYS_INLINE Value toValue() const
    {
        if (isSMI()) {
            // we cannot use EncodedValueImpl::PlatformSmiTagging::SmiToInt
            // here because when convert negative from int32 to intptr_t loses
            // negative value
            int32_t value = (int)(m_data.payload) >> 1;
            return Value(value);
        }

        void* ptr = reinterpret_cast<void*>(payload());
        if (((size_t)ptr) <= ValueLast) {
            if (shouldTreatEmptyAsUndefined && UNLIKELY(ptr == ValueEmpty)) {
                return Value();
            }
            return Value(reinterpret_cast<PointerValue*>(ptr));
        }

        if (EncodedValue::readPointerIsNumberEncodedValue(ptr)) {
            return reinterpret_cast<NumberInEncodedValue*>(ptr)->value();
        } else {
            return Value(reinterpret_cast<PointerValue*>(ptr));
        }
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
        setPayload(reinterpret_cast<intptr_t>(from));
    }

    bool operator==(const EncodedSmallValue& other) const
    {
        return payload() == other.payload();
    }

    ALWAYS_INLINE const EncodedSmallValue& operator=(const EncodedValue& from)
    {
        setPayload(from.payload());
        return *this;
    }

    ALWAYS_INLINE const EncodedSmallValue& operator=(const Value& from)
    {
        if (from.isPointerValue()) {
            setPayload(reinterpret_cast<intptr_t>(from.asPointerValue()));
            return *this;
        }

        int32_t i32;
        if (from.isInt32() && EncodedValueImpl::PlatformSmiTagging::IsValidSmi(i32 = from.asInt32())) {
            setPayload(EncodedValueImpl::PlatformSmiTagging::IntToSmi(i32));
            return *this;
        }

        if (from.isNumber()) {
            Value mutableFrom(from);
            if (UNLIKELY(Value::isInt32ConvertibleDouble(mutableFrom.asNumber(), i32))) {
                mutableFrom = Value(i32);
            }
            auto pl = payload();
            if (!isSMI() && ((size_t)pl > (size_t)ValueLast)) {
                void* v = reinterpret_cast<void*>(pl);
                if (EncodedValue::readPointerIsNumberEncodedValue(v)) {
                    reinterpret_cast<NumberInEncodedValue*>(v)->setValue(mutableFrom);
                    return *this;
                }
            }
            setPayload(reinterpret_cast<intptr_t>(new NumberInEncodedValue(mutableFrom)));
            ASSERT(EncodedValue::readPointerIsNumberEncodedValue((void*)payload()));
            return *this;
        }
        setPayload(from.payload());
        return *this;
    }

private:
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
typedef TightVectorWithNoSize<EncodedSmallValue, CustomAllocator<EncodedSmallValue>> ObjectPropertyValueVector;
#else
using ObjectPropertyValue = EncodedValue;
typedef ObjectPropertyValue EncodedValueVectorElement;
typedef Vector<EncodedValueVectorElement, GCUtil::gc_malloc_allocator<EncodedValueVectorElement>> EncodedValueVector;
typedef TightVector<EncodedValueVectorElement, GCUtil::gc_malloc_allocator<EncodedValueVectorElement>> EncodedValueTightVector;
typedef TightVectorWithNoSizeUseGCRealloc<EncodedValue> ObjectPropertyValueVector;
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
