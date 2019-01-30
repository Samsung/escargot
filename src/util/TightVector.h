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

#ifndef __EscargotTightVector__
#define __EscargotTightVector__

namespace Escargot {

template <typename T, typename Allocator>
class TightVector : public gc {
public:
    TightVector()
    {
        m_buffer = nullptr;
        m_size = 0;
    }

    TightVector(size_t siz)
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

            if (std::is_fundamental<T>()) {
                memcpy(m_buffer, other.m_buffer, sizeof(T) * m_size);
            } else {
                for (size_t i = 0; i < m_size; i++) {
                    m_buffer[i] = other[i];
                }
            }
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
            if (std::is_fundamental<T>()) {
                memcpy(m_buffer, other.m_buffer, sizeof(T) * m_size);
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

    TightVector(const TightVector<T, Allocator>& other, const T& newItem)
    {
        m_size = other.size() + 1;
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

    ~TightVector()
    {
        if (m_buffer)
            Allocator().deallocate(m_buffer, m_size);
    }

    void pushBack(const T& val)
    {
        T* newBuffer = Allocator().allocate(m_size + 1);
        if (std::is_fundamental<T>()) {
            memcpy(newBuffer, m_buffer, sizeof(T) * m_size);
        } else {
            for (size_t i = 0; i < m_size; i++) {
                newBuffer[i] = m_buffer[i];
            }
        }

        newBuffer[m_size] = val;
        if (m_buffer)
            Allocator().deallocate(m_buffer, m_size);
        m_buffer = newBuffer;
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
        for (size_t i = 0; i < pos; i++) {
            newBuffer[i] = m_buffer[i];
        }
        newBuffer[pos] = val;
        for (size_t i = pos; i < m_size; i++) {
            newBuffer[i + 1] = m_buffer[i];
        }
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
            for (size_t i = 0; i < start; i++) {
                newBuffer[i] = m_buffer[i];
            }

            for (size_t i = end; i < m_size; i++) {
                newBuffer[i - c] = m_buffer[i];
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

            if (std::is_fundamental<T>()) {
                memcpy(newBuffer, m_buffer, sizeof(T) * std::min(m_size, newSize));
            } else {
                for (size_t i = 0; i < m_size && i < newSize; i++) {
                    newBuffer[i] = m_buffer[i];
                }
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

    void resize(size_t newSize, const T& val = T())
    {
        if (newSize) {
            T* newBuffer = Allocator().allocate(newSize);

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

    ~TightVectorWithNoSize()
    {
        if (m_buffer) {
            Allocator().deallocate(m_buffer);
        }
    }

    void pushBack(const T& val, size_t newSize)
    {
        T* newBuffer = Allocator().allocate(newSize);

        if (std::is_fundamental<T>()) {
            memcpy(newBuffer, m_buffer, sizeof(T) * (newSize - 1));
        } else {
            for (size_t i = 0; i < newSize - 1; i++) {
                newBuffer[i] = m_buffer[i];
            }
        }
        newBuffer[newSize - 1] = val;
        if (m_buffer)
            Allocator().deallocate(m_buffer);
        m_buffer = newBuffer;
    }

    void push_back(const T& val, size_t newSize)
    {
        pushBack(val, newSize);
    }

    T& operator[](const size_t& idx)
    {
        return m_buffer[idx];
    }

    const T& operator[](const size_t& idx) const
    {
        return m_buffer[idx];
    }

    void resizeWithUninitializedValues(size_t oldSize, size_t newSize)
    {
        if (newSize) {
            T* newBuffer = Allocator().allocate(newSize);

            for (size_t i = 0; i < oldSize && i < newSize; i++) {
                newBuffer[i] = m_buffer[i];
            }

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
            for (size_t i = 0; i < start; i++) {
                newBuffer[i] = m_buffer[i];
            }

            for (size_t i = end; i < currentSize; i++) {
                newBuffer[i - c] = m_buffer[i];
            }

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
}

#endif
