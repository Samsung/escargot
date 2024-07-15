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

#if defined(ENABLE_THREADING)

#include "Escargot.h"
#include "runtime/Context.h"
#include "runtime/Global.h"
#include "runtime/Platform.h"
#include "runtime/SharedArrayBufferObject.h"
#include "runtime/TypedArrayInlines.h"

namespace Escargot {

SharedArrayBufferObject::SharedArrayBufferObject(ExecutionState& state, Object* proto, size_t byteLength)
    : ArrayBuffer(state, proto)
{
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

    updateBackingStore(BackingStore::createDefaultSharedBackingStore(byteLength));
}

SharedArrayBufferObject::SharedArrayBufferObject(ExecutionState& state, Object* proto, size_t byteLength, size_t maxByteLength)
    : ArrayBuffer(state, proto)
{
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

    updateBackingStore(BackingStore::createDefaultGrowableSharedBackingStore(byteLength, maxByteLength));
}

SharedArrayBufferObject::SharedArrayBufferObject(ExecutionState& state, Object* proto, BackingStore* backingStore)
    : ArrayBuffer(state, proto)
{
    // BackingStore should be valid and shared
    ASSERT(!!backingStore && backingStore->isShared());

    updateBackingStore(BackingStore::createSharedBackingStore(backingStore->sharedDataBlockInfo()));
}

SharedArrayBufferObject::SharedArrayBufferObject(ExecutionState& state, Object* proto, SharedDataBlockInfo* sharedInfo)
    : ArrayBuffer(state, proto)
{
    updateBackingStore(BackingStore::createSharedBackingStore(sharedInfo));
}

SharedArrayBufferObject* SharedArrayBufferObject::allocateSharedArrayBuffer(ExecutionState& state, Object* constructor, uint64_t byteLength, Optional<uint64_t> maxByteLength)
{
    // https://262.ecma-international.org/#sec-allocatesharedarraybuffer
    Object* proto = Object::getPrototypeFromConstructor(state, constructor, [](ExecutionState& state, Context* constructorRealm) -> Object* {
        return constructorRealm->globalObject()->sharedArrayBufferPrototype();
    });

    if (UNLIKELY(byteLength >= ArrayBuffer::maxArrayBufferSize)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, state.context()->staticStrings().SharedArrayBuffer.string(), false, String::emptyString, ErrorObject::Messages::GlobalObject_InvalidArrayBufferSize);
    }

    if (UNLIKELY(maxByteLength.hasValue())) {
        ASSERT(byteLength <= maxByteLength.value());
        uint64_t maxLength = maxByteLength.value();
        if (UNLIKELY(maxLength >= ArrayBuffer::maxArrayBufferSize)) {
            ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, state.context()->staticStrings().SharedArrayBuffer.string(), false, String::emptyString, ErrorObject::Messages::GlobalObject_InvalidArrayBufferSize);
        }

        return new SharedArrayBufferObject(state, proto, byteLength, maxLength);
    }

    return new SharedArrayBufferObject(state, proto, byteLength);
}

SharedArrayBufferObject* SharedArrayBufferObject::allocateExternalSharedArrayBuffer(ExecutionState& state, void* dataBlock, size_t byteLength)
{
    if (UNLIKELY(byteLength >= ArrayBuffer::maxArrayBufferSize)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, state.context()->staticStrings().SharedArrayBuffer.string(), false, String::emptyString, ErrorObject::Messages::GlobalObject_InvalidArrayBufferSize);
    }

    // creating a fixed length memory buffer from memaddr.
    // NOTE) deleter do nothing, dataBlock will be freed in external module
    SharedDataBlockInfo* sharedInfo = new SharedDataBlockInfo(dataBlock, byteLength,
                                                              [](void* data, size_t length, void* deleterData) {});

    return new SharedArrayBufferObject(state, state.context()->globalObject()->sharedArrayBufferPrototype(), sharedInfo);
}

void* SharedArrayBufferObject::operator new(size_t size)
{
    static MAY_THREAD_LOCAL bool typeInited = false;
    static MAY_THREAD_LOCAL GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(SharedArrayBufferObject)] = { 0 };
        Object::fillGCDescriptor(obj_bitmap);
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(SharedArrayBufferObject, m_observerItems));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(SharedArrayBufferObject, m_backingStore));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(SharedArrayBufferObject));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

void SharedArrayBufferObject::fillData(const uint8_t* newData, size_t length)
{
#if defined(HAVE_BUILTIN_ATOMIC_FUNCTIONS)
    uint8_t* rawBytes = ALLOCA(8, uint8_t);
    uint8_t* rawTarget = data();
    for (size_t i = 0; i < length; i++) {
        __atomic_load(newData + i, rawBytes, __ATOMIC_SEQ_CST);
        __atomic_store(rawTarget + i, rawBytes, __ATOMIC_SEQ_CST);
    }
#else
    std::lock_guard<SpinLock> guard(Global::atomicsLock());
    memcpy(data(), newData, length);
#endif
}

Value SharedArrayBufferObject::getValueFromBuffer(ExecutionState& state, size_t byteindex, TypedArrayType type, bool isLittleEndian)
{
    ASSERT(byteLength());
    size_t elemSize = TypedArrayHelper::elementSize(type);
    ASSERT(byteindex + elemSize <= byteLength());

    uint8_t* rawStart = data() + byteindex;
#if defined(HAVE_BUILTIN_ATOMIC_FUNCTIONS)
    uint8_t* rawBytes = ALLOCA(8, uint8_t);

    switch (type) {
    case TypedArrayType::Int8:
        __atomic_load(reinterpret_cast<int8_t*>(rawStart), reinterpret_cast<int8_t*>(rawBytes), __ATOMIC_SEQ_CST);
        break;
    case TypedArrayType::Int16:
        __atomic_load(reinterpret_cast<int16_t*>(rawStart), reinterpret_cast<int16_t*>(rawBytes), __ATOMIC_SEQ_CST);
        break;
    case TypedArrayType::Int32:
        __atomic_load(reinterpret_cast<int32_t*>(rawStart), reinterpret_cast<int32_t*>(rawBytes), __ATOMIC_SEQ_CST);
        break;
    case TypedArrayType::Uint8:
        __atomic_load(reinterpret_cast<uint8_t*>(rawStart), reinterpret_cast<uint8_t*>(rawBytes), __ATOMIC_SEQ_CST);
        break;
    case TypedArrayType::Uint16:
        __atomic_load(reinterpret_cast<uint16_t*>(rawStart), reinterpret_cast<uint16_t*>(rawBytes), __ATOMIC_SEQ_CST);
        break;
    case TypedArrayType::Uint32:
        __atomic_load(reinterpret_cast<uint32_t*>(rawStart), reinterpret_cast<uint32_t*>(rawBytes), __ATOMIC_SEQ_CST);
        break;
    case TypedArrayType::Uint8Clamped:
        __atomic_load(reinterpret_cast<uint8_t*>(rawStart), reinterpret_cast<uint8_t*>(rawBytes), __ATOMIC_SEQ_CST);
        break;
    case TypedArrayType::Float32:
        __atomic_load(reinterpret_cast<float*>(rawStart), reinterpret_cast<float*>(rawBytes), __ATOMIC_SEQ_CST);
        break;
    case TypedArrayType::Float64:
        __atomic_load(reinterpret_cast<double*>(rawStart), reinterpret_cast<double*>(rawBytes), __ATOMIC_SEQ_CST);
        break;
    case TypedArrayType::BigInt64:
        __atomic_load(reinterpret_cast<int64_t*>(rawStart), reinterpret_cast<int64_t*>(rawBytes), __ATOMIC_SEQ_CST);
        break;
    default:
        ASSERT(TypedArrayType::BigUint64 == type);
        __atomic_load(reinterpret_cast<uint64_t*>(rawStart), reinterpret_cast<uint64_t*>(rawBytes), __ATOMIC_SEQ_CST);
        break;
    }

    if (LIKELY(isLittleEndian)) {
        return TypedArrayHelper::rawBytesToNumber(state, type, rawBytes);
    } else {
        uint8_t* rawBytes2 = ALLOCA(8, uint8_t);
        for (size_t i = 0; i < elemSize; i++) {
            rawBytes2[elemSize - i - 1] = rawBytes[i];
        }
        return TypedArrayHelper::rawBytesToNumber(state, type, rawBytes2);
    }
#else
    std::lock_guard<SpinLock> guard(Global::atomicsLock());
    if (LIKELY(isLittleEndian)) {
        return TypedArrayHelper::rawBytesToNumber(state, type, rawStart);
    } else {
        uint8_t* rawBytes = ALLOCA(8, uint8_t);
        for (size_t i = 0; i < elemSize; i++) {
            rawBytes[elemSize - i - 1] = rawStart[i];
        }
        return TypedArrayHelper::rawBytesToNumber(state, type, rawBytes);
    }
#endif
}

void SharedArrayBufferObject::setValueInBuffer(ExecutionState& state, size_t byteindex, TypedArrayType type, const Value& val, bool isLittleEndian)
{
    ASSERT(byteLength());
    size_t elemSize = TypedArrayHelper::elementSize(type);
    ASSERT(byteindex + elemSize <= byteLength());

    uint8_t* rawStart = data() + byteindex;
    uint8_t* rawBytes = ALLOCA(8, uint8_t);
    TypedArrayHelper::numberToRawBytes(state, type, val, rawBytes);
#if defined(HAVE_BUILTIN_ATOMIC_FUNCTIONS)
    uint8_t* rawBytes2 = ALLOCA(8, uint8_t);
    if (LIKELY(isLittleEndian)) {
        rawBytes2 = rawBytes;
    } else {
        for (size_t i = 0; i < elemSize; i++) {
            rawBytes2[i] = rawBytes[elemSize - i - 1];
        }
    }

    switch (type) {
    case TypedArrayType::Int8:
        __atomic_store(reinterpret_cast<int8_t*>(rawStart), reinterpret_cast<int8_t*>(rawBytes2), __ATOMIC_SEQ_CST);
        break;
    case TypedArrayType::Int16:
        __atomic_store(reinterpret_cast<int16_t*>(rawStart), reinterpret_cast<int16_t*>(rawBytes2), __ATOMIC_SEQ_CST);
        break;
    case TypedArrayType::Int32:
        __atomic_store(reinterpret_cast<int32_t*>(rawStart), reinterpret_cast<int32_t*>(rawBytes2), __ATOMIC_SEQ_CST);
        break;
    case TypedArrayType::Uint8:
        __atomic_store(reinterpret_cast<uint8_t*>(rawStart), reinterpret_cast<uint8_t*>(rawBytes2), __ATOMIC_SEQ_CST);
        break;
    case TypedArrayType::Uint16:
        __atomic_store(reinterpret_cast<uint16_t*>(rawStart), reinterpret_cast<uint16_t*>(rawBytes2), __ATOMIC_SEQ_CST);
        break;
    case TypedArrayType::Uint32:
        __atomic_store(reinterpret_cast<uint32_t*>(rawStart), reinterpret_cast<uint32_t*>(rawBytes2), __ATOMIC_SEQ_CST);
        break;
    case TypedArrayType::Uint8Clamped:
        __atomic_store(reinterpret_cast<uint8_t*>(rawStart), reinterpret_cast<uint8_t*>(rawBytes2), __ATOMIC_SEQ_CST);
        break;
    case TypedArrayType::Float32:
        __atomic_store(reinterpret_cast<float*>(rawStart), reinterpret_cast<float*>(rawBytes2), __ATOMIC_SEQ_CST);
        break;
    case TypedArrayType::Float64:
        __atomic_store(reinterpret_cast<double*>(rawStart), reinterpret_cast<double*>(rawBytes2), __ATOMIC_SEQ_CST);
        break;
    case TypedArrayType::BigInt64:
        __atomic_store(reinterpret_cast<int64_t*>(rawStart), reinterpret_cast<int64_t*>(rawBytes2), __ATOMIC_SEQ_CST);
        break;
    default:
        ASSERT(TypedArrayType::BigUint64 == type);
        __atomic_store(reinterpret_cast<uint64_t*>(rawStart), reinterpret_cast<uint64_t*>(rawBytes2), __ATOMIC_SEQ_CST);
        break;
    }
#else
    std::lock_guard<SpinLock> guard(Global::atomicsLock());
    if (LIKELY(isLittleEndian)) {
        memcpy(rawStart, rawBytes, elemSize);
    } else {
        for (size_t i = 0; i < elemSize; i++) {
            rawStart[i] = rawBytes[elemSize - i - 1];
        }
    }
#endif
}

size_t SharedArrayBufferObject::byteLengthRMW(size_t newByteLength)
{
    ASSERT(m_backingStore.hasValue());
    return m_backingStore->byteLengthRMW(newByteLength);
}
} // namespace Escargot

#endif
