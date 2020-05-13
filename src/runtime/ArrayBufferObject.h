/*
 * Copyright (c) 2017-present Samsung Electronics Co., Ltd
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

#ifndef __EscargotArrayBufferObject__
#define __EscargotArrayBufferObject__

#include "runtime/Context.h"
#include "runtime/TypedArrayInlines.h"

namespace Escargot {

class ArrayBufferObject : public Object {
    friend void initializeCustomAllocators();
    friend int getValidValueInArrayBufferObject(void* ptr, GC_mark_custom_result* arr);
    friend void careArrayBufferObjectsOnGCEvent(GC_EventType evtType);

public:
    explicit ArrayBufferObject(ExecutionState& state);
    explicit ArrayBufferObject(ExecutionState& state, Object* proto);

    static ArrayBufferObject* allocateArrayBuffer(ExecutionState& state, Object* constructor, uint64_t byteLength);
    static ArrayBufferObject* cloneArrayBuffer(ExecutionState& state, ArrayBufferObject* srcBuffer, size_t srcByteOffset, uint64_t srcLength, Object* constructor);

    static const uint32_t maxArrayBufferSize = 210000000;

    void allocateBuffer(ExecutionState& state, size_t bytelength);
    void attachBuffer(ExecutionState& state, void* buffer, size_t bytelength);
    void detachArrayBuffer(ExecutionState& state);

    virtual bool isArrayBufferObject() const
    {
        return true;
    }

    ALWAYS_INLINE const uint8_t* data() { return m_data; }
    ALWAYS_INLINE size_t byteLength() { return m_bytelength; }
    // $24.1.1.6
    Value getValueFromBuffer(ExecutionState& state, size_t byteindex, TypedArrayType type, bool isLittleEndian = true);
    // $24.1.1.8
    void setValueInBuffer(ExecutionState& state, size_t byteindex, TypedArrayType type, const Value& val, bool isLittleEndian = true);

    ALWAYS_INLINE bool isDetachedBuffer()
    {
        if (data() == nullptr) {
            return true;
        }
        return false;
    }

    ALWAYS_INLINE void throwTypeErrorIfDetached(ExecutionState& state)
    {
        if (UNLIKELY(isDetachedBuffer())) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().TypedArray.string(), true, state.context()->staticStrings().constructor.string(), ErrorObject::Messages::GlobalObject_DetachedBuffer);
        }
    }

    void fillData(const uint8_t* data, size_t length)
    {
        ASSERT(!isDetachedBuffer());
        memcpy(m_data, data, length);
    }

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

private:
    Context* m_context;
    uint8_t* m_data;
    size_t m_bytelength;
};
}

#endif
