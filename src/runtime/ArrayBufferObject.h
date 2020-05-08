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
#include "runtime/TypedArrayInline.h"

namespace Escargot {

class ArrayBufferObject : public Object {
    friend void initializeCustomAllocators();
    friend int getValidValueInArrayBufferObject(void* ptr, GC_mark_custom_result* arr);
    friend void careArrayBufferObjectsOnGCEvent(GC_EventType evtType);

public:
    explicit ArrayBufferObject(ExecutionState& state);
    explicit ArrayBufferObject(ExecutionState& state, Object* proto);

    static ArrayBufferObject* allocateArrayBuffer(ExecutionState& state, Object* constructor, size_t byteLength);
    static ArrayBufferObject* cloneArrayBuffer(ExecutionState& state, ArrayBufferObject* srcBuffer, size_t srcByteOffset, size_t srcLength, Object* constructor);

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
    Value getValueFromBuffer(ExecutionState& state, size_t byteindex, TypedArrayType type, bool isLittleEndian = true)
    {
        // If isLittleEndian is not present, set isLittleEndian to either true or false.
        ASSERT(m_bytelength);
        size_t elemSize = elementSize(type);
        ASSERT(byteindex + elemSize <= m_bytelength);
        uint8_t* rawStart = m_data + byteindex;
        if (LIKELY(isLittleEndian)) {
            return rawBytesToNumber(state, type, rawStart);
        } else {
            uint8_t* rawBytes = ALLOCA(8, uint8_t, state);
            for (size_t i = 0; i < elemSize; i++) {
                rawBytes[elemSize - i - 1] = rawStart[i];
            }
            return rawBytesToNumber(state, type, rawBytes);
        }
    }

    // $24.1.1.8
    void setValueInBuffer(ExecutionState& state, size_t byteindex, TypedArrayType type, const Value& val, bool isLittleEndian = true)
    {
        // If isLittleEndian is not present, set isLittleEndian to either true or false.
        ASSERT(m_bytelength);
        size_t elemSize = elementSize(type);
        ASSERT(byteindex + elemSize <= m_bytelength);
        uint8_t* rawStart = m_data + byteindex;
        uint8_t* rawBytes = ALLOCA(8, uint8_t, state);
        numberToRawBytes(state, type, val, rawBytes);
        if (LIKELY(isLittleEndian)) {
            memcpy(rawStart, rawBytes, elemSize);
        } else {
            for (size_t i = 0; i < elemSize; i++) {
                rawStart[i] = rawBytes[elemSize - i - 1];
            }
        }
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
    size_t elementSize(TypedArrayType type)
    {
        switch (type) {
        case TypedArrayType::Int8:
        case TypedArrayType::Uint8:
        case TypedArrayType::Uint8Clamped:
            return 1;
        case TypedArrayType::Int16:
        case TypedArrayType::Uint16:
            return 2;
        case TypedArrayType::Int32:
        case TypedArrayType::Uint32:
        case TypedArrayType::Float32:
            return 4;
        case TypedArrayType::Float64:
            return 8;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    }

    Value rawBytesToNumber(ExecutionState& state, TypedArrayType type, uint8_t* rawBytes)
    {
        switch (type) {
        case TypedArrayType::Int8:
            return Value(*reinterpret_cast<Int8Adaptor::Type*>(rawBytes));
        case TypedArrayType::Uint8:
            return Value(*reinterpret_cast<Uint8Adaptor::Type*>(rawBytes));
        case TypedArrayType::Uint8Clamped:
            return Value(*reinterpret_cast<Uint8ClampedAdaptor::Type*>(rawBytes));
        case TypedArrayType::Int16:
            return Value(*reinterpret_cast<Int16Adaptor::Type*>(rawBytes));
        case TypedArrayType::Uint16:
            return Value(*reinterpret_cast<Uint16Adaptor::Type*>(rawBytes));
        case TypedArrayType::Int32:
            return Value(*reinterpret_cast<Int32Adaptor::Type*>(rawBytes));
        case TypedArrayType::Uint32:
            return Value(*reinterpret_cast<Uint32Adaptor::Type*>(rawBytes));
        case TypedArrayType::Float32:
            return Value(*reinterpret_cast<Float32Adaptor::Type*>(rawBytes));
        case TypedArrayType::Float64:
            return Value(*reinterpret_cast<Float64Adaptor::Type*>(rawBytes));
        default:
            RELEASE_ASSERT_NOT_REACHED();
            return Value();
        }
    }

    void numberToRawBytes(ExecutionState& state, TypedArrayType type, const Value& val, uint8_t* rawBytes)
    {
        switch (type) {
        case TypedArrayType::Int8:
            *reinterpret_cast<Int8Adaptor::Type*>(rawBytes) = Int8Adaptor::toNative(state, val);
            break;
        case TypedArrayType::Uint8:
            *reinterpret_cast<Uint8Adaptor::Type*>(rawBytes) = Uint8Adaptor::toNative(state, val);
            break;
        case TypedArrayType::Uint8Clamped:
            *reinterpret_cast<Uint8ClampedAdaptor::Type*>(rawBytes) = Uint8ClampedAdaptor::toNative(state, val);
            break;
        case TypedArrayType::Int16:
            *reinterpret_cast<Int16Adaptor::Type*>(rawBytes) = Int16Adaptor::toNative(state, val);
            break;
        case TypedArrayType::Uint16:
            *reinterpret_cast<Uint16Adaptor::Type*>(rawBytes) = Uint16Adaptor::toNative(state, val);
            break;
        case TypedArrayType::Int32:
            *reinterpret_cast<Int32Adaptor::Type*>(rawBytes) = Int32Adaptor::toNative(state, val);
            break;
        case TypedArrayType::Uint32:
            *reinterpret_cast<Uint32Adaptor::Type*>(rawBytes) = Uint32Adaptor::toNative(state, val);
            break;
        case TypedArrayType::Float32:
            *reinterpret_cast<Float32Adaptor::Type*>(rawBytes) = Float32Adaptor::toNative(state, val);
            break;
        case TypedArrayType::Float64:
            *reinterpret_cast<Float64Adaptor::Type*>(rawBytes) = Float64Adaptor::toNative(state, val);
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    }


    Context* m_context;
    uint8_t* m_data;
    size_t m_bytelength;
};
}

#endif
