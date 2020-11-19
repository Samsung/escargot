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

namespace Escargot {

enum class TypedArrayType : unsigned {
    Int8 = 0,
    Int16,
    Int32,
    Uint8,
    Uint16,
    Uint32,
    Uint8Clamped,
    Float32,
    Float64,
    BigInt64,
    BigUint64
};

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

class ArrayBufferView : public Object {
public:
    explicit ArrayBufferView(ExecutionState& state, Object* proto)
        : Object(state, proto, ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER)
        , m_buffer(nullptr)
        , m_byteLength(0)
        , m_byteOffset(0)
        , m_arrayLength(0)
    {
    }

    ALWAYS_INLINE ArrayBufferObject* buffer() { return m_buffer; }
    ALWAYS_INLINE size_t byteLength() { return m_byteLength; }
    ALWAYS_INLINE size_t byteOffset() { return m_byteOffset; }
    ALWAYS_INLINE size_t arrayLength() { return m_arrayLength; }
    ALWAYS_INLINE uint8_t* rawBuffer()
    {
        return m_buffer ? (uint8_t*)(m_buffer->data() + m_byteOffset) : nullptr;
    }

    ALWAYS_INLINE void setBuffer(ArrayBufferObject* bo, size_t byteOffset, size_t byteLength, size_t arrayLength)
    {
        m_buffer = bo;
        m_byteOffset = byteOffset;
        m_byteLength = byteLength;
        m_arrayLength = arrayLength;
    }

    ALWAYS_INLINE void setBuffer(ArrayBufferObject* bo, size_t byteOffset, size_t byteLength)
    {
        m_buffer = bo;
        m_byteOffset = byteOffset;
        m_byteLength = byteLength;
    }

    virtual bool isArrayBufferView() const override
    {
        return true;
    }

    void* operator new(size_t size)
    {
        static bool typeInited = false;
        static GC_descr descr;
        if (!typeInited) {
            GC_word obj_bitmap[GC_BITMAP_SIZE(ArrayBufferView)] = { 0 };
            Object::fillGCDescriptor(obj_bitmap);
            GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ArrayBufferView, m_buffer));
            descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(ArrayBufferView));
            typeInited = true;
        }
        return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
    }
    void* operator new[](size_t size) = delete;

private:
    ArrayBufferObject* m_buffer;
    size_t m_byteLength;
    size_t m_byteOffset;
    size_t m_arrayLength;
};
}

#endif
