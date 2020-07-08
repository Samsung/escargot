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

namespace Escargot {

class ExtendedNativeFunctionObject : public NativeFunctionObject {
public:
    virtual bool isExtendedNativeFunctionObject() const
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

    ExtendedNativeFunctionObject(ExecutionState& state, NativeFunctionInfo info)
        : NativeFunctionObject(state, info)
    {
    }

    ExtendedNativeFunctionObject(ExecutionState& state, NativeFunctionInfo info, NativeFunctionObject::ForBuiltinConstructor flag)
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
    ExtendedNativeFunctionObjectImpl(ExecutionState& state, NativeFunctionInfo info)
        : ExtendedNativeFunctionObject(state, info)
#ifndef NDEBUG
        , m_slotCount(slotNumber)
#endif
    {
    }

    ExtendedNativeFunctionObjectImpl(ExecutionState& state, NativeFunctionInfo info, NativeFunctionObject::ForBuiltinConstructor flag)
        : ExtendedNativeFunctionObject(state, info, flag)
#ifndef NDEBUG
        , m_slotCount(slotNumber)
#endif
    {
    }

    // used only for FunctionTemplate instantiation
    ExtendedNativeFunctionObjectImpl(Context* context, ObjectStructure* structure, ObjectPropertyValueVector&& values, const NativeFunctionInfo& info)
        : ExtendedNativeFunctionObject(context, structure, std::forward<ObjectPropertyValueVector>(values), info)
#ifndef NDEBUG
        , m_slotCount(slotNumber)
#endif
    {
    }

protected:
    virtual InternalSlotData* internalSlots() override
    {
        return m_values;
    }

#ifndef NDEBUG
    virtual size_t slotCount() const override
    {
        return m_slotCount;
    }

    size_t m_slotCount;
#endif
    InternalSlotData m_values[slotNumber];
};
}

#endif
