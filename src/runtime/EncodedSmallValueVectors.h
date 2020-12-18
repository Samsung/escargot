/*
 * Copyright (c) 2020-present Samsung Electronics Co., Ltd
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

#ifndef __EscargotEncodedSmallValueVectors__
#define __EscargotEncodedSmallValueVectors__

#ifndef __EscargotEncodedValue__
#error "don't include this file directly. include EncodedValue.h instead"
#endif

#if defined(ESCARGOT_64) && defined(ESCARGOT_USE_32BIT_IN_64BIT)

namespace Escargot {

template <typename Allocator, typename ComputeReservedCapacityFunction>
class Vector<EncodedSmallValue, Allocator, ComputeReservedCapacityFunction> : public gc {
    using T = EncodedSmallValue;

public:
    Vector()
    {
        m_buffer = nullptr;
        m_size = 0;
        m_capacity = 0;
    }

    explicit Vector(size_t siz)
    {
        m_buffer = nullptr;
        m_size = 0;
        m_capacity = 0;
        resize(siz);
    }

    Vector(Vector<T, Allocator, ComputeReservedCapacityFunction>&& other)
    {
        m_size = other.size();
        m_buffer = other.m_buffer;
        m_capacity = other.m_capacity;
        other.m_buffer = nullptr;
        other.m_size = 0;
        other.m_capacity = 0;
    }

    Vector(const Vector<T, Allocator, ComputeReservedCapacityFunction>& other)
    {
        if (other.size()) {
            m_size = other.size();
            m_capacity = other.m_capacity;
            m_buffer = Allocator().allocate(m_capacity);
            VectorCopier<T>::copy(m_buffer, other.data(), m_size);
        } else {
            m_buffer = nullptr;
            m_size = 0;
            m_capacity = 0;
        }
    }

    Vector(std::initializer_list<T> list)
        : Vector()
    {
        reserve(list.size());
        for (auto& i : list) {
            push_back(i);
        }
    }

    const Vector<T, Allocator, ComputeReservedCapacityFunction>& operator=(const Vector<T, Allocator, ComputeReservedCapacityFunction>& other)
    {
        if (&other == this)
            return *this;

        if (other.size()) {
            m_size = other.size();
            m_capacity = other.m_capacity;
            m_buffer = Allocator().allocate(m_capacity);
            VectorCopier<T>::copy(m_buffer, other.data(), m_size);
        } else {
            clear();
        }
        return *this;
    }

    const Vector<T, Allocator, ComputeReservedCapacityFunction>& operator=(Vector<T, Allocator, ComputeReservedCapacityFunction>&& other)
    {
        if (&other == this)
            return *this;

        if (other.size()) {
            m_size = other.size();
            m_buffer = other.m_buffer;
            m_capacity = other.m_capacity;
            other.m_buffer = nullptr;
            other.m_size = 0;
            other.m_capacity = 0;
        } else {
            clear();
        }
        return *this;
    }

    Vector(const Vector<T, Allocator, ComputeReservedCapacityFunction>& other, const T& newItem)
    {
        m_size = other.size() + 1;
        ComputeReservedCapacityFunction f;
        m_capacity = f(m_size);
        m_buffer = Allocator().allocate(m_capacity);
        VectorCopier<T>::copy(m_buffer, other.data(), other.size());

        m_buffer[other.size()] = newItem;
    }

    ~Vector()
    {
        if (m_buffer)
            Allocator().deallocate(m_buffer, m_capacity);
    }

    void insert(size_t pos, const Value& val)
    {
        ASSERT(pos <= m_size);
        if (m_capacity <= (m_size + 1)) {
            size_t oldC = m_capacity;

            ComputeReservedCapacityFunction f;
            m_capacity = f(m_size + 1);
            T* newBuffer = Allocator().allocate(m_capacity);

            VectorCopier<T>::copy(newBuffer, m_buffer, pos);
            newBuffer[pos] = val;
            VectorCopier<T>::copy(&newBuffer[pos + 1], m_buffer + pos, m_size - pos);

            if (m_buffer)
                Allocator().deallocate(m_buffer, oldC);
            m_buffer = newBuffer;
        } else {
            // TODO use memmove
            for (size_t i = m_size; i > pos; i--) {
                m_buffer[i] = m_buffer[i - 1];
            }

            m_buffer[pos] = val;
        }

        m_size++;
    }

    void pushBack(const Value& val)
    {
        if (m_capacity <= (m_size + 1)) {
            size_t oldc = m_capacity;
            ComputeReservedCapacityFunction f;
            m_capacity = f(m_size + 1);
            T* newBuffer = Allocator().allocate(m_capacity);
            VectorCopier<T>::copy(newBuffer, m_buffer, m_size);

            if (m_buffer)
                Allocator().deallocate(m_buffer, oldc);
            m_buffer = newBuffer;
        }
        m_buffer[m_size] = val;
        m_size++;
    }

    void push_back(const Value& val)
    {
        pushBack(val);
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
            size_t oldC = m_capacity;
            T* newBuffer = Allocator().allocate(m_size - c);
            VectorCopier<T>::copy(newBuffer, m_buffer, start);
            VectorCopier<T>::copy(&newBuffer[end - c], &m_buffer[end], m_size - end);

            if (m_buffer)
                Allocator().deallocate(m_buffer, oldC);
            m_buffer = newBuffer;
            m_size = m_size - c;
            m_capacity = m_size;
        } else {
            clear();
        }
    }

    size_t size() const
    {
        return m_size;
    }

    using iterator = T*;
    constexpr iterator begin() const { return m_buffer; }
    constexpr iterator end() const { return m_buffer + m_size; }
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

    T& at(const size_t& idx)
    {
        ASSERT(idx < m_size);
        return m_buffer[idx];
    }

    const T& at(const size_t& idx) const
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
        if (m_buffer) {
            Allocator().deallocate(m_buffer, m_capacity);
        }
        m_buffer = nullptr;
        m_size = 0;
        m_capacity = 0;
    }

    void reserve(size_t newCapacity)
    {
        if (m_capacity < newCapacity) {
            T* newBuffer = Allocator().allocate(newCapacity);
            VectorCopier<T>::copy(newBuffer, m_buffer, m_size);

            if (m_buffer) {
                Allocator().deallocate(m_buffer, m_capacity);
            }

            m_buffer = newBuffer;
            m_capacity = newCapacity;
        }
    }

    void resizeWithUninitializedValues(size_t newSize)
    {
        if (newSize) {
            if (m_capacity < newSize) {
                ComputeReservedCapacityFunction f;
                size_t newCapacity = f(newSize);
                T* newBuffer = Allocator().allocate(newCapacity);
                VectorCopier<T>::copy(newBuffer, m_buffer, std::min(m_size, newSize));

                if (m_buffer)
                    Allocator().deallocate(m_buffer, m_capacity);

                m_buffer = newBuffer;
                m_capacity = newCapacity;
            }
            m_size = newSize;
        } else {
            clear();
        }
    }

    void resize(size_t newSize, const Value& val = Value())
    {
        if (newSize) {
            if (newSize > m_capacity) {
                ComputeReservedCapacityFunction f;
                size_t newCapacity = f(newSize);
                T* newBuffer = Allocator().allocate(newCapacity);
                VectorCopier<T>::copy(newBuffer, m_buffer, std::min(m_size, newSize));

                for (size_t i = m_size; i < newSize; i++) {
                    newBuffer[i] = val;
                }
                if (m_buffer)
                    Allocator().deallocate(m_buffer, m_capacity);
                m_size = newSize;
                m_capacity = newCapacity;
                m_buffer = newBuffer;
            } else {
                for (size_t i = m_size; i < newSize; i++) {
                    m_buffer[i] = val;
                }
                m_size = newSize;
            }
        } else {
            clear();
        }
    }

    void* operator new(size_t size)
    {
        static bool typeInited = false;
        static GC_descr descr;
        if (!typeInited) {
            GC_word desc[GC_BITMAP_SIZE(Vector)] = { 0 };
            GC_set_bit(desc, GC_WORD_OFFSET(Vector, m_buffer));
            descr = GC_make_descriptor(desc, GC_WORD_LEN(Vector));
            typeInited = true;
        }
        return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
    }
    void* operator new[](size_t size) = delete;

protected:
    T* m_buffer;
    size_t m_capacity;
    size_t m_size;
};

template <typename Allocator>
class TightVector<EncodedSmallValue, Allocator> : public gc {
    using T = EncodedSmallValue;

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

    TightVector(TightVector<T, Allocator>&& other)
    {
        m_size = other.size();
        m_buffer = other.m_buffer;
        other.m_buffer = nullptr;
        other.m_size = 0;
    }

    TightVector(const TightVector<T, Allocator>& other)
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

    const TightVector<T, Allocator>& operator=(const TightVector<T, Allocator>& other)
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

    TightVector(const TightVector<T, Allocator>& other, const T& newItem)
    {
        m_size = other.size() + 1;
        m_buffer = Allocator().allocate(m_size);
        VectorCopier<T>::copy(m_buffer, other.data(), other.size());

        m_buffer[other.size()] = newItem;
    }

    ~TightVector()
    {
        if (m_buffer)
            Allocator().deallocate(m_buffer, m_size);
    }

    void pushBack(const Value& val)
    {
        T* newBuffer = Allocator().allocate(m_size + 1);
        VectorCopier<T>::copy(newBuffer, m_buffer, m_size);

        newBuffer[m_size] = val;
        if (m_buffer)
            Allocator().deallocate(m_buffer, m_size);
        m_buffer = newBuffer;
        m_size++;
    }

    void push_back(const Value& val)
    {
        pushBack(val);
    }

    void insert(size_t pos, const Value& val)
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
            VectorCopier<T>::copy(&newBuffer[end - c], &m_buffer[end], m_size - end);

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

    void resize(size_t newSize, const Value& val = Value())
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

protected:
    T* m_buffer;
    size_t m_size;
};

template <typename Allocator>
class TightVectorWithNoSize<EncodedSmallValue, Allocator> : public gc {
    using T = EncodedSmallValue;

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

    ~TightVectorWithNoSize()
    {
        if (m_buffer) {
            Allocator().deallocate(m_buffer);
        }
    }

    void pushBack(const Value& val, size_t newSize)
    {
        T* newBuffer = Allocator().allocate(newSize);
        VectorCopier<T>::copy(newBuffer, m_buffer, newSize - 1);

        newBuffer[newSize - 1] = val;
        if (m_buffer)
            Allocator().deallocate(m_buffer);
        m_buffer = newBuffer;
    }

    void push_back(const Value& val, size_t newSize)
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

protected:
    T* m_buffer;
};
} // namespace Escargot

#endif

#endif
