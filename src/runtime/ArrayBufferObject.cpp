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
#include "runtime/BackingStore.h"
#include "runtime/ArrayBufferObject.h"
#include "runtime/TypedArrayInlines.h"

namespace Escargot {

unsigned TypedArrayHelper::elementSizeTable[11] = { 1, 2, 4, 1, 2, 4, 1, 4, 8, 8, 8 };

ArrayBufferObject* ArrayBufferObject::allocateArrayBuffer(ExecutionState& state, Object* constructor, uint64_t byteLength, Optional<uint64_t> maxByteLength, bool resizable)
{
    // https://www.ecma-international.org/ecma-262/10.0/#sec-allocatearraybuffer
    Object* proto = Object::getPrototypeFromConstructor(state, constructor, [](ExecutionState& state, Context* constructorRealm) -> Object* {
        return constructorRealm->globalObject()->arrayBufferPrototype();
    });

    if (UNLIKELY(byteLength >= ArrayBuffer::maxArrayBufferSize)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, state.context()->staticStrings().ArrayBuffer.string(), false, String::emptyString, ErrorObject::Messages::GlobalObject_InvalidArrayBufferSize);
    }

    ArrayBufferObject* obj = new ArrayBufferObject(state, proto);

    if (UNLIKELY(maxByteLength.hasValue())) {
        ASSERT(byteLength <= maxByteLength.value());
        uint64_t maxLength = maxByteLength.value();
        if (UNLIKELY(maxLength >= ArrayBuffer::maxArrayBufferSize)) {
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, state.context()->staticStrings().ArrayBuffer.string(), false, String::emptyString, ErrorObject::Messages::GlobalObject_InvalidArrayBufferSize);
        }

        // allocate default resizable non-shared BackingStore
        if (resizable) {
            obj->allocateResizableBuffer(state, byteLength, maxLength);
        } else {
            obj->allocateBuffer(state, byteLength);
        }
        return obj;
    }

    // allocate default non-shared BackingStore
    obj->allocateBuffer(state, byteLength);
    return obj;
}

ArrayBufferObject* ArrayBufferObject::allocateExternalArrayBuffer(ExecutionState& state, void* dataBlock, size_t byteLength)
{
    if (UNLIKELY(byteLength >= ArrayBuffer::maxArrayBufferSize)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, state.context()->staticStrings().ArrayBuffer.string(), false, String::emptyString, ErrorObject::Messages::GlobalObject_InvalidArrayBufferSize);
    }

    // creating a fixed length memory buffer from memaddr.
    // NOTE) deleter do nothing, dataBlock will be freed in external module
    BackingStore* backingStore = BackingStore::createNonSharedBackingStore(dataBlock, byteLength,
                                                                           [](void* data, size_t length, void* deleterData) {}, nullptr);

    return new ArrayBufferObject(state, backingStore);
}

ArrayBufferObject* ArrayBufferObject::cloneArrayBuffer(ExecutionState& state, ArrayBuffer* srcBuffer, size_t srcByteOffset, uint64_t srcLength, Object* constructor)
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
    : ArrayBuffer(state, proto)
{
}

ArrayBufferObject::ArrayBufferObject(ExecutionState& state, BackingStore* backingStore)
    : ArrayBufferObject(state)
{
    // BackingStore should be valid and non-shared
    ASSERT(!!backingStore && !backingStore->isShared());

    updateBackingStore(backingStore);
}

void ArrayBufferObject::allocateBuffer(ExecutionState& state, size_t byteLength)
{
    detachArrayBuffer();

    ASSERT(byteLength < ArrayBuffer::maxArrayBufferSize);

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

    updateBackingStore(BackingStore::createDefaultNonSharedBackingStore(byteLength));
}

void ArrayBufferObject::allocateResizableBuffer(ExecutionState& state, size_t byteLength, size_t maxByteLength)
{
    detachArrayBuffer();

    ASSERT(byteLength <= maxByteLength);
    ASSERT(maxByteLength < ArrayBuffer::maxArrayBufferSize);

    const size_t ratio = std::max((size_t)GC_get_free_space_divisor() / 6, (size_t)1);
    if (maxByteLength > (GC_get_heap_size() / ratio)) {
        size_t n = 0;
        size_t times = maxByteLength / (GC_get_heap_size() / ratio) / 3;
        do {
            GC_gcollect_and_unmap();
            n += 1;
        } while (n < times);
        GC_invoke_finalizers();
    }

    updateBackingStore(BackingStore::createDefaultResizableNonSharedBackingStore(byteLength, maxByteLength));
}

void ArrayBufferObject::attachBuffer(BackingStore* backingStore)
{
    // BackingStore should not be Shared Data Block
    ASSERT(!backingStore->isShared());
    detachArrayBuffer();

    updateBackingStore(backingStore);
}

void ArrayBufferObject::detachArrayBuffer()
{
    updateBackingStore(nullptr);
}

void* ArrayBufferObject::operator new(size_t size)
{
#ifdef NDEBUG
    static MAY_THREAD_LOCAL bool typeInited = false;
    static MAY_THREAD_LOCAL GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(ArrayBufferObject)] = { 0 };
        Object::fillGCDescriptor(obj_bitmap);
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ArrayBufferObject, m_observerItems));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ArrayBufferObject, m_backingStore));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(ArrayBufferObject));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
#else
    return CustomAllocator<ArrayBufferObject>().allocate(1);
#endif
}

Value ArrayBufferObject::getValueFromBuffer(ExecutionState& state, size_t byteindex, TypedArrayType type, bool isLittleEndian)
{
    // If isLittleEndian is not present, set isLittleEndian to either true or false.
    ASSERT(byteLength());
    size_t elemSize = TypedArrayHelper::elementSize(type);
    ASSERT(byteindex + elemSize <= byteLength());
    uint8_t* rawStart = data() + byteindex;
    if (LIKELY(isLittleEndian)) {
        return TypedArrayHelper::rawBytesToNumber(state, type, rawStart);
    } else {
        uint8_t* rawBytes = ALLOCA(8, uint8_t);
        for (size_t i = 0; i < elemSize; i++) {
            rawBytes[elemSize - i - 1] = rawStart[i];
        }
        return TypedArrayHelper::rawBytesToNumber(state, type, rawBytes);
    }
}

void ArrayBufferObject::setValueInBuffer(ExecutionState& state, size_t byteindex, TypedArrayType type, const Value& val, bool isLittleEndian)
{
    // If isLittleEndian is not present, set isLittleEndian to either true or false.
    ASSERT(byteLength());
    size_t elemSize = TypedArrayHelper::elementSize(type);
    ASSERT(byteindex + elemSize <= byteLength());
    uint8_t* rawStart = data() + byteindex;
    uint8_t* rawBytes = ALLOCA(8, uint8_t);
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
