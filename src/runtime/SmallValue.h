// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
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

#ifndef __EscargotSmallValue__
#define __EscargotSmallValue__

#include "runtime/SmallValueData.h"
#include "runtime/Value.h"

namespace Escargot {

class PointerValue;


#ifdef ESCARGOT_32
COMPILE_ASSERT(sizeof(SmallValueData) == 4, "");
#else
COMPILE_ASSERT(sizeof(SmallValueData) == 8, "");
#endif

class DoubleInSmallValue : public PointerValue {
    friend class SmallValue;

public:
    virtual bool isDoubleInSmallValue() const
    {
        return true;
    }

    explicit DoubleInSmallValue(double v)
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

// TODO add license comment these code.

namespace SmallValueImpl {

const int kApiAlignSize = 4;
const int kApiIntSize = sizeof(int);
const int kApiInt64Size = sizeof(int64_t);

// Tag information for HeapObject.
const intptr_t kHeapObjectTag = 0;
const int kHeapObjectTagSize = 1;
const intptr_t kHeapObjectTagMask = (1 << kHeapObjectTagSize) - 1;

// Tag information for Smi.
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

#define HAS_SMI_TAG(value) \
    ((reinterpret_cast<intptr_t>(value) & ::Escargot::SmallValueImpl::kSmiTagMask) == ::Escargot::SmallValueImpl::kSmiTag)
#define HAS_OBJECT_TAG(value) \
    ((value & ::Escargot::SmallValueImpl::kHeapObjectTagMask) == ::Escargot::SmallValueImpl::kHeapObjectTag)
}

extern size_t g_doubleInSmallValueTag;

class SmallValue {
public:
    SmallValue()
    {
        m_data.payload = (intptr_t)(ValueUndefined);
    }

    SmallValue(const SmallValue& from)
    {
        m_data.payload = from.m_data.payload;
    }

    explicit SmallValue(const uint32_t& from)
    {
        if (LIKELY(SmallValueImpl::PlatformSmiTagging::IsValidSmi(from))) {
            m_data.payload = SmallValueImpl::PlatformSmiTagging::IntToSmi(from);
        } else {
            fromValueForCtor(Value(from));
        }
    }

    SmallValue(const Value& from)
    {
        fromValueForCtor(from);
    }

    explicit SmallValue(PointerValue* v)
    {
        m_data.payload = (intptr_t)v;
    }

    bool isStoredInHeap()
    {
        if (HAS_OBJECT_TAG(m_data.payload)) {
            PointerValue* v = (PointerValue*)m_data.payload;
            if (((size_t)v) > ValueLast) {
                return true;
            }
        }
        return false;
    }

    intptr_t payload() const
    {
        return m_data.payload;
    }

    static SmallValue fromPayload(const void* p)
    {
        return SmallValue(SmallValueData((void*)p));
    }

    bool isEmpty() const
    {
        return m_data.payload == ValueEmpty;
    }

    operator Value() const
    {
        if (HAS_OBJECT_TAG(m_data.payload)) {
            PointerValue* v = (PointerValue*)m_data.payload;
            if (((size_t)v) <= ValueLast) {
#ifdef ESCARGOT_32
                return Value(Value::FromTag, ~((uint32_t)v));
#else
                return Value(v);
#endif
            } else if (g_doubleInSmallValueTag == *((size_t*)v)) {
                return Value(v->asDoubleInSmallValue()->value());
            }
            return Value(v);
        } else {
            int32_t value = SmallValueImpl::PlatformSmiTagging::SmiToInt(m_data.payload);
            return Value(value);
        }
        return Value();
    }

    bool isInt32()
    {
        if (HAS_OBJECT_TAG(m_data.payload)) {
            return false;
        }

        return true;
    }

    uint32_t asInt32()
    {
        ASSERT(!HAS_OBJECT_TAG(m_data.payload));
        int32_t value = SmallValueImpl::PlatformSmiTagging::SmiToInt(m_data.payload);
        return (uint32_t)value;
    }

    uint32_t asUint32()
    {
        ASSERT(!HAS_OBJECT_TAG(m_data.payload));
        int32_t value = SmallValueImpl::PlatformSmiTagging::SmiToInt(m_data.payload);
        return (uint32_t)value;
    }

    uint32_t toUint32(ExecutionState& state)
    {
        if (UNLIKELY(HAS_OBJECT_TAG(m_data.payload))) {
            return operator Escargot::Value().toUint32(state);
        } else {
            int32_t value = SmallValueImpl::PlatformSmiTagging::SmiToInt(m_data.payload);
            return (uint32_t)value;
        }
    }

    void operator=(PointerValue* from)
    {
        ASSERT(from);
        m_data.payload = (intptr_t)from;
    }

    void operator=(const Value& from)
    {
        if (from.isPointerValue()) {
#ifdef ESCARGOT_32
            ASSERT(!from.isEmpty());
#endif
            m_data.payload = (intptr_t)from.asPointerValue();
        } else {
            int32_t i32;
            if (from.isInt32() && SmallValueImpl::PlatformSmiTagging::IsValidSmi(i32 = from.asInt32())) {
                m_data.payload = SmallValueImpl::PlatformSmiTagging::IntToSmi(i32);
            } else if (from.isNumber()) {
                auto payload = m_data.payload;
                if (((size_t)payload > (size_t)ValueLast) && HAS_OBJECT_TAG(payload)) {
                    PointerValue* v = (PointerValue*)payload;
                    if (g_doubleInSmallValueTag == *((size_t*)v)) {
                        ((DoubleInSmallValue*)m_data.payload)->m_value = from.asNumber();
                        return;
                    }
                }
                m_data.payload = reinterpret_cast<intptr_t>(new DoubleInSmallValue(from.asNumber()));
            } else {
#ifdef ESCARGOT_32
                m_data.payload = ~from.tag();
#else
                m_data.payload = from.payload();
#endif
            }
        }
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
            if (from.isInt32() && SmallValueImpl::PlatformSmiTagging::IsValidSmi(i32 = from.asInt32())) {
                m_data.payload = SmallValueImpl::PlatformSmiTagging::IntToSmi(i32);
            } else if (from.isNumber()) {
                m_data.payload = reinterpret_cast<intptr_t>(new DoubleInSmallValue(from.asNumber()));
            } else {
#ifdef ESCARGOT_32
                m_data.payload = ~from.tag();
#else
                m_data.payload = from.payload();
#endif
            }
        }
    }

    explicit SmallValue(SmallValueData v)
        : m_data(v)
    {
    }

private:
    SmallValueData m_data;
};

typedef Vector<SmallValue, GCUtil::gc_malloc_ignore_off_page_allocator<SmallValue>> SmallValueVector;
typedef TightVector<SmallValue, GCUtil::gc_malloc_ignore_off_page_allocator<SmallValue>> SmallValueTightVector;
}

namespace std {

template <>
struct is_fundamental<Escargot::SmallValue> : public true_type {
};
}

#endif
