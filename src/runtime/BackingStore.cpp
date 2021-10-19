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
 *  Lesser General Public License for more detaials.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 */

#include "Escargot.h"
#include "BackingStore.h"
#include "runtime/Global.h"
#include "runtime/Platform.h"

namespace Escargot {

static void backingStorePlatformDeleter(void* data, size_t length, void* deleterData)
{
    if (!!data) {
        Global::platform()->onFreeArrayBufferObjectDataBuffer(data, length);
    }
}

BackingStore* BackingStore::createDefaultNonSharedBackingStore(size_t byteLength)
{
    return new NonSharedBackingStore(
        Global::platform()->onMallocArrayBufferObjectDataBuffer(byteLength),
        byteLength, backingStorePlatformDeleter, nullptr, true);
}

BackingStore* BackingStore::createDefaultResizableNonSharedBackingStore(size_t byteLength, size_t maxByteLength)
{
    // Resizable BackingStore is allocated by Platform only
    return new NonSharedBackingStore(
        Global::platform()->onMallocArrayBufferObjectDataBuffer(byteLength),
        byteLength, backingStorePlatformDeleter, maxByteLength, true);
}

BackingStore* BackingStore::createNonSharedBackingStore(void* data, size_t byteLength, BackingStoreDeleterCallback callback, void* callbackData)
{
    return new NonSharedBackingStore(data, byteLength, callback, callbackData, false);
}

NonSharedBackingStore::NonSharedBackingStore(void* data, size_t byteLength, BackingStoreDeleterCallback callback, void* callbackData, bool isAllocatedByPlatform)
    : m_data(data)
    , m_byteLength(byteLength)
    , m_deleter(callback)
    , m_deleterData(callbackData)
    , m_isAllocatedByPlatform(isAllocatedByPlatform)
    , m_isResizable(false)
{
    GC_REGISTER_FINALIZER_NO_ORDER(this, [](void* obj, void*) {
        NonSharedBackingStore* self = (NonSharedBackingStore*)obj;
        self->m_deleter(self->m_data, self->m_byteLength, self->m_deleterData);
    },
                                   nullptr, nullptr, nullptr);
}

NonSharedBackingStore::NonSharedBackingStore(void* data, size_t byteLength, BackingStoreDeleterCallback callback, size_t maxByteLength, bool isAllocatedByPlatform)
    : m_data(data)
    , m_byteLength(byteLength)
    , m_deleter(callback)
    , m_maxByteLength(maxByteLength)
    , m_isAllocatedByPlatform(isAllocatedByPlatform)
    , m_isResizable(true)
{
    ASSERT(isAllocatedByPlatform);

    GC_REGISTER_FINALIZER_NO_ORDER(this, [](void* obj, void*) {
        NonSharedBackingStore* self = (NonSharedBackingStore*)obj;
        self->m_deleter(self->m_data, self->m_maxByteLength, nullptr);
    },
                                   nullptr, nullptr, nullptr);
}

void* NonSharedBackingStore::operator new(size_t size)
{
    static MAY_THREAD_LOCAL bool typeInited = false;
    static MAY_THREAD_LOCAL GC_descr descr;
    if (!typeInited) {
        // only mark m_deleterData
        GC_word obj_bitmap[GC_BITMAP_SIZE(NonSharedBackingStore)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(NonSharedBackingStore, m_deleterData));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(NonSharedBackingStore));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

void NonSharedBackingStore::resize(size_t newByteLength)
{
    ASSERT(m_isResizable && newByteLength <= m_maxByteLength);

    if (m_byteLength < newByteLength) {
        memset(static_cast<uint8_t*>(m_data) + m_byteLength, 0, newByteLength - m_byteLength);
    }

    m_byteLength = newByteLength;
}

void NonSharedBackingStore::reallocate(size_t newByteLength)
{
    if (m_byteLength == newByteLength) {
        return;
    }

    if (m_isAllocatedByPlatform) {
        m_data = Global::platform()->onReallocArrayBufferObjectDataBuffer(m_data, m_byteLength, newByteLength);
        m_byteLength = newByteLength;
    } else {
        // Note) even if BackingStore was previously allocated by other allocator,
        // newly reallocate it with default Platform allocator
        m_deleter(m_data, m_byteLength, m_deleterData);
        m_data = Global::platform()->onMallocArrayBufferObjectDataBuffer(newByteLength);
        m_deleter = backingStorePlatformDeleter;
        m_deleterData = nullptr;
        m_byteLength = newByteLength;
        m_isAllocatedByPlatform = true;
    }
}

#if defined(ENABLE_THREADING)
BackingStore* BackingStore::createDefaultSharedBackingStore(size_t byteLength)
{
    SharedDataBlockInfo* sharedInfo = new SharedDataBlockInfo(
        Global::platform()->onMallocArrayBufferObjectDataBuffer(byteLength),
        byteLength);
    return new SharedBackingStore(sharedInfo);
}

BackingStore* BackingStore::createDefaultGrowableSharedBackingStore(size_t byteLength, size_t maxByteLength)
{
    SharedDataBlockInfo* sharedInfo = new GrowableSharedDataBlockInfo(
        Global::platform()->onMallocArrayBufferObjectDataBuffer(maxByteLength),
        byteLength, maxByteLength);
    return new SharedBackingStore(sharedInfo);
}

BackingStore* BackingStore::createSharedBackingStore(SharedDataBlockInfo* sharedInfo)
{
    ASSERT(sharedInfo->hasValidReference());
    return new SharedBackingStore(sharedInfo);
}

void SharedDataBlockInfo::deref()
{
    ASSERT(hasValidReference());

    auto oldValue = m_refCount.fetch_sub(1);
    if (oldValue == 1) {
        if (isGrowable()) {
            Global::platform()->onFreeArrayBufferObjectDataBuffer(m_data, maxByteLength());
        } else {
            Global::platform()->onFreeArrayBufferObjectDataBuffer(m_data, m_byteLength);
        }

        m_data = nullptr;
        m_byteLength = 0;

        delete this;
        return;
    }
}

SharedBackingStore::SharedBackingStore(SharedDataBlockInfo* sharedInfo)
    : m_sharedDataBlockInfo(sharedInfo)
{
    // increase reference count
    sharedInfo->ref();

    GC_REGISTER_FINALIZER_NO_ORDER(this, [](void* obj, void*) {
        SharedBackingStore* self = (SharedBackingStore*)obj;
        self->sharedDataBlockInfo()->deref();
    },
                                   nullptr, nullptr, nullptr);
}

void* SharedBackingStore::operator new(size_t size)
{
    // SharedBackingStore does not have any GC member
    return GC_MALLOC_ATOMIC(size);
}
#endif

} // namespace Escargot
