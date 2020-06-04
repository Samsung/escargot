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
    ExtendedNativeFunctionObject(ExecutionState& state, NativeFunctionInfo info)
        : NativeFunctionObject(state, info)
    {
    }

    virtual bool isExtendedNativeFunctionObject() const
    {
        return true;
    }

    void setInternalSlot(const size_t idx, const Value& value)
    {
        ASSERT(idx < slotCount());
        internalSlots()[idx] = value;
    }

    EncodedValue& getInternalSlot(const size_t idx)
    {
        ASSERT(idx < slotCount());
        return internalSlots()[idx];
    }

protected:
#ifndef NDEBUG
    virtual size_t slotCount() const = 0;
#endif
    virtual EncodedValue* internalSlots() = 0;
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

protected:
    virtual EncodedValue* internalSlots() override
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
    EncodedValue m_values[slotNumber];
};
}

#endif
