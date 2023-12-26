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

#ifndef __EscargotByteCodeBlockData__
#define __EscargotByteCodeBlockData__

#include "util/Vector.h"

namespace Escargot {

class ByteCodeBlockData {
    typedef ComputeReservedCapacityFunctionWithLog2<200> ComputeReservedCapacityFunction;

public:
    ByteCodeBlockData()
        : m_buffer(nullptr)
        , m_size(0)
        , m_capacity(0)
    {
    }

    ~ByteCodeBlockData()
    {
        if (m_buffer) {
            free(m_buffer);
        }
    }

    void pushBack(const uint8_t& val)
    {
        if (m_capacity <= (m_size + 1)) {
            size_t oldc = m_capacity;
            ComputeReservedCapacityFunction f;
            m_capacity = f(m_size + 1);
            m_buffer = reinterpret_cast<uint8_t*>(realloc(m_buffer, m_capacity));
        }
        m_buffer[m_size] = val;
        m_size++;
    }

    void push_back(const uint8_t& val)
    {
        pushBack(val);
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

    uint8_t& operator[](const size_t idx)
    {
        ASSERT(idx < m_size);
        return m_buffer[idx];
    }

    const uint8_t& operator[](const size_t idx) const
    {
        ASSERT(idx < m_size);
        return m_buffer[idx];
    }

    uint8_t& at(const size_t& idx)
    {
        ASSERT(idx < m_size);
        return m_buffer[idx];
    }

    const uint8_t& at(const size_t& idx) const
    {
        ASSERT(idx < m_size);
        return m_buffer[idx];
    }

    uint8_t* data()
    {
        return m_buffer;
    }

    const uint8_t* data() const
    {
        return m_buffer;
    }

    uint8_t& back()
    {
        return m_buffer[m_size - 1];
    }

    void clear()
    {
        if (m_buffer) {
            free(m_buffer);
        }
        m_buffer = nullptr;
        m_size = 0;
        m_capacity = 0;
    }

    void shrinkToFit()
    {
        ASSERT(m_size <= m_capacity);
        if (m_size != m_capacity) {
            if (m_size) {
                ASSERT(!!m_buffer);
                m_buffer = reinterpret_cast<uint8_t*>(realloc(m_buffer, m_size));
                m_capacity = m_size;
            } else {
                clear();
            }
        }
    }

    void reserve(size_t newCapacity)
    {
        if (m_capacity < newCapacity) {
            m_buffer = reinterpret_cast<uint8_t*>(realloc(m_buffer, newCapacity));
            m_capacity = newCapacity;
        }
    }

    void resizeFitWithUninitializedValues(size_t newSize)
    {
        if (newSize) {
            if (m_capacity < newSize) {
                m_buffer = reinterpret_cast<uint8_t*>(realloc(m_buffer, newSize));
                m_capacity = newSize;
            }
            m_size = newSize;
        } else {
            clear();
        }
    }

    void resizeWithUninitializedValues(size_t newSize)
    {
        if (newSize) {
            if (m_capacity < newSize) {
                ComputeReservedCapacityFunction f;
                size_t newCapacity = f(newSize);
                m_buffer = reinterpret_cast<uint8_t*>(realloc(m_buffer, newCapacity));
                m_capacity = newCapacity;
            }
            m_size = newSize;
        } else {
            clear();
        }
    }

    uint8_t* takeBuffer()
    {
        uint8_t* buf = m_buffer;
        m_buffer = nullptr;
        clear();
        return buf;
    }

    void* operator new(size_t size) = delete;
    void* operator new[](size_t size) = delete;

protected:
    uint8_t* m_buffer;
    size_t m_size;
    size_t m_capacity;
};
} // namespace Escargot

#endif
