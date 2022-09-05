/*
 * Copyright (c) 2021-present Samsung Electronics Co., Ltd
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

#ifndef __EscargotArrayBuffer__
#define __EscargotArrayBuffer__

#include "runtime/Context.h"
#include "runtime/BackingStore.h"

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

class ArrayBuffer : public DerivedObject, public BufferAddressObserverManager<ArrayBuffer> {
    friend int getValidValueInArrayBufferObject(void* ptr, GC_mark_custom_result* arr);

public:
    static const uint64_t maxArrayBufferSize = 210000000;

    explicit ArrayBuffer(ExecutionState& state, Object* proto)
        : DerivedObject(state, proto, ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER)
        , BufferAddressObserverManager<ArrayBuffer>(this)
    {
        addFinalizer(arrayBufferFinalizer, nullptr);
    }

    virtual bool isArrayBuffer() const override
    {
        return true;
    }

    // pure virtual function to ensure that ArrayBuffer instance never created
    // all these pure virtual functions guarantee thread-safe operations
    virtual void fillData(const uint8_t* newData, size_t length) = 0;
    virtual Value getValueFromBuffer(ExecutionState& state, size_t byteindex, TypedArrayType type, bool isLittleEndian = true) = 0;
    virtual void setValueInBuffer(ExecutionState& state, size_t byteindex, TypedArrayType type, const Value& val, bool isLittleEndian = true) = 0;

    Optional<BackingStore*> backingStore()
    {
        return m_backingStore;
    }

    ALWAYS_INLINE uint8_t* data()
    {
        if (LIKELY(m_backingStore)) {
            return reinterpret_cast<uint8_t*>(m_backingStore->data());
        }
        return nullptr;
    }

    ALWAYS_INLINE size_t byteLength()
    {
        if (LIKELY(m_backingStore.hasValue())) {
            return m_backingStore->byteLength();
        }
        return 0;
    }

    ALWAYS_INLINE size_t maxByteLength()
    {
        ASSERT(m_backingStore && m_backingStore->isResizable());
        return m_backingStore->maxByteLength();
    }

    ALWAYS_INLINE bool isDetachedBuffer()
    {
        return (data() == nullptr);
    }

    ALWAYS_INLINE bool isResizableArrayBuffer() const
    {
        if (LIKELY(m_backingStore)) {
            return m_backingStore->isResizable();
        }
        return false;
    }

    ALWAYS_INLINE void throwTypeErrorIfDetached(ExecutionState& state)
    {
        if (UNLIKELY(isDetachedBuffer())) {
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().TypedArray.string(), true, state.context()->staticStrings().constructor.string(), ErrorObject::Messages::GlobalObject_DetachedBuffer);
        }
    }

    void* operator new(size_t size) = delete;
    void* operator new[](size_t size) = delete;

protected:
    static void arrayBufferFinalizer(Object* obj, void* data)
    {
        ArrayBuffer* self = reinterpret_cast<ArrayBuffer*>(obj);
        self->dispose();
    }

    static void backingStoreObserver(BackingStore* bufferOwner, void* newAddress, size_t newByteLength, void* userData)
    {
        reinterpret_cast<ArrayBuffer*>(userData)->bufferUpdated(newAddress, newByteLength);
    }

    void updateBackingStore(Optional<BackingStore*> bs)
    {
        if (m_backingStore) {
            m_backingStore->removeObserver(this, backingStoreObserver, this);
        }
        m_backingStore = bs;
        if (m_backingStore) {
            m_backingStore->addObserver(this, backingStoreObserver, this);
            bufferUpdated(m_backingStore->data(), m_backingStore->byteLength());
        } else {
            bufferUpdated(nullptr, 0);
        }
    }

    Optional<BackingStore*> m_backingStore;
};

class ArrayBufferView : public DerivedObject {
public:
    explicit ArrayBufferView(ExecutionState& state, Object* proto)
        : DerivedObject(state, proto, ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER)
        , m_buffer(nullptr)
        , m_cachedRawBufferAddress(nullptr)
        , m_byteLength(0)
        , m_byteOffset(0)
        , m_arrayLength(0)
    {
    }

    ALWAYS_INLINE ArrayBuffer* buffer() { return m_buffer; }
    ALWAYS_INLINE size_t byteLength() { return m_byteLength; }
    ALWAYS_INLINE size_t byteOffset() { return m_byteOffset; }
    ALWAYS_INLINE size_t arrayLength() { return m_arrayLength; }
    ALWAYS_INLINE uint8_t* rawBuffer()
    {
        return m_cachedRawBufferAddress;
    }

    ALWAYS_INLINE void setBuffer(ArrayBuffer* bo, size_t byteOffset, size_t byteLength, size_t arrayLength)
    {
        if (m_buffer) {
            m_buffer->removeObserver(this, backingStoreObserver, this);
        }
        m_buffer = bo;
        m_byteOffset = byteOffset;
        m_byteLength = byteLength;
        m_arrayLength = arrayLength;
        updateCacheAddress();
        if (m_buffer) {
            m_buffer->addObserver(this, backingStoreObserver, this);
        }
    }

    ALWAYS_INLINE void setBuffer(ArrayBuffer* bo, size_t byteOffset, size_t byteLength)
    {
        setBuffer(bo, byteOffset, byteLength, 0);
    }

    virtual bool isArrayBufferView() const override
    {
        return true;
    }

    void* operator new(size_t size)
    {
        static MAY_THREAD_LOCAL bool typeInited = false;
        static MAY_THREAD_LOCAL GC_descr descr;
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
    void updateCacheAddress()
    {
        m_cachedRawBufferAddress = (m_buffer && m_buffer->data()) ? m_buffer->data() + m_byteOffset : nullptr;
    }

    static void backingStoreObserver(ArrayBuffer* bufferOwner, void* newAddress, size_t newByteLength, void* userData)
    {
        ArrayBufferView* self = reinterpret_cast<ArrayBufferView*>(userData);
        self->updateCacheAddress();
        if (self->m_byteLength + self->m_byteOffset != newByteLength) {
            if (newByteLength > self->m_byteOffset) {
                size_t ratio = self->m_arrayLength ? self->m_byteLength / self->m_arrayLength : SIZE_MAX;
                self->m_byteLength = newByteLength - self->m_byteOffset;
                if (ratio == SIZE_MAX) {
                    self->m_arrayLength = 0;
                } else {
                    self->m_arrayLength = self->m_byteLength / ratio;
                }
            } else {
                self->m_arrayLength = self->m_byteLength = 0;
            }
        }
    }

    ArrayBuffer* m_buffer;
    uint8_t* m_cachedRawBufferAddress;
    size_t m_byteLength;
    size_t m_byteOffset;
    size_t m_arrayLength;
};

} // namespace Escargot

#endif
