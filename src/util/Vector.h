/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#ifndef __EscargotVector__
#define __EscargotVector__

namespace Escargot {

template <typename T, typename Allocator, int const glowFactor = 120>
class Vector : public gc {
public:
    Vector()
    {
        m_buffer = nullptr;
        m_size = 0;
        m_capacity = 0;
    }

    Vector(size_t siz)
    {
        m_buffer = nullptr;
        m_size = 0;
        m_capacity = 0;
        resize(siz);
    }

    Vector(Vector<T, Allocator, glowFactor>&& other)
    {
        m_size = other.size();
        m_buffer = other.m_buffer;
        m_capacity = other.m_capacity;
        other.m_buffer = nullptr;
        other.m_size = 0;
        other.m_capacity = 0;
    }

    Vector(const Vector<T, Allocator, glowFactor>& other)
    {
        if (other.size()) {
            m_size = other.size();
            m_capacity = other.m_capacity;
            m_buffer = Allocator().allocate(m_capacity);
            for (size_t i = 0; i < m_size; i++) {
                m_buffer[i] = other[i];
            }
        } else {
            m_buffer = nullptr;
            m_size = 0;
            m_capacity = 0;
        }
    }

    const Vector<T, Allocator, glowFactor>& operator=(const Vector<T, Allocator, glowFactor>& other)
    {
        if (other.size()) {
            m_size = other.size();
            m_capacity = other.m_capacity;
            m_buffer = Allocator().allocate(m_capacity);
            for (size_t i = 0; i < m_size; i++) {
                m_buffer[i] = other[i];
            }
        } else {
            clear();
        }
        return *this;
    }

    Vector(const Vector<T, Allocator, glowFactor>& other, const T& newItem)
    {
        m_size = other.size() + 1;
        m_capacity = other.m_capacity + 1;
        m_buffer = Allocator().allocate(m_size);
        for (size_t i = 0; i < other.size(); i++) {
            m_buffer[i] = other[i];
        }
        m_buffer[other.size()] = newItem;
    }

    ~Vector()
    {
        if (m_buffer)
            Allocator().deallocate(m_buffer, m_capacity);
    }

    void pushBack(const T& val)
    {
        if (m_capacity <= (m_size + 1)) {
            size_t oldc = m_capacity;
            m_capacity = computeAllocateSize(m_size + 1);
            T* newBuffer = Allocator().allocate(m_capacity);
            for (size_t i = 0; i < m_size; i++) {
                newBuffer[i] = m_buffer[i];
            }
            if (m_buffer)
                Allocator().deallocate(m_buffer, oldc);
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
        if (m_capacity <= (m_size + 1)) {
            size_t oldC = m_capacity;
            m_capacity = computeAllocateSize(m_size + 1);
            T* newBuffer = Allocator().allocate(m_capacity);
            for (size_t i = 0; i < pos; i++) {
                newBuffer[i] = m_buffer[i];
            }
            newBuffer[pos] = val;
            for (size_t i = pos; i < m_size; i++) {
                newBuffer[i + 1] = m_buffer[i];
            }
            if (m_buffer)
                Allocator().deallocate(m_buffer, oldC);
            m_buffer = newBuffer;
        } else {
            for (size_t i = pos; i < m_size; i++) {
                m_buffer[i + 1] = m_buffer[i];
            }
            m_buffer[pos] = val;
        }

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
            size_t oldC = m_capacity;
            T* newBuffer = Allocator().allocate(m_size - c);
            for (size_t i = 0; i < start; i++) {
                newBuffer[i] = m_buffer[i];
            }

            for (size_t i = end; i < m_size; i++) {
                newBuffer[i - c] = m_buffer[i];
            }

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

    size_t capacity() const
    {
        return m_capacity;
    }

    bool empty() const
    {
        return m_size == 0;
    }

    void pop_back()
    {
        erase(m_size - 1);
    }

    T& operator[](const size_t& idx)
    {
        ASSERT(idx < m_size);
        return m_buffer[idx];
    }

    const T& operator[](const size_t& idx) const
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

    void shrinkToFit()
    {
        if (m_size != m_capacity) {
            T* newBuffer = Allocator().allocate(m_size);
            for (size_t i = 0; i < m_size; i++) {
                newBuffer[i] = m_buffer[i];
            }
            size_t oldC = m_capacity;
            m_capacity = m_size;
            if (m_buffer)
                Allocator().deallocate(m_buffer, oldC);
            m_buffer = newBuffer;
        }
    }

    void resizeWithUninitializedValues(size_t newSize)
    {
        if (newSize) {
            T* newBuffer = Allocator().allocate(newSize);

            for (size_t i = 0; i < m_size && i < newSize; i++) {
                newBuffer[i] = m_buffer[i];
            }
            if (m_buffer)
                Allocator().deallocate(m_buffer, m_capacity);
            m_capacity = newSize;
            m_size = newSize;
            m_buffer = newBuffer;
        } else {
            clear();
        }
    }

    void resize(size_t newSize, const T& val = T())
    {
        if (newSize) {
            if (newSize > m_capacity) {
                size_t newCapacity = computeAllocateSize(newSize);
                T* newBuffer = Allocator().allocate(newCapacity);
                for (size_t i = 0; i < m_size && i < newSize; i++) {
                    newBuffer[i] = m_buffer[i];
                }
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

protected:
    size_t computeAllocateSize(size_t newSize)
    {
        size_t n = newSize * glowFactor / 100.f;
        if (n == newSize)
            n++;
        return n;
    }

    T* m_buffer;
    size_t m_capacity;
    size_t m_size;
};

template <typename T, typename Allocator, int const glowFactor = 120>
class VectorWithNoSize : public gc {
public:
    VectorWithNoSize()
    {
        m_buffer = nullptr;
        m_capacity = 0;
    }

    const VectorWithNoSize<T, Allocator, glowFactor>& operator=(const VectorWithNoSize<T, Allocator, glowFactor>& other) = delete;
    VectorWithNoSize(const VectorWithNoSize<T, Allocator, glowFactor>& other, const T& newItem) = delete;
    ~VectorWithNoSize()
    {
        if (m_buffer)
            Allocator().deallocate(m_buffer, m_capacity);
    }

    void pushBack(const T& val, size_t newSize)
    {
        if (m_capacity <= (newSize)) {
            size_t oldc = m_capacity;
            m_capacity = computeAllocateSize(newSize);
            T* newBuffer = Allocator().allocate(m_capacity);
            for (size_t i = 0; i < (newSize - 1); i++) {
                newBuffer[i] = m_buffer[i];
            }
            if (m_buffer)
                Allocator().deallocate(m_buffer, oldc);
            m_buffer = newBuffer;
        }
        m_buffer[(newSize - 1)] = val;
    }

    size_t capacity() const
    {
        return m_capacity;
    }

    T& operator[](const size_t& idx)
    {
        return m_buffer[idx];
    }

    const T& operator[](const size_t& idx) const
    {
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

    void clear()
    {
        if (m_buffer) {
            Allocator().deallocate(m_buffer, m_capacity);
        }
        m_buffer = nullptr;
        m_capacity = 0;
    }

    void resize(size_t oldSize, size_t newSize, const T& val = T())
    {
        if (newSize) {
            if (newSize > m_capacity) {
                size_t newCapacity = computeAllocateSize(newSize);
                T* newBuffer = Allocator().allocate(newCapacity);
                for (size_t i = 0; i < oldSize && i < newSize; i++) {
                    newBuffer[i] = m_buffer[i];
                }
                for (size_t i = oldSize; i < newSize; i++) {
                    newBuffer[i] = val;
                }
                if (m_buffer)
                    Allocator().deallocate(m_buffer, m_capacity);
                m_capacity = newCapacity;
                m_buffer = newBuffer;
            } else {
                for (size_t i = oldSize; i < newSize; i++) {
                    m_buffer[i] = val;
                }
            }
        } else {
            clear();
        }
    }

protected:
    size_t computeAllocateSize(size_t newSize)
    {
        size_t n = newSize * glowFactor / 100.f;
        if (n == newSize)
            n++;
        return n;
    }

    T* m_buffer;
    size_t m_capacity;
};


class VectorUtil {
public:
    static const size_t invalidIndex;

    template <typename T, typename Allocator>
    static size_t findInVector(const Vector<T, Allocator>& vector, const T& target)
    {
        size_t len = vector.size();
        for (size_t i = 0; i < len; i++) {
            if (vector[i] == target) {
                return i;
            }
        }

        return invalidIndex;
    }
};
}

#endif
