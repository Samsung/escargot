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
#include "heap/CustomAllocator.h"
#include "runtime/Global.h"
#include "runtime/Platform.h"

namespace Escargot {

static void backingStorePlatformDeleter(void* data, size_t length, void* deleterData)
{
    if (!!data) {
        Global::platform()->onFreeArrayBufferObjectDataBuffer(data, length, deleterData);
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
        Global::platform()->onMallocArrayBufferObjectDataBuffer(maxByteLength),
        byteLength, backingStorePlatformDeleter, maxByteLength, true);
}

BackingStore* BackingStore::createNonSharedBackingStore(void* data, size_t byteLength, BackingStoreDeleterCallback deleter, void* callbackData)
{
    return new NonSharedBackingStore(data, byteLength, deleter, callbackData, false);
}

NonSharedBackingStore::NonSharedBackingStore(void* data, size_t byteLength, BackingStoreDeleterCallback deleter, void* callbackData, bool isAllocatedByPlatform)
    : m_data(data)
    , m_byteLength(byteLength)
    , m_deleter(deleter)
    , m_deleterData(callbackData)
    , m_isAllocatedByPlatform(isAllocatedByPlatform)
    , m_isResizable(false)
{
}

NonSharedBackingStore::NonSharedBackingStore(void* data, size_t byteLength, BackingStoreDeleterCallback deleter, size_t maxByteLength, bool isAllocatedByPlatform)
    : m_data(data)
    , m_byteLength(byteLength)
    , m_deleter(deleter)
    , m_maxByteLength(maxByteLength)
    , m_isAllocatedByPlatform(isAllocatedByPlatform)
    , m_isResizable(true)
{
    ASSERT(isAllocatedByPlatform);
}

int NonSharedBackingStore::clearNonSharedBackingStore(void* obj)
{
#if !defined(NDEBUG)
    obj = GC_USR_PTR_FROM_BASE(obj);
#endif
    NonSharedBackingStore* self = (NonSharedBackingStore*)obj;
    // check vptr to see if this is still a valid NonSharedBackingStore
    if (*(void**)self == nullptr) {
        // already freed
        return 0;
    }
    if (!self->m_isResizable) {
        self->m_deleter(self->m_data, self->m_byteLength, self->m_deleterData);
    } else {
        self->m_deleter(self->m_data, self->m_maxByteLength, nullptr);
    }
    // zero the vptr to mark as cleaned
    *(void**)self = nullptr;
    return 0;
}

void* NonSharedBackingStore::operator new(size_t size)
{
    return CustomAllocator<NonSharedBackingStore>().allocate(1);
}

void NonSharedBackingStore::resize(size_t newByteLength)
{
    ASSERT(m_isResizable && newByteLength <= m_maxByteLength);

    if (m_byteLength < newByteLength) {
        memset(static_cast<uint8_t*>(m_data) + m_byteLength, 0, newByteLength - m_byteLength);
    }

    m_byteLength = newByteLength;
    bufferUpdated(m_data, m_byteLength);
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
    bufferUpdated(m_data, newByteLength);
}

#if defined(ENABLE_THREADING)
BackingStore* BackingStore::createDefaultSharedBackingStore(size_t byteLength)
{
    SharedDataBlockInfo* sharedInfo = new SharedDataBlockInfo(
        Global::platform()->onMallocArrayBufferObjectDataBuffer(byteLength),
        byteLength,
        backingStorePlatformDeleter);
    return new SharedBackingStore(sharedInfo);
}

BackingStore* BackingStore::createDefaultGrowableSharedBackingStore(size_t byteLength, size_t maxByteLength)
{
    SharedDataBlockInfo* sharedInfo = new GrowableSharedDataBlockInfo(
        Global::platform()->onMallocArrayBufferObjectDataBuffer(maxByteLength),
        byteLength, maxByteLength,
        backingStorePlatformDeleter);
    return new SharedBackingStore(sharedInfo);
}

BackingStore* BackingStore::createSharedBackingStore(SharedDataBlockInfo* sharedInfo)
{
    return new SharedBackingStore(sharedInfo);
}

void SharedDataBlockInfo::deref()
{
    ASSERT(hasValidReference());

    auto oldValue = m_refCount.fetch_sub(1);
    if (oldValue == 1) {
        if (isGrowable()) {
            m_deleter(m_data, maxByteLength(), nullptr);
        } else {
            m_deleter(m_data, m_byteLength, nullptr);
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
}

int SharedBackingStore::clearSharedBackingStore(void* obj)
{
#if !defined(NDEBUG)
    obj = GC_USR_PTR_FROM_BASE(obj);
#endif
    SharedBackingStore* self = (SharedBackingStore*)obj;
    // check vptr to see if this is still a valid SharedBackingStore
    if (*(void**)self == nullptr) {
        // already freed
        return 0;
    }
    self->sharedDataBlockInfo()->deref();
    // zero the vptr to mark as cleaned
    *(void**)self = nullptr;
    return 0;
}

void SharedBackingStore::resize(size_t newByteLength)
{
    ASSERT(m_sharedDataBlockInfo->hasValidReference());
    ASSERT(isResizable() && newByteLength <= maxByteLength());

    m_sharedDataBlockInfo->grow(newByteLength);
    bufferUpdated(m_sharedDataBlockInfo->data(), m_sharedDataBlockInfo->byteLength());
}

void* SharedBackingStore::operator new(size_t size)
{
    return CustomAllocator<SharedBackingStore>().allocate(1);
}
#endif

} // namespace Escargot
