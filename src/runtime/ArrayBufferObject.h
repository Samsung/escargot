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

#include "runtime/Object.h"
#include "runtime/ErrorObject.h"
#include "runtime/Context.h"

namespace Escargot {

enum TypedArrayType : unsigned {
    Int8,
    Int16,
    Int32,
    Uint8,
    Uint16,
    Uint32,
    Uint8Clamped,
    Float32,
    Float64
};

class ArrayBufferObject : public Object {
    friend void initializeCustomAllocators();
    friend int getValidValueInArrayBufferObject(void* ptr, GC_mark_custom_result* arr);
    friend void careArrayBufferObjectsOnGCEvent(GC_EventType evtType);

public:
    explicit ArrayBufferObject(ExecutionState& state);
    explicit ArrayBufferObject(ExecutionState& state, Object* proto);

    static ArrayBufferObject* allocateArrayBuffer(ExecutionState& state, Object* constructor);

    static const uint32_t maxArrayBufferSize = 210000000;

    // Clone srcBuffer's srcByteOffset ~ end.
    bool cloneBuffer(ExecutionState& state, ArrayBufferObject* srcBuffer, size_t srcByteOffset);
    // Clone srcBuffer's srcByteOffset ~ (srcByteOffset + cloneLength).
    bool cloneBuffer(ExecutionState& state, ArrayBufferObject* srcBuffer, size_t srcByteOffset, size_t cloneLength);
    void allocateBuffer(ExecutionState& state, size_t bytelength);
    void attachBuffer(ExecutionState& state, void* buffer, size_t bytelength);
    void detachArrayBuffer(ExecutionState& state);

    virtual bool isArrayBufferObject() const
    {
        return true;
    }

    ALWAYS_INLINE const uint8_t* data() { return m_data; }
    ALWAYS_INLINE unsigned byteLength() { return m_bytelength; }
    // $24.1.1.5
    template <typename Type>
    Value getValueFromBuffer(ExecutionState& state, unsigned byteindex, bool isLittleEndian = 1)
    {
        // If isLittleEndian is not present, set isLittleEndian to either true or false.
        ASSERT(m_bytelength);
        size_t elementSize = sizeof(Type);
        ASSERT(byteindex + elementSize <= m_bytelength);
        uint8_t* rawStart = m_data + byteindex;
        Type res;
        if (isLittleEndian != 1) { // bigEndian
            for (size_t i = 0; i < elementSize; i++) {
                ((uint8_t*)&res)[elementSize - i - 1] = rawStart[i];
            }
        } else { // littleEndian
            res = *((Type*)rawStart);
        }
        return Value(res);
    }
    // $24.1.1.6
    template <typename TypeAdaptor>
    bool setValueInBuffer(ExecutionState& state, unsigned byteindex, Value val, bool isLittleEndian = 1)
    {
        // If isLittleEndian is not present, set isLittleEndian to either true or false.
        ASSERT(m_bytelength);
        size_t elementSize = sizeof(typename TypeAdaptor::Type);
        ASSERT(byteindex + elementSize <= m_bytelength);
        uint8_t* rawStart = m_data + byteindex;
        typename TypeAdaptor::Type littleEndianVal = TypeAdaptor::toNative(state, val);
        if (isLittleEndian != 1) {
            for (size_t i = 0; i < elementSize; i++) {
                rawStart[i] = ((uint8_t*)&littleEndianVal)[elementSize - i - 1];
            }
        } else {
            *((typename TypeAdaptor::Type*)rawStart) = littleEndianVal;
        }
        return true;
    }

    Value getTypedValueFromBuffer(ExecutionState& state, unsigned byteindex, TypedArrayType type)
    {
        switch (type) {
        case TypedArrayType::Int8:
            return getValueFromBuffer<int8_t>(state, byteindex, true);
        case TypedArrayType::Int16:
            return getValueFromBuffer<int16_t>(state, byteindex, true);
        case TypedArrayType::Int32:
            return getValueFromBuffer<int32_t>(state, byteindex, true);
        case TypedArrayType::Uint8:
            return getValueFromBuffer<uint8_t>(state, byteindex, true);
        case TypedArrayType::Uint16:
            return getValueFromBuffer<uint16_t>(state, byteindex, true);
        case TypedArrayType::Uint32:
            return getValueFromBuffer<uint32_t>(state, byteindex, true);
        case TypedArrayType::Uint8Clamped:
            return getValueFromBuffer<uint8_t>(state, byteindex, true);
        case TypedArrayType::Float32:
            return getValueFromBuffer<float>(state, byteindex, true);
        case TypedArrayType::Float64:
            return getValueFromBuffer<double>(state, byteindex, true);
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    ALWAYS_INLINE bool isDetachedBuffer()
    {
        if (data() == NULL)
            return true;
        return false;
    }

    ALWAYS_INLINE void throwTypeErrorIfDetached(ExecutionState& state)
    {
        if (UNLIKELY(isDetachedBuffer())) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().TypedArray.string(), true, state.context()->staticStrings().constructor.string(), errorMessage_GlobalObject_DetachedBuffer);
        }
    }

    void fillData(const uint8_t* data, unsigned length)
    {
        ASSERT(!isDetachedBuffer());
        memcpy(m_data, data, length);
    }

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

private:
    Context* m_context;
    uint8_t* m_data;
    unsigned m_bytelength;
};
}

#endif
