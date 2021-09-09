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

#ifndef __EscargotBackingStore__
#define __EscargotBackingStore__

namespace Escargot {

class SharedDataBlockInfo;

using BackingStoreDeleterCallback = void (*)(void* data, size_t length, void* deleterData);

class BackingStore : public gc {
public:
    static BackingStore* createDefaultNonSharedBackingStore(size_t byteLength);
    static BackingStore* createNonSharedBackingStore(void* data, size_t byteLength, BackingStoreDeleterCallback callback, void* callbackData);

#if defined(ENABLE_THREADING)
    static BackingStore* createDefaultSharedBackingStore(size_t byteLength);
    // SharedBackingStore is newly created by sharing with already created one
    static BackingStore* createSharedBackingStore(SharedDataBlockInfo* sharedInfo);
#endif

    // indicates whether the backing store was created for SharedArrayBuffer (SharedDataBlock)
    virtual bool isShared() const = 0;
    virtual void* data() const = 0;
    virtual size_t byteLength() const = 0;
    virtual void* deleterData() const = 0;
    virtual SharedDataBlockInfo* sharedDataBlockInfo() const = 0;
    virtual void reallocate(size_t newByteLength) = 0;

    void* operator new(size_t size) = delete;
    void* operator new[](size_t size) = delete;
};

class NonSharedBackingStore : public BackingStore {
    friend class BackingStore;

public:
    virtual bool isShared() const override
    {
        return false;
    }

    virtual void* data() const override
    {
        return m_data;
    }

    virtual size_t byteLength() const override
    {
        return m_byteLength;
    }

    virtual void* deleterData() const override
    {
        return m_deleterData;
    }

    virtual SharedDataBlockInfo* sharedDataBlockInfo() const override
    {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    virtual void reallocate(size_t newByteLength) override;

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

private:
    NonSharedBackingStore(void* data, size_t byteLength, BackingStoreDeleterCallback callback, void* callbackData, bool isAllocatedByPlatform);

    void* m_data;
    size_t m_byteLength;
    BackingStoreDeleterCallback m_deleter;
    void* m_deleterData;
    bool m_isAllocatedByPlatform;
};

#if defined(ENABLE_THREADING)
class SharedDataBlockInfo {
public:
    SharedDataBlockInfo(void* data, size_t byteLength)
        : m_data(data)
        , m_byteLength(byteLength)
        , m_refCount(0)
    {
    }

    void* data() const
    {
        ASSERT(hasValidReference());
        return m_data;
    }

    size_t byteLength() const
    {
        ASSERT(hasValidReference());
        return m_byteLength;
    }

    void ref()
    {
        m_refCount++;
    }

    void deref();

    bool hasValidReference() const
    {
        return (m_refCount.load() > 0);
    }

private:
    void* m_data;
    size_t m_byteLength;
    std::atomic<size_t> m_refCount;
};

class SharedBackingStore : public BackingStore {
    friend class BackingStore;

public:
    virtual bool isShared() const override
    {
        return true;
    }

    virtual void* data() const override
    {
        ASSERT(!!m_sharedDataBlockInfo);
        return m_sharedDataBlockInfo->data();
    }

    virtual size_t byteLength() const override
    {
        ASSERT(!!m_sharedDataBlockInfo);
        return m_sharedDataBlockInfo->byteLength();
    }

    virtual void* deleterData() const override
    {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    virtual SharedDataBlockInfo* sharedDataBlockInfo() const override
    {
        ASSERT(!!m_sharedDataBlockInfo && m_sharedDataBlockInfo->hasValidReference());
        return m_sharedDataBlockInfo;
    }

    virtual void reallocate(size_t newByteLength) override
    {
        ASSERT_NOT_REACHED();
    }

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

private:
    SharedBackingStore(SharedDataBlockInfo* sharedInfo);

    SharedDataBlockInfo* m_sharedDataBlockInfo;
};
#endif

} // namespace Escargot

#endif
