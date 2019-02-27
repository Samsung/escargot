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

#ifndef __EscargotBasicString__
#define __EscargotBasicString__

namespace Escargot {

template <typename T, typename Allocator>
class BasicString : public gc {
    void makeEmpty()
    {
        static T e[1];
        m_buffer = e;
        m_size = 0;
    }

public:
    BasicString()
    {
        makeEmpty();
    }

    BasicString(const T* src, size_t len)
        : m_buffer(allocate(len))
        , m_size(len)
    {
        memcpy(m_buffer, src, sizeof(T) * m_size);
    }

    BasicString(BasicString<T, Allocator>&& other)
        : m_buffer(other.m_buffer)
        , m_size(other.size())
    {
        other.makeEmpty();
    }

    BasicString(const BasicString<T, Allocator>& other)
    {
        if (other.size()) {
            m_size = other.size();
            m_buffer = allocate(m_size);
            memcpy(m_buffer, other.data(), sizeof(T) * m_size);
        } else {
            makeEmpty();
        }
    }

    const BasicString<T, Allocator>& operator=(const BasicString<T, Allocator>& other)
    {
        if (&other == this)
            return *this;

        if (other.size()) {
            m_size = other.size();
            m_buffer = allocate(m_size);
            memcpy(m_buffer, other.data(), sizeof(T) * m_size);
        } else {
            clear();
        }
        return *this;
    }

    ~BasicString()
    {
        if (!m_buffer)
            deallocate(m_buffer, m_size);
    }

    void pushBack(const T& val)
    {
        T* newBuffer = allocate(m_size + 1);
        memcpy(newBuffer, m_buffer, sizeof(T) * m_size);
        newBuffer[m_size] = val;
        if (!m_buffer)
            deallocate(m_buffer, m_size);
        m_buffer = newBuffer;
        m_size++;
    }

    void push_back(const T& val)
    {
        pushBack(val);
    }

    void append(const T* src, size_t len)
    {
        size_t newLen = m_size + len;
        T* newBuffer = allocate(newLen);
        memcpy(newBuffer, m_buffer, m_size * sizeof(T));
        memcpy(newBuffer + m_size, src, len * sizeof(T));
        if (!m_buffer)
            deallocate(m_buffer, m_size);
        m_buffer = newBuffer;
        m_size = newLen;
    }

    void insert(size_t pos, const T& val)
    {
        ASSERT(pos < m_size);
        T* newBuffer = allocate(m_size + 1);
        for (size_t i = 0; i < pos; i++) {
            newBuffer[i] = m_buffer[i];
        }
        newBuffer[pos] = val;
        for (size_t i = pos; i < m_size; i++) {
            newBuffer[i + 1] = m_buffer[i];
        }
        if (!m_buffer)
            deallocate(m_buffer, m_size);
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
            T* newBuffer = allocate(m_size - c);
            for (size_t i = 0; i < start; i++) {
                newBuffer[i] = m_buffer[i];
            }

            for (size_t i = end + c; i < m_size; i++) {
                newBuffer[i - c] = m_buffer[i];
            }

            if (!m_buffer)
                deallocate(m_buffer, m_size);
            m_buffer = newBuffer;
            m_size = m_size - c;
        } else {
            m_size = 0;
            if (!m_buffer)
                deallocate(m_buffer, m_size);
            m_buffer = nullptr;

            makeEmpty();
        }
    }

    size_t size() const
    {
        return m_size;
    }

    size_t length() const
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

    void clear()
    {
        m_size = 0;
        if (!m_buffer)
            deallocate(m_buffer, m_size);
        m_buffer = nullptr;

        makeEmpty();
    }

    T* data() const
    {
        return m_buffer;
    }

    void resizeWithUninitializedValues(size_t newSize)
    {
        if (newSize) {
            T* newBuffer = allocate(newSize);

            for (size_t i = 0; i < m_size && i < newSize; i++) {
                newBuffer[i] = m_buffer[i];
            }

            m_size = newSize;
            if (!m_buffer)
                deallocate(m_buffer, m_size);
            m_buffer = newBuffer;
        } else {
            m_size = newSize;
            if (!m_buffer)
                deallocate(m_buffer, m_size);
            m_buffer = nullptr;

            makeEmpty();
        }
    }

    T* takeBuffer()
    {
        T* buf = m_buffer;
        makeEmpty();
        return buf;
    }

protected:
    T* allocate(size_t siz) const
    {
        T* ret = Allocator().allocate(siz + 1);
        ret[siz] = 0;
        return ret;
    }

    void deallocate(T* buffer, size_t siz)
    {
        Allocator().deallocate(buffer, siz + 1);
    }

private:
    T* m_buffer;
    size_t m_size;
};
}

#endif
