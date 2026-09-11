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

#include "runtime/Object.h"

namespace Escargot {

class SharedDataBlockInfo;

template <typename BufferOwnerType>
class BufferAddressObserverManager {
public:
    BufferAddressObserverManager()
#ifndef NDEBUG
        : m_isUpdating(false)
#endif
    {
    }

    using BufferAddressObserverCallback = void (*)(Object* from, void* newAddress, size_t newByteLength);
    void addObserver(Object* from, BufferAddressObserverCallback cb)
    {
        ASSERT(!m_isUpdating && !!from);

        for (size_t i = 0; i < m_observerItems.size(); i++) {
            if (m_observerItems[i]->from.unwrap() == nullptr) {
                m_observerItems[i]->from = from;
                m_observerItems[i]->callback = cb;
                GC_GENERAL_REGISTER_DISAPPEARING_LINK_SAFE(reinterpret_cast<void**>(&m_observerItems[i]->from), from);
                return;
            }
        }

        auto newData = new ObserverVectorItem(from, cb);
        m_observerItems.pushBack(newData);
        GC_GENERAL_REGISTER_DISAPPEARING_LINK_SAFE(reinterpret_cast<void**>(&newData->from), from);
    }

    void removeObserver(Object* from, BufferAddressObserverCallback callback)
    {
        ASSERT(!m_isUpdating);
        for (size_t i = 0; i < m_observerItems.size(); i++) {
            if (m_observerItems[i]->from.unwrap() == from && m_observerItems[i]->callback == callback) {
                GC_unregister_disappearing_link(reinterpret_cast<void**>(&m_observerItems[i]->from));
                m_observerItems[i]->from = nullptr;
                m_observerItems[i]->callback = nullptr;
                break;
            }
        }
    }

protected:
    struct ObserverVectorItem : public gc {
        Optional<Object*> from;
        BufferAddressObserverCallback callback;

        ObserverVectorItem(Object* from, BufferAddressObserverCallback callback)
            : from(from)
            , callback(callback)
        {
        }

        void* operator new(size_t size)
        {
            return GC_MALLOC_ATOMIC(size);
        }
    };

    TightVector<ObserverVectorItem*, GCUtil::gc_malloc_allocator<ObserverVectorItem*>> m_observerItems;

#ifndef NDEBUG
    bool m_isUpdating;
#endif
    void bufferUpdated(void* newAddress, size_t newByteLength)
    {
#ifndef NDEBUG
        m_isUpdating = true;
#endif
        for (size_t i = 0; i < m_observerItems.size(); i++) {
            if (LIKELY(!!m_observerItems[i]->from)) {
                m_observerItems[i]->callback(m_observerItems[i]->from.value(), newAddress, newByteLength);
            }
        }
#ifndef NDEBUG
        m_isUpdating = false;
#endif
    }
};

using BackingStoreDeleterCallback = void (*)(void* data, size_t length, void* deleterData);

class BackingStore : public gc, public BufferAddressObserverManager<BackingStore> {
    friend int getValidValueInNonSharedBackingStore(void* ptr, GC_mark_pair* arr);
#if defined(ENABLE_THREADING)
    friend int getValidValueInSharedBackingStore(void* ptr, GC_mark_pair* arr);
#endif

public:
    static BackingStore* createDefaultNonSharedBackingStore(size_t byteLength);
    static BackingStore* createDefaultResizableNonSharedBackingStore(size_t byteLength, size_t maxByteLength);
    static BackingStore* createNonSharedBackingStore(void* data, size_t byteLength, BackingStoreDeleterCallback deleter, void* callbackData);

#if defined(ENABLE_THREADING)
    static BackingStore* createDefaultSharedBackingStore(size_t byteLength);
    static BackingStore* createDefaultGrowableSharedBackingStore(size_t byteLength, size_t maxByteLength);
    // SharedBackingStore is newly created by sharing with already created one
    static BackingStore* createSharedBackingStore(SharedDataBlockInfo* sharedInfo);
#endif

    // indicates whether the backing store was created for SharedArrayBuffer (SharedDataBlock)
    virtual bool isShared() const = 0;
    virtual void* data() const = 0;
    virtual size_t byteLength() const = 0;
    virtual size_t maxByteLength() const = 0;
    virtual void* deleterData() const = 0;
    virtual bool isResizable() const = 0;

    virtual size_t byteLengthRMW(size_t newByteLength) // special function used only for WASMMemoryObject
    {
        ASSERT_NOT_REACHED();
        return 0;
    }

    virtual SharedDataBlockInfo* sharedDataBlockInfo() const
    {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    virtual void resize(size_t newByteLength)
    {
        ASSERT_NOT_REACHED();
    }

    virtual void reallocate(size_t newByteLength)
    {
        ASSERT_NOT_REACHED();
    }

    void* operator new(size_t size) = delete;
    void* operator new[](size_t size) = delete;

protected:
    BackingStore()
        : BufferAddressObserverManager<BackingStore>()
    {
    }
};

class NonSharedBackingStore : public BackingStore {
    friend class BackingStore;
    friend int getValidValueInNonSharedBackingStore(void* ptr, GC_mark_pair* arr);

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

    virtual size_t maxByteLength() const override
    {
        ASSERT(m_isResizable);
        return m_maxByteLength;
    }

    virtual void* deleterData() const override
    {
        return m_deleterData;
    }

    virtual bool isResizable() const override
    {
        return m_isResizable;
    }

    virtual void resize(size_t newByteLength) override;
    virtual void reallocate(size_t newByteLength) override;

    static int clearNonSharedBackingStore(void* obj);

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

private:
    NonSharedBackingStore(void* data, size_t byteLength, BackingStoreDeleterCallback deleter, void* callbackData, bool isAllocatedByPlatform);
    NonSharedBackingStore(void* data, size_t byteLength, BackingStoreDeleterCallback deleter, size_t maxByteLength, bool isAllocatedByPlatform);

    void* m_data;
    size_t m_byteLength;
    BackingStoreDeleterCallback m_deleter;
    union {
        void* m_deleterData;
        size_t m_maxByteLength;
    };
    // bitfields so the alive mark below packs into the same byte instead of costing this
    // (very numerous) object a whole extra one
    bool m_isAllocatedByPlatform : 1;
    bool m_isResizable : 1;
    // alive mark for clearNonSharedBackingStore. Must not reuse the vptr
    // (word 0): GC_disclaim_and_reclaim (GCutil reclaim.c) writes the
    // free-list link into word 0 the moment this disclaim proc first returns
    // 0, and keeps re-invoking the proc on that same still-unreclaimed slot
    // on every later sweep; GC_clear_block() zeroes everything *except* word
    // 0, so a later revisit would read that stale link instead of a real
    // vtable/zero and wrongly treat the slot as still alive.
    bool m_gcDisclaimAlive : 1;
};

#if defined(ENABLE_THREADING)
class SharedDataBlockInfo {
public:
    SharedDataBlockInfo(void* data, size_t byteLength, BackingStoreDeleterCallback deleter)
        : m_data(data)
        , m_byteLength(byteLength)
        , m_refCount(0)
        , m_deleter(deleter)
    {
        ASSERT(!!deleter);
    }

    virtual ~SharedDataBlockInfo() {}

    virtual bool isGrowable() const
    {
        return false;
    }

    virtual size_t maxByteLength() const
    {
        ASSERT_NOT_REACHED();
        return 0;
    }

    virtual void grow(size_t newByteLength)
    {
        UNUSED_PARAMETER(newByteLength);
        ASSERT_NOT_REACHED();
    }

    void* data() const
    {
        ASSERT(hasValidReference());
        return m_data;
    }

    size_t byteLength() const
    {
        ASSERT(hasValidReference());
        return m_byteLength.load();
    }

    size_t byteLengthRMW(size_t newByteLength)
    {
        ASSERT(hasValidReference());
        return m_byteLength.exchange(newByteLength);
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

protected:
    void* m_data;
    // defined as atomic value to not to use a lock
    std::atomic<size_t> m_byteLength;
    std::atomic<size_t> m_refCount;
    BackingStoreDeleterCallback m_deleter;
};

class GrowableSharedDataBlockInfo : public SharedDataBlockInfo {
public:
    GrowableSharedDataBlockInfo(void* data, size_t byteLength, size_t maxByteLength, BackingStoreDeleterCallback deleter)
        : SharedDataBlockInfo(data, byteLength, deleter)
        , m_maxByteLength(maxByteLength)
    {
    }

    virtual bool isGrowable() const override
    {
        return true;
    }

    virtual size_t maxByteLength() const override
    {
        return m_maxByteLength;
    }

    virtual void grow(size_t newByteLength) override
    {
        ASSERT(newByteLength <= m_maxByteLength);
        m_byteLength.store(newByteLength);
    }

private:
    // defined once and never change
    const size_t m_maxByteLength;
};

class SharedBackingStore : public BackingStore {
    friend class BackingStore;
    friend int getValidValueInSharedBackingStore(void* ptr, GC_mark_pair* arr);

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

    virtual size_t maxByteLength() const override
    {
        ASSERT(!!m_sharedDataBlockInfo && m_sharedDataBlockInfo->hasValidReference());
        return m_sharedDataBlockInfo->maxByteLength();
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

    virtual bool isResizable() const override
    {
        return m_sharedDataBlockInfo->isGrowable();
    }

    virtual size_t byteLengthRMW(size_t newByteLength) override
    {
        return m_sharedDataBlockInfo->byteLengthRMW(newByteLength);
    }

    virtual void resize(size_t newByteLength) override;

    static int clearSharedBackingStore(void* obj);

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

private:
    SharedBackingStore(SharedDataBlockInfo* sharedInfo);

    // Also doubles as the alive mark for clearSharedBackingStore: nulled out
    // once the slot is disposed. Unlike the vptr (word 0), GC_clear_block()
    // reliably zeroes this on every sweep pass, including revisits of an
    // already-reclaimed slot -- see the comment on
    // NonSharedBackingStore::m_gcDisclaimAlive for why word 0 can't be used
    // for this.
    SharedDataBlockInfo* m_sharedDataBlockInfo;
};
#endif

} // namespace Escargot

#endif
