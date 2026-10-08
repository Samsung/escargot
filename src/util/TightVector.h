/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
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

#ifndef __EscargotTightVector__
#define __EscargotTightVector__

#include "runtime/CompressibleHeapPointer.h"

namespace Escargot {

template <typename T, typename Allocator, typename SizeType = size_t>
class TightVector : public gc {
public:
    TightVector()
    {
        m_buffer = nullptr;
        m_size = 0;
    }

    explicit TightVector(size_t siz)
    {
        m_buffer = nullptr;
        m_size = 0;
        resize(siz);
    }

    TightVector(TightVector<T, Allocator, SizeType>&& other)
    {
        m_size = other.size();
        m_buffer = other.m_buffer;
        other.m_buffer = nullptr;
        other.m_size = 0;
    }

    TightVector(const TightVector<T, Allocator, SizeType>& other)
    {
        if (other.size()) {
            m_size = other.size();
            m_buffer = Allocator().allocate(m_size);
            VectorCopier<T>::copy(m_buffer, other.data(), m_size);
        } else {
            m_buffer = nullptr;
            m_size = 0;
        }
    }

    const TightVector<T, Allocator, SizeType>& operator=(const TightVector<T, Allocator, SizeType>& other)
    {
        if (&other == this)
            return *this;

        if (other.size()) {
            m_size = other.size();
            m_buffer = Allocator().allocate(m_size);
            VectorCopier<T>::copy(m_buffer, other.data(), m_size);
        } else {
            clear();
        }
        return *this;
    }

    const TightVector<T, Allocator, SizeType>& operator=(TightVector<T, Allocator, SizeType>&& other)
    {
        if (&other == this)
            return *this;

        if (other.size()) {
            m_size = other.size();
            m_buffer = other.m_buffer;
            other.m_buffer = nullptr;
            other.m_size = 0;
        } else {
            clear();
        }
        return *this;
    }

    TightVector(const TightVector<T, Allocator, SizeType>& other, const T& newItem)
    {
        m_size = other.size() + 1;
        m_buffer = Allocator().allocate(m_size);
        VectorCopier<T>::copy(m_buffer, other.data(), other.size());

        m_buffer[other.size()] = newItem;
    }

    TightVector(const T* start, const T* end)
    {
        m_buffer = nullptr;
        m_size = std::distance(start, end);

        if (m_size) {
            T* newBuffer = Allocator().allocate(m_size);
            m_buffer = newBuffer;

            VectorCopier<T>::copy(m_buffer, start, m_size);
        }
    }

    ~TightVector()
    {
        if (m_buffer)
            Allocator().deallocate(m_buffer, m_size);
    }

    void pushBack(const T& val)
    {
        if (std::is_fundamental<T>::value && m_buffer) {
            m_buffer = reinterpret_cast<T*>(GC_REALLOC_NO_SHRINK(m_buffer, (m_size + 1) * sizeof(T)));
        } else {
            T* newBuffer = Allocator().allocate(m_size + 1);
            VectorCopier<T>::copy(newBuffer, m_buffer, m_size);
            if (m_buffer) {
                Allocator().deallocate(m_buffer, m_size);
            }
            m_buffer = newBuffer;
        }

        m_buffer[m_size] = val;
        m_size++;
    }

    void push_back(const T& val)
    {
        pushBack(val);
    }

    void insert(size_t pos, const T& val)
    {
        ASSERT(pos <= m_size);
        T* newBuffer = Allocator().allocate(m_size + 1);

        VectorCopier<T>::copy(newBuffer, m_buffer, pos);
        newBuffer[pos] = val;
        VectorCopier<T>::copy(&newBuffer[pos + 1], m_buffer + pos, m_size - pos);

        if (m_buffer)
            Allocator().deallocate(m_buffer, m_size);
        m_buffer = newBuffer;
        m_size++;
    }

    void erase(size_t pos)
    {
        erase(pos, pos + 1);
    }

    void erase(size_t start, size_t end)
    {
        ASSERT(start < end);
        ASSERT(start >= 0);
        ASSERT(end <= m_size);

        size_t c = end - start;
        if (m_size - c) {
            T* newBuffer = Allocator().allocate(m_size - c);
            VectorCopier<T>::copy(newBuffer, m_buffer, start);
            if (m_size > end) {
                VectorCopier<T>::copy(&newBuffer[start], &m_buffer[end], m_size - end);
            }

            if (m_buffer)
                Allocator().deallocate(m_buffer, m_size);
            m_buffer = newBuffer;
            m_size = m_size - c;
        } else {
            if (m_buffer)
                Allocator().deallocate(m_buffer, m_size);
            m_size = 0;
            m_buffer = nullptr;
        }
    }

    size_t size() const
    {
        return m_size;
    }

    size_t capacity() const
    {
        return m_size;
    }

    bool empty() const
    {
        return m_size == 0;
    }

    void pop_back()
    {
        erase(m_size - 1);
    }

    T& operator[](const size_t idx)
    {
        ASSERT(idx < m_size);
        return m_buffer[idx];
    }

    const T& operator[](const size_t idx) const
    {
        ASSERT(idx < m_size);
        return m_buffer[idx];
    }

    T* data()
    {
        return m_buffer;
    }

    const T* data() const
    {
        return m_buffer;
    }

    T& back()
    {
        return m_buffer[m_size - 1];
    }

    void clear()
    {
        if (m_buffer)
            Allocator().deallocate(m_buffer, m_size);
        m_size = 0;
        m_buffer = nullptr;
    }

    void resizeWithUninitializedValues(size_t newSize)
    {
        if (newSize) {
            T* newBuffer = Allocator().allocate(newSize);
            VectorCopier<T>::copy(newBuffer, m_buffer, std::min(m_size, newSize));

            if (m_buffer)
                Allocator().deallocate(m_buffer, m_size);
            m_size = newSize;
            m_buffer = newBuffer;
        } else {
            if (m_buffer)
                Allocator().deallocate(m_buffer, m_size);
            m_size = newSize;
            m_buffer = nullptr;
        }
    }

    void resize(size_t newSize, const T& val = T())
    {
        if (newSize) {
            T* newBuffer = Allocator().allocate(newSize);
            VectorCopier<T>::copy(newBuffer, m_buffer, std::min(m_size, newSize));

            for (size_t i = m_size; i < newSize; i++) {
                newBuffer[i] = val;
            }

            if (m_buffer)
                Allocator().deallocate(m_buffer, m_size);

            m_size = newSize;
            m_buffer = newBuffer;
        } else {
            if (m_buffer)
                Allocator().deallocate(m_buffer, m_size);

            m_size = newSize;
            m_buffer = nullptr;
        }
    }

    void assign(const T* start, const T* end)
    {
        clear();

        m_size = std::distance(start, end);

        if (m_size) {
            T* newBuffer = Allocator().allocate(m_size);
            m_buffer = newBuffer;

            VectorCopier<T>::copy(m_buffer, start, m_size);
        }
    }

    // used for specific case
    void reset(T* newData, size_t newSize)
    {
        m_buffer = newData;
        m_size = newSize;
    }

    using iterator = T*;
    constexpr iterator begin() const { return m_buffer; }
    constexpr iterator end() const { return m_buffer + m_size; }

    void* operator new(size_t size)
    {
        static MAY_THREAD_LOCAL bool typeInited = false;
        static MAY_THREAD_LOCAL GC_descr descr;
        if (!typeInited) {
            GC_word desc[GC_BITMAP_SIZE(TightVector)] = { 0 };
            GC_set_bit(desc, GC_WORD_OFFSET(TightVector, m_buffer));
            descr = GC_make_descriptor(desc, GC_WORD_LEN(TightVector));
            typeInited = true;
        }
        return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
    }
    void* operator new[](size_t size) = delete;

protected:
    T* m_buffer;
    SizeType m_size;
};

template <typename T, typename Allocator>
class TightVectorWithNoSize : public gc {
public:
    TightVectorWithNoSize()
    {
        m_buffer = nullptr;
    }

    TightVectorWithNoSize(TightVectorWithNoSize<T, Allocator>&& other)
    {
        m_buffer = other.m_buffer;
        other.m_buffer = nullptr;
    }

    TightVectorWithNoSize(const TightVectorWithNoSize<T, Allocator>& other) = delete;

    const TightVectorWithNoSize<T, Allocator>& operator=(const TightVectorWithNoSize<T, Allocator>& other) = delete;
    void operator=(TightVectorWithNoSize<T, Allocator>&& other)
    {
        if (m_buffer) {
            Allocator().deallocate(m_buffer);
        }
        m_buffer = other.m_buffer;
        other.m_buffer = nullptr;
    }

    ~TightVectorWithNoSize()
    {
        if (m_buffer) {
            Allocator().deallocate(m_buffer);
        }
    }

    void pushBack(const T& val, size_t newSize)
    {
        if (std::is_fundamental<T>::value && m_buffer) {
            m_buffer = reinterpret_cast<T*>(GC_REALLOC_NO_SHRINK(m_buffer, newSize * sizeof(T)));
        } else {
            T* newBuffer = Allocator().allocate(newSize);
            VectorCopier<T>::copy(newBuffer, m_buffer, newSize - 1);
            if (m_buffer) {
                Allocator().deallocate(m_buffer, newSize - 1);
            }
            m_buffer = newBuffer;
        }

        m_buffer[newSize - 1] = val;
    }

    void push_back(const T& val, size_t newSize)
    {
        pushBack(val, newSize);
    }

    T& operator[](const size_t idx)
    {
        return m_buffer[idx];
    }

    const T& operator[](const size_t idx) const
    {
        return m_buffer[idx];
    }

    void resizeWithUninitializedValues(size_t oldSize, size_t newSize)
    {
        if (newSize) {
            // Allocate the first buffer through Allocator so custom GC kinds
            // are retained. Realloc preserves that kind when the buffer moves.
            if (std::is_fundamental<T>::value && m_buffer) {
                m_buffer = reinterpret_cast<T*>(GC_REALLOC(m_buffer, newSize * sizeof(T)));
                return;
            }
            T* newBuffer = Allocator().allocate(newSize);
            VectorCopier<T>::copy(newBuffer, m_buffer, std::min(oldSize, newSize));

            if (m_buffer)
                Allocator().deallocate(m_buffer, oldSize);
            m_buffer = newBuffer;
        } else {
            if (m_buffer)
                Allocator().deallocate(m_buffer, oldSize);
            m_buffer = nullptr;
        }
    }

    void erase(size_t pos, size_t currentSize)
    {
        erase(pos, pos + 1, currentSize);
    }

    void erase(size_t start, size_t end, size_t currentSize)
    {
        ASSERT(start < end);
        ASSERT(start >= 0);
        ASSERT(end <= currentSize);

        size_t c = end - start;
        if (currentSize - c) {
            T* newBuffer = Allocator().allocate(currentSize - c);
            VectorCopier<T>::copy(newBuffer, m_buffer, start);
            VectorCopier<T>::copy(&newBuffer[end - c], &m_buffer[end], currentSize - end);

            if (m_buffer)
                Allocator().deallocate(m_buffer, currentSize);
            m_buffer = newBuffer;
        } else {
            if (m_buffer)
                Allocator().deallocate(m_buffer, currentSize);
            m_buffer = nullptr;
        }
    }

    T* data()
    {
        return m_buffer;
    }

    const T* data() const
    {
        return m_buffer;
    }

    // used for specific case
    // should not have any valid data
    void reset(T* resetData)
    {
        ASSERT(!m_buffer);
        m_buffer = resetData;
    }

    // used for specific case
    void reset()
    {
        m_buffer = nullptr;
    }

protected:
    T* m_buffer;
};

template <typename T, typename GCAllocator>
class TightVectorWithNoSizeUseGCRealloc : public gc {
public:
    TightVectorWithNoSizeUseGCRealloc()
    {
        m_buffer = nullptr;
    }

    TightVectorWithNoSizeUseGCRealloc(TightVectorWithNoSizeUseGCRealloc<T, GCAllocator>&& other)
    {
        m_buffer = other.m_buffer;
        other.m_buffer = nullptr;
    }

    TightVectorWithNoSizeUseGCRealloc(const TightVectorWithNoSizeUseGCRealloc<T, GCAllocator>& other) = delete;

    const TightVectorWithNoSizeUseGCRealloc<T, GCAllocator>& operator=(const TightVectorWithNoSizeUseGCRealloc<T, GCAllocator>& other) = delete;
    void operator=(TightVectorWithNoSizeUseGCRealloc<T, GCAllocator>&& other)
    {
        if (m_buffer) {
            GC_FREE(m_buffer);
        }
        m_buffer = other.m_buffer;
        other.m_buffer = nullptr;
    }

    ~TightVectorWithNoSizeUseGCRealloc()
    {
        if (m_buffer) {
            GC_FREE(m_buffer);
        }
    }

    void clear()
    {
        if (m_buffer) {
            GC_FREE(m_buffer);
        }
        m_buffer = nullptr;
    }

    void pushBack(const T& val, size_t newSize)
    {
        auto oldSize = newSize - 1;
        expandBuffer(newSize);
        m_buffer[oldSize] = val;
    }

    void push_back(const T& val, size_t newSize)
    {
        pushBack(val, newSize);
    }

    void insert(size_t pos, const T& val, size_t currentSize)
    {
        ASSERT(pos <= currentSize);
        expandBuffer(currentSize + 1);
        memmove(m_buffer + pos + 1, m_buffer + pos, sizeof(T) * (currentSize - pos));
        m_buffer[pos] = val;
    }

    T& operator[](const size_t idx)
    {
        return m_buffer[idx];
    }

    const T& operator[](const size_t idx) const
    {
        return m_buffer[idx];
    }

    void resize(size_t oldSize, size_t newSize, const T& val = T())
    {
        if (newSize) {
            expandBuffer(newSize);
            for (size_t i = oldSize; i < newSize; i++) {
                m_buffer[i] = val;
            }
        } else {
            clear();
        }
    }

    void resizeWithUninitializedValues(size_t oldSize, size_t newSize)
    {
        if (newSize) {
            expandBuffer(newSize);
        } else {
            GC_FREE(m_buffer);
            m_buffer = nullptr;
        }
    }

    void erase(size_t pos, size_t currentSize)
    {
        erase(pos, pos + 1, currentSize);
    }

    void erase(size_t start, size_t end, size_t currentSize)
    {
        ASSERT(start < end);
        ASSERT(start >= 0);
        ASSERT(end <= currentSize);

        size_t c = end - start;
        if (currentSize - c) {
            T* newBuffer = GCAllocator().allocate((currentSize - c) * sizeof(T));
            VectorCopier<T>::copy(newBuffer, m_buffer, start);
            VectorCopier<T>::copy(&newBuffer[end - c], &m_buffer[end], currentSize - end);

            if (m_buffer) {
                GC_FREE(m_buffer);
            }
            m_buffer = newBuffer;
        } else {
            if (m_buffer) {
                GC_FREE(m_buffer);
            }
            m_buffer = nullptr;
        }
    }

    T* data()
    {
        return m_buffer;
    }

    // used for specific case
    // should not have any valid data
    void reset(T* resetData)
    {
        ASSERT(!m_buffer);
        m_buffer = resetData;
    }

    void expandBuffer(size_t newSize)
    {
        if (m_buffer == nullptr) {
            if (newSize) {
                m_buffer = GCAllocator().allocate(newSize);
            }
        } else {
            m_buffer = reinterpret_cast<T*>(GC_REALLOC_NO_SHRINK(m_buffer, newSize * sizeof(T)));
        }
    }

    T* takeBuffer()
    {
        T* buffer = m_buffer;
        m_buffer = nullptr;
        return buffer;
    }

protected:
    T* m_buffer;
};

// Heap-member vectors share their operations in both pointer modes. Compressed
// buffers use the member's address to restore the cage's upper address bits.
template <typename T, typename Allocator>
class CompressibleHeapTightVector {
public:
    CompressibleHeapTightVector()
        : m_buffer(nullptr)
        , m_size(0)
    {
    }

    CompressibleHeapTightVector(const CompressibleHeapTightVector&) = delete;
    CompressibleHeapTightVector& operator=(const CompressibleHeapTightVector&) = delete;

    ~CompressibleHeapTightVector()
    {
        if (m_buffer) {
            Allocator().deallocate(m_buffer.raw(), m_size);
        }
    }

    T* data() { return m_buffer.raw(); }
    const T* data() const { return m_buffer.raw(); }
    size_t size() const { return m_size; }
    T& operator[](size_t index) { return m_buffer.value()[index]; }
    const T& operator[](size_t index) const { return m_buffer.value()[index]; }
    T* begin() { return m_buffer.raw(); }
    T* end() { return m_size ? m_buffer.value() + m_size : m_buffer.raw(); }
    const T* begin() const { return m_buffer.raw(); }
    const T* end() const { return m_size ? m_buffer.value() + m_size : m_buffer.raw(); }

    void pushBack(const T& value)
    {
        RELEASE_ASSERT(m_size < std::numeric_limits<typename CompressibleHeapPointer<T>::StorageType>::max());
        T* buffer = m_buffer
            ? static_cast<T*>(GC_REALLOC_NO_SHRINK(m_buffer.raw(), (size() + 1) * sizeof(T)))
            : Allocator().allocate(size() + 1);
        ASSERT(buffer);
        m_buffer = buffer;
        m_buffer.value()[m_size++] = value;
    }

    void resizeWithUninitializedValues(size_t newSize)
    {
        RELEASE_ASSERT(newSize <= std::numeric_limits<typename CompressibleHeapPointer<T>::StorageType>::max());
        Optional<T*> buffer = newSize ? Allocator().allocate(newSize) : nullptr;
        if (buffer && m_buffer) {
            VectorCopier<T>::copy(buffer.value(), m_buffer.value(), std::min(size(), newSize));
        }
        if (m_buffer) {
            Allocator().deallocate(m_buffer.value(), size());
        }
        m_buffer = buffer;
        m_size = newSize;
    }

    void resize(size_t newSize, const T& value = T())
    {
        size_t oldSize = size();
        resizeWithUninitializedValues(newSize);
        for (size_t i = oldSize; i < newSize; i++) {
            m_buffer.value()[i] = value;
        }
    }

    void erase(size_t index)
    {
        ASSERT(index < size());
        Optional<T*> buffer = m_size > 1 ? Allocator().allocate(m_size - 1) : nullptr;
        if (buffer) {
            VectorCopier<T>::copy(buffer.value(), m_buffer.value(), index);
            VectorCopier<T>::copy(buffer.value() + index, m_buffer.value() + index + 1, m_size - index - 1);
        }
        Allocator().deallocate(m_buffer.value(), size());
        m_buffer = buffer;
        m_size--;
    }

private:
    CompressibleHeapPointer<T> m_buffer;
    typename CompressibleHeapPointer<T>::StorageType m_size;
};

template <typename T, typename Allocator>
class CompressibleHeapTightVectorWithNoSize {
public:
    CompressibleHeapTightVectorWithNoSize()
        : m_buffer(nullptr)
    {
    }

    CompressibleHeapTightVectorWithNoSize(const CompressibleHeapTightVectorWithNoSize&) = delete;
    CompressibleHeapTightVectorWithNoSize& operator=(const CompressibleHeapTightVectorWithNoSize&) = delete;

    ~CompressibleHeapTightVectorWithNoSize()
    {
        if (m_buffer) {
            Allocator().deallocate(m_buffer.raw());
        }
    }

    T& operator[](size_t index) { return m_buffer.value()[index]; }
    const T& operator[](size_t index) const { return m_buffer.value()[index]; }

    void resizeWithUninitializedValues(size_t oldSize, size_t newSize)
    {
        T* newBuffer = newSize ? Allocator().allocate(newSize) : nullptr;
        if (newBuffer && m_buffer) {
            VectorCopier<T>::copy(newBuffer, m_buffer.raw(), std::min(oldSize, newSize));
        }
        if (m_buffer) {
            Allocator().deallocate(m_buffer.raw(), oldSize);
        }
        m_buffer = newBuffer;
    }

private:
    CompressibleHeapPointer<T> m_buffer;
};
#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
static_assert(sizeof(CompressibleHeapTightVectorWithNoSize<void*, GCUtil::gc_malloc_allocator<void*>>) == 4,
              "compressed no-size vector must be four bytes");
#endif

// Native builds embed the Vector; compressed builds store a four-byte
// reference to a separate Vector with its existing native GC descriptor.
// Both expose the same operations without branching at each use site.
template <typename VectorType, typename ItemType>
class CompressibleHeapVectorOwner {
public:
    CompressibleHeapVectorOwner()
#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
        : m_vector(new VectorType())
#endif
    {
    }

    CompressibleHeapVectorOwner(const CompressibleHeapVectorOwner&) = delete;
    CompressibleHeapVectorOwner& operator=(const CompressibleHeapVectorOwner&) = delete;

    CompressibleHeapVectorOwner& operator=(VectorType&& vector)
    {
        this->vector() = std::move(vector);
        return *this;
    }

    VectorType& vector()
    {
#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
        return *m_vector.value();
#else
        return m_vector;
#endif
    }
    const VectorType& vector() const
    {
#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
        return *m_vector.value();
#else
        return m_vector;
#endif
    }
    size_t size() const { return vector().size(); }
    ItemType& operator[](size_t index) { return vector()[index]; }
    const ItemType& operator[](size_t index) const { return vector()[index]; }
    ItemType* begin() const { return vector().begin(); }
    ItemType* end() const { return vector().end(); }
    void pushBack(const ItemType& item) { vector().pushBack(item); }
    void push_back(const ItemType& item) { vector().push_back(item); }
    void clear() { vector().clear(); }
    void resize(size_t size) { vector().resize(size); }
    void resizeFitWithUninitializedValues(size_t size) { vector().resizeFitWithUninitializedValues(size); }
    void erase(size_t index) { vector().erase(index); }

private:
#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
    CompressibleHeapPointer<VectorType> m_vector;
#else
    VectorType m_vector;
#endif
};

#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
static_assert(sizeof(CompressibleHeapVectorOwner<Vector<void*, GCUtil::gc_malloc_allocator<void*>>, void*>) == 4,
              "compressed vector owner must be four bytes");
static_assert(sizeof(CompressibleHeapTightVector<void*, GCUtil::gc_malloc_allocator<void*>>) == 8,
              "compressed heap vector should be eight bytes");
#endif
} // namespace Escargot

#endif
