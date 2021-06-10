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

#include "Escargot.h"
#include "runtime/VMInstance.h"
#include "runtime/ArrayBufferObject.h"
#include "runtime/TypedArrayInlines.h"

namespace Escargot {

unsigned TypedArrayHelper::elementSizeTable[11] = { 1, 2, 4, 1, 2, 4, 1, 4, 8, 8, 8 };

ArrayBufferObject* ArrayBufferObject::allocateArrayBuffer(ExecutionState& state, Object* constructor, uint64_t byteLength)
{
    // https://www.ecma-international.org/ecma-262/10.0/#sec-allocatearraybuffer
    Object* proto = Object::getPrototypeFromConstructor(state, constructor, [](ExecutionState& state, Context* constructorRealm) -> Object* {
        return constructorRealm->globalObject()->arrayBufferPrototype();
    });

    if (byteLength >= (uint64_t)ArrayBufferObject::maxArrayBufferSize) {
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, state.context()->staticStrings().ArrayBuffer.string(), false, String::emptyString, ErrorObject::Messages::GlobalObject_InvalidArrayBufferSize);
    }

    ArrayBufferObject* obj = new ArrayBufferObject(state, proto);
    obj->allocateBuffer(state, byteLength);

    return obj;
}

ArrayBufferObject* ArrayBufferObject::cloneArrayBuffer(ExecutionState& state, ArrayBufferObject* srcBuffer, size_t srcByteOffset, uint64_t srcLength, Object* constructor)
{
    // https://www.ecma-international.org/ecma-262/10.0/#sec-clonearraybuffer
    ASSERT(constructor->isConstructor());

    ArrayBufferObject* targetBuffer = ArrayBufferObject::allocateArrayBuffer(state, constructor, srcLength);
    srcBuffer->throwTypeErrorIfDetached(state);

    targetBuffer->fillData(srcBuffer->data() + srcByteOffset, srcLength);

    return targetBuffer;
}

ArrayBufferObject::ArrayBufferObject(ExecutionState& state)
    : ArrayBufferObject(state, state.context()->globalObject()->arrayBufferPrototype())
{
}

ArrayBufferObject::ArrayBufferObject(ExecutionState& state, Object* proto)
    : Object(state, proto, ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER)
    , m_data(nullptr)
    , m_byteLength(0)
    , m_mayPointsSharedBackingStore(false)
{
}

void ArrayBufferObject::allocateBuffer(ExecutionState& state, size_t byteLength)
{
    detachArrayBuffer();

    ASSERT(byteLength < (size_t)ArrayBufferObject::maxArrayBufferSize);

    const size_t ratio = std::max((size_t)GC_get_free_space_divisor() / 6, (size_t)1);
    if (byteLength > (GC_get_heap_size() / ratio)) {
        size_t n = 0;
        size_t times = byteLength / (GC_get_heap_size() / ratio) / 3;
        do {
            GC_gcollect_and_unmap();
            n += 1;
        } while (n < times);
        GC_invoke_finalizers();
    }

    auto platform = state.context()->vmInstance()->platform();
    void* buffer = platform->onMallocArrayBufferObjectDataBuffer(byteLength);
    m_backingStore = new BackingStore(buffer, byteLength, [](void* data, size_t length, void* deleterData) {
        Platform* platform = (Platform*)deleterData;
        platform->onFreeArrayBufferObjectDataBuffer(data, length);
    },
                                      platform);
    m_data = (uint8_t*)m_backingStore->data();
    m_byteLength = byteLength;
}

void ArrayBufferObject::attachBuffer(BackingStore* backingStore)
{
    detachArrayBuffer();

    m_mayPointsSharedBackingStore = true;
    m_backingStore = backingStore;
    m_data = (uint8_t*)backingStore->data();
    m_byteLength = backingStore->byteLength();
}

void ArrayBufferObject::detachArrayBuffer()
{
    if (m_data && !m_mayPointsSharedBackingStore) {
        // if backingstore is definitely not shared, we deallocate the backingstore immediately.
        m_backingStore.value()->deallocate();
    }
    m_data = nullptr;
    m_byteLength = 0;
    m_backingStore.reset();
    m_mayPointsSharedBackingStore = false;
}

void* ArrayBufferObject::operator new(size_t size)
{
#ifdef NDEBUG
    static bool typeInited = false;
    static GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(ArrayBufferObject)] = { 0 };
        Object::fillGCDescriptor(obj_bitmap);
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ArrayBufferObject, m_backingStore));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(ArrayBufferObject));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
#else
    return CustomAllocator<ArrayBufferObject>().allocate(1);
#endif
}

// $24.1.1.6
Value ArrayBufferObject::getValueFromBuffer(ExecutionState& state, size_t byteindex, TypedArrayType type, bool isLittleEndian)
{
    // If isLittleEndian is not present, set isLittleEndian to either true or false.
    ASSERT(m_byteLength);
    size_t elemSize = TypedArrayHelper::elementSize(type);
    ASSERT(byteindex + elemSize <= m_byteLength);
    uint8_t* rawStart = m_data + byteindex;
    if (LIKELY(isLittleEndian)) {
        return TypedArrayHelper::rawBytesToNumber(state, type, rawStart);
    } else {
        uint8_t* rawBytes = ALLOCA(8, uint8_t, state);
        for (size_t i = 0; i < elemSize; i++) {
            rawBytes[elemSize - i - 1] = rawStart[i];
        }
        return TypedArrayHelper::rawBytesToNumber(state, type, rawBytes);
    }
}

// $24.1.1.8
void ArrayBufferObject::setValueInBuffer(ExecutionState& state, size_t byteindex, TypedArrayType type, const Value& val, bool isLittleEndian)
{
    // If isLittleEndian is not present, set isLittleEndian to either true or false.
    ASSERT(m_byteLength);
    size_t elemSize = TypedArrayHelper::elementSize(type);
    ASSERT(byteindex + elemSize <= m_byteLength);
    uint8_t* rawStart = m_data + byteindex;
    uint8_t* rawBytes = ALLOCA(8, uint8_t, state);
    TypedArrayHelper::numberToRawBytes(state, type, val, rawBytes);
    if (LIKELY(isLittleEndian)) {
        memcpy(rawStart, rawBytes, elemSize);
    } else {
        for (size_t i = 0; i < elemSize; i++) {
            rawStart[i] = rawBytes[elemSize - i - 1];
        }
    }
}
} // namespace Escargot
