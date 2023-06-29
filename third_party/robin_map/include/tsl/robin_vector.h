/**
 * MIT License
 *
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef TSL_ROBIN_VECTOR_H
#define TSL_ROBIN_VECTOR_H

#include <type_traits>
#include <initializer_list>
#include <iterator>
#include <cstddef>
#include <cmath>
#include <limits>

namespace tsl {

template <typename T, typename Allocator>
class robin_vector {
    struct compute_capacity {
        size_t operator()(size_t newSize)
        {
            if (newSize == 0) {
                return 0;
            }
            size_t base = log2l(newSize);
            size_t capacity = size_t(1) << (base + 1);
            return capacity;
        }
    };
public:
    robin_vector()
        : m_buffer(nullptr)
        , m_size(0)
        , m_capacity(0)
    {
    }

    explicit robin_vector(Allocator a)
        : robin_vector()
    {
    }

    explicit robin_vector(size_t siz)
        : m_buffer(nullptr)
        , m_size(0)
        , m_capacity(0)
    {
        resize(siz);
    }

    explicit robin_vector(size_t siz, Allocator a)
        : robin_vector(siz)
    {
    }

    robin_vector(robin_vector<T, Allocator>&& other)
        : m_buffer(other.m_buffer)
        , m_size(other.m_size)
        , m_capacity(other.m_capacity)
    {
        other.m_buffer = nullptr;
        other.m_size = 0;
        other.m_capacity = 0;
    }

    robin_vector(const robin_vector<T, Allocator>& other)
        : m_buffer(nullptr)
        , m_size(0)
        , m_capacity(0)
    {
        if (other.size()) {
            m_size = other.m_size;
            m_capacity = other.m_capacity;
            m_buffer = Allocator().allocate(m_capacity);
            for (size_t i = 0; i < m_size; i++) {
                new (&m_buffer[i]) T(other.data()[i]);
            }
        }
    }

    robin_vector(std::initializer_list<T> list)
        : robin_vector()
    {
        reserve(list.size());
        for (auto& i : list) {
            push_back(i);
        }
    }

    Allocator get_allocator() const
    {
        Allocator allocator;
        return allocator;
    }

    const robin_vector<T, Allocator>& operator=(const robin_vector<T, Allocator>& other)
    {
        if (&other == this)
            return *this;

        if (other.size()) {
            m_size = other.size();
            m_capacity = other.m_capacity;
            m_buffer = Allocator().allocate(m_capacity);
            for (size_t i = 0; i < m_size; i++) {
                new (&m_buffer[i]) T(other.data()[i]);
            }
        } else {
            clear();
        }
        return *this;
    }

    const robin_vector<T, Allocator>& operator=(robin_vector<T, Allocator>&& other)
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

    ~robin_vector()
    {
        deallocateBuffer(m_buffer, m_size, m_capacity);
    }

    void push_back(const T& val)
    {
        if (m_capacity <= (m_size + 1)) {
            size_t oldc = m_capacity;
            compute_capacity f;
            m_capacity = f(m_size + 1);
            T* newBuffer = Allocator().allocate(m_capacity);

            if (m_buffer) {
                for (size_t i = 0; i < m_size; i++) {
                    new (&newBuffer[i]) T(m_buffer[i]);
                }

                deallocateBuffer(m_buffer, m_size, oldc);
            }
            m_buffer = newBuffer;
        }
        new (&m_buffer[m_size]) T(val);
        m_size++;
    }

    void insert(size_t pos, const T& val)
    {
        if (m_capacity <= (m_size + 1)) {
            size_t oldC = m_capacity;

            compute_capacity f;
            m_capacity = f(m_size + 1);
            T* newBuffer = Allocator().allocate(m_capacity);

            for (size_t i = 0; i < pos; i++) {
                new (&newBuffer[i]) T(m_buffer[i]);
            }

            new (&newBuffer[pos]) T(val);

            auto newPartPtr = &newBuffer[pos + 1];
            auto newSrcPtr = &m_buffer[pos];
            auto copySize = m_size - pos;
            for (size_t i = 0; i < copySize; i++) {
                new (&newPartPtr[i]) T(newSrcPtr[i]);
            }

            deallocateBuffer(m_buffer, m_size, m_capacity);
            m_buffer = newBuffer;
        } else {
            if (m_size) {
                new (&m_buffer[m_size]) T(std::move(m_buffer[m_size - 1]));
            }
            if (m_size > 1) {
                for (size_t i = m_size - 1; i > pos; i--) {
                    m_buffer[i] = std::move(m_buffer[i - 1]);
                }
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
        size_t c = end - start;
        if (m_size - c) {
            size_t oldC = m_capacity;
            T* newBuffer = Allocator().allocate(m_size - c);

            for (size_t i = 0; i < start; i++) {
                new (&newBuffer[i]) T(m_buffer[i]);
            }
            if (m_size > end) {
                auto newPartPtr = &newBuffer[end - c];
                auto newSrcPtr = &m_buffer[end];
                auto copySize = m_size - end;
                for (size_t i = 0; i < copySize; i++) {
                    new (&newPartPtr[i]) T(newSrcPtr[i]);
                }
            }

            deallocateBuffer(m_buffer, m_size, m_capacity);
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

    size_t max_size() const
    {
        return std::numeric_limits<size_t>::max() / sizeof(T);
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

    T& operator[](const size_t idx)
    {
        return m_buffer[idx];
    }

    const T& operator[](const size_t idx) const
    {
        return m_buffer[idx];
    }

    T& at(const size_t& idx)
    {
        return m_buffer[idx];
    }

    const T& at(const size_t& idx) const
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

    T& back()
    {
        return m_buffer[m_size - 1];
    }

    using iterator = T*;
    constexpr iterator begin() const { return m_buffer; }
    constexpr iterator end() const { return m_buffer + m_size; }
    void clear()
    {
        deallocateBuffer(m_buffer, m_size, m_capacity);
        m_buffer = nullptr;
        m_size = 0;
        m_capacity = 0;
    }

    void shrink_to_fit()
    {
        if (m_size != m_capacity) {
            if (m_size) {
                T* newBuffer = Allocator().allocate(m_size);
                for (size_t i = 0; i < m_size; i++) {
                    new (&newBuffer[i]) T(m_buffer[i]);
                }

                deallocateBuffer(m_buffer, m_size, m_capacity);

                m_buffer = newBuffer;
                m_capacity = m_size;
            } else {
                clear();
            }
        }
    }

    void reserve(size_t newCapacity)
    {
        if (m_capacity < newCapacity) {
            T* newBuffer = Allocator().allocate(newCapacity);
            for (size_t i = 0; i < m_size; i++) {
                new (&newBuffer[i]) T(m_buffer[i]);
            }

            deallocateBuffer(m_buffer, m_size, m_capacity);

            m_buffer = newBuffer;
            m_capacity = newCapacity;
        }
    }

    void resize(size_t newSize, const T& val = T())
    {
        if (newSize) {
            if (newSize > m_capacity) {
                compute_capacity f;
                size_t newCapacity = f(newSize);
                T* newBuffer = Allocator().allocate(newCapacity);
                auto copySize = std::min(m_size, newSize);
                for (size_t i = 0; i < copySize; i++) {
                    new (&newBuffer[i]) T(m_buffer[i]);
                }
                for (size_t i = m_size; i < newSize; i++) {
                    new (&newBuffer[i]) T(val);
                }
                deallocateBuffer(m_buffer, m_size, m_capacity);
                m_size = newSize;
                m_capacity = newCapacity;
                m_buffer = newBuffer;
            } else {
                for (size_t i = m_size; i < newSize; i++) {
                    new (&m_buffer[i]) T(val);
                }
                m_size = newSize;
            }
        } else {
            clear();
        }
    }

protected:
    void deallocateBuffer(T* buffer, size_t size, size_t capacity)
    {
        if (buffer) {
            for (size_t i = 0; i < size; i ++) {
                buffer[i].~T();
            }
            Allocator().deallocate(buffer, capacity);
        }
    }

    T* m_buffer;
    size_t m_size;
    size_t m_capacity;
};
} // namespace tsl

#endif
