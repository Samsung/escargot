/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
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

            if (std::is_fundamental<T>()) {
                memcpy(m_buffer, other.data(), sizeof(T) * m_size);
            } else {
                for (size_t i = 0; i < m_size; i++) {
                    m_buffer[i] = other[i];
                }
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

            if (std::is_fundamental<T>()) {
                memcpy(m_buffer, other.data(), sizeof(T) * m_size);
            } else {
                for (size_t i = 0; i < m_size; i++) {
                    m_buffer[i] = other[i];
                }
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

        if (std::is_fundamental<T>()) {
            memcpy(m_buffer, other.data(), sizeof(T) * other.size());
        } else {
            for (size_t i = 0; i < other.size(); i++) {
                m_buffer[i] = other[i];
            }
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

            if (std::is_fundamental<T>()) {
                memcpy(newBuffer, m_buffer, sizeof(T) * m_size);
            } else {
                for (size_t i = 0; i < m_size; i++) {
                    newBuffer[i] = m_buffer[i];
                }
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

            if (std::is_fundamental<T>()) {
                memcpy(newBuffer, m_buffer, sizeof(T) * pos);
            } else {
                for (size_t i = 0; i < pos; i++) {
                    newBuffer[i] = m_buffer[i];
                }
            }

            newBuffer[pos] = val;

            if (std::is_fundamental<T>()) {
                memcpy(&newBuffer[pos + 1], &m_buffer[pos], sizeof(T) * (pos - m_size));
            } else {
                for (size_t i = pos; i < m_size; i++) {
                    newBuffer[i + 1] = m_buffer[i];
                }
            }

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

            if (std::is_fundamental<T>()) {
                memcpy(newBuffer, m_buffer, sizeof(T) * start);
                memcpy(&newBuffer[end - c], &m_buffer[end], sizeof(T) * (m_size - end));
            } else {
                for (size_t i = 0; i < start; i++) {
                    newBuffer[i] = m_buffer[i];
                }

                for (size_t i = end; i < m_size; i++) {
                    newBuffer[i - c] = m_buffer[i];
                }
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

            if (std::is_fundamental<T>()) {
                memcpy(newBuffer, m_buffer, sizeof(T) * m_size);
            } else {
                for (size_t i = 0; i < m_size; i++) {
                    newBuffer[i] = m_buffer[i];
                }
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
            if (m_capacity < newSize) {
                size_t newCapacity = computeAllocateSize(newSize);
                T* newBuffer = Allocator().allocate(newCapacity);

                if (std::is_fundamental<T>()) {
                    memcpy(newBuffer, m_buffer, sizeof(T) * std::min(m_size, newSize));
                } else {
                    for (size_t i = 0; i < m_size && i < newSize; i++) {
                        newBuffer[i] = m_buffer[i];
                    }
                }

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

    void resize(size_t newSize, const T& val = T())
    {
        if (newSize) {
            if (newSize > m_capacity) {
                size_t newCapacity = computeAllocateSize(newSize);
                T* newBuffer = Allocator().allocate(newCapacity);

                if (std::is_fundamental<T>()) {
                    memcpy(newBuffer, m_buffer, sizeof(T) * std::min(m_size, newSize));
                } else {
                    for (size_t i = 0; i < m_size && i < newSize; i++) {
                        newBuffer[i] = m_buffer[i];
                    }
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
        if (newSize == 0) {
            return 1;
        }
        size_t base = log2l(newSize);
        size_t capacity = 1 << (base + 1);
        return capacity * glowFactor / 100.f;
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

            if (std::is_fundamental<T>()) {
                memcpy(newBuffer, m_buffer, sizeof(T) * (newSize - 1));
            } else {
                for (size_t i = 0; i < (newSize - 1); i++) {
                    newBuffer[i] = m_buffer[i];
                }
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

                if (std::is_fundamental<T>()) {
                    memcpy(newBuffer, m_buffer, sizeof(T) * std::min(oldSize, newSize));
                } else {
                    for (size_t i = 0; i < oldSize && i < newSize; i++) {
                        newBuffer[i] = m_buffer[i];
                    }
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
    size_t computeAllocateSize(size_t siz)
    {
        if (siz == 0) {
            return 1;
        }
        size_t base = log2l(siz);
        size_t capacity = 1 << (base + 1);
        return capacity * glowFactor / 100.f;
    }

    T* m_buffer;
    size_t m_capacity;
};


class VectorUtil {
public:
    static const size_t invalidIndex;

    template <typename T, typename E>
    static size_t findInVector(const T& vector, const E& target)
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
