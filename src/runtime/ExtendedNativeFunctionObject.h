/*
 * Copyright (c) 2020-present Samsung Electronics Co., Ltd
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more detaials.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 */

#ifndef __EscargotExtendedNativeFunctionObject__
#define __EscargotExtendedNativeFunctionObject__

#include "NativeFunctionObject.h"
#include "heap/Heap.h"

namespace Escargot {

class ExtendedNativeFunctionObject : public NativeFunctionObject {
public:
    virtual bool isExtendedNativeFunctionObject() const override
    {
        return true;
    }

    void setInternalSlot(const size_t idx, const Value& value)
    {
        ASSERT(idx < slotCount());
        internalSlots()[idx] = EncodedValue(value);
    }

    Value internalSlot(const size_t idx)
    {
        ASSERT(idx < slotCount());
        return internalSlots()[idx].m_heapValue;
    }

    void setInternalSlotAsPointer(const size_t idx, void* ptr)
    {
        ASSERT(idx < slotCount());
        internalSlots()[idx] = ptr;
    }

    template <typename T>
    T* internalSlotAsPointer(const size_t idx)
    {
        ASSERT(idx < slotCount());
        return (T*)internalSlots()[idx].m_pointer;
    }

protected:
    union InternalSlotData {
        EncodedValue m_heapValue;
        void* m_pointer;

        InternalSlotData()
            : m_heapValue()
        {
        }

        InternalSlotData(const EncodedValue& v)
            : m_heapValue(v)
        {
        }

        InternalSlotData(void* ptr)
            : m_pointer(ptr)
        {
        }
    };

    ExtendedNativeFunctionObject(ExecutionState& state, const NativeFunctionInfo& info)
        : NativeFunctionObject(state, info)
    {
    }

    ExtendedNativeFunctionObject(ExecutionState& state, const NativeFunctionInfo& info, NativeFunctionObject::ForBuiltinConstructor flag)
        : NativeFunctionObject(state, info, flag)
    {
    }

    ExtendedNativeFunctionObject(Context* context, ObjectStructure* structure, ObjectPropertyValueVector&& values, const NativeFunctionInfo& info)
        : NativeFunctionObject(context, structure, std::forward<ObjectPropertyValueVector>(values), info)
    {
    }

#ifndef NDEBUG
    virtual size_t slotCount() const = 0;
#endif
    virtual InternalSlotData* internalSlots() = 0;
};

template <const size_t slotNumber>
class ExtendedNativeFunctionObjectImpl : public ExtendedNativeFunctionObject {
public:
    ExtendedNativeFunctionObjectImpl(ExecutionState& state, const NativeFunctionInfo& info)
        : ExtendedNativeFunctionObject(state, info)
#ifndef NDEBUG
        , m_slotCount(slotNumber)
#endif
#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
        , m_values(allocateSlots(slotNumber))
#endif
    {
    }

    ExtendedNativeFunctionObjectImpl(ExecutionState& state, const NativeFunctionInfo& info, NativeFunctionObject::ForBuiltinConstructor flag)
        : ExtendedNativeFunctionObject(state, info, flag)
#ifndef NDEBUG
        , m_slotCount(slotNumber)
#endif
#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
        , m_values(allocateSlots(slotNumber))
#endif
    {
    }

    // used only for FunctionTemplate instantiation
    ExtendedNativeFunctionObjectImpl(Context* context, ObjectStructure* structure, ObjectPropertyValueVector&& values, const NativeFunctionInfo& info)
        : ExtendedNativeFunctionObject(context, structure, std::forward<ObjectPropertyValueVector>(values), info)
#ifndef NDEBUG
        , m_slotCount(slotNumber)
#endif
#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
        , m_values(allocateSlots(slotNumber))
#endif
    {
    }

    void* operator new(size_t size)
    {
#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
        if (UNLIKELY(!Heap::isCompressedTypeInitialized(Heap::CompressedType::ExtendedNativeFunctionObject))) {
            GC_word bitmap[(sizeof(ExtendedNativeFunctionObjectImpl) / 4 + GC_WORDSZ - 1) / GC_WORDSZ] = { 0 };
            FunctionObject::fillCompressedGCDescriptor(bitmap);
            GC_set_bit(bitmap, offsetof(ExtendedNativeFunctionObjectImpl, m_values) / 4);
            Heap::initializeCompressedType(Heap::CompressedType::ExtendedNativeFunctionObject, size, bitmap, sizeof(ExtendedNativeFunctionObjectImpl) / 4);
        }
        return Heap::mallocCompressed(Heap::CompressedType::ExtendedNativeFunctionObject, size);
#else
        return GC_MALLOC(size);
#endif
    }
    void* operator new[](size_t size) = delete;

protected:
    virtual InternalSlotData* internalSlots() override
    {
#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
        return m_values.raw();
#else
        return m_values;
#endif
    }

#ifndef NDEBUG
    virtual size_t slotCount() const override
    {
        return m_slotCount;
    }

    size_t m_slotCount;
#endif
#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
    static Optional<InternalSlotData*> allocateSlots(size_t count)
    {
        if (count == 0) {
            return nullptr;
        }
        InternalSlotData* slots = static_cast<InternalSlotData*>(GC_MALLOC(sizeof(InternalSlotData) * count));
        ASSERT(slots);
        for (size_t i = 0; i < count; ++i) {
            new (&slots[i]) InternalSlotData();
        }
        return slots;
    }
    CompressibleHeapPointer<InternalSlotData> m_values;
#else
    InternalSlotData m_values[slotNumber];
#endif
};
} // namespace Escargot

#endif
