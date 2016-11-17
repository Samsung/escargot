#ifndef __EscargotVector__
#define __EscargotVector__

namespace Escargot {

template <typename T, typename Allocator>
class Vector : public gc {
public:
    Vector()
    {
        m_buffer = nullptr;
        m_size = 0;
    }

    Vector(size_t siz)
    {
        m_buffer = nullptr;
        m_size = 0;
        resize(siz);
    }

    Vector(Vector<T, Allocator>&& other)
    {
        m_size = other.size();
        m_buffer = other.m_buffer;
        other.m_buffer = nullptr;
        other.m_size = 0;
    }

    Vector(const Vector<T, Allocator>& other)
    {
        if (other.size()) {
            m_size = other.size();
            m_buffer = Allocator().allocate(m_size);
            for (size_t i = 0; i < m_size; i ++) {
                m_buffer[i] = other[i];
            }
        } else {
            m_buffer = nullptr;
            m_size = 0;
        }
    }

    const Vector<T, Allocator>& operator=(const Vector<T, Allocator>& other)
    {
        if (other.size()) {
            m_size = other.size();
            m_buffer = Allocator().allocate(m_size);
            for (size_t i = 0; i < m_size; i ++) {
                m_buffer[i] = other[i];
            }
        } else {
            clear();
        }
        return *this;
    }

    ~Vector()
    {
        if (!m_buffer)
            Allocator().deallocate(m_buffer, m_size);
    }

    void reserve(size_t unused)
    {
        // TODO
    }

    void push_back(const T& val)
    {
        T* newBuffer = Allocator().allocate(m_size + 1);
        for (size_t i = 0; i < m_size; i ++) {
            newBuffer[i] = m_buffer[i];
        }
        newBuffer[m_size] = val;
        if (!m_buffer)
            Allocator().deallocate(m_buffer, m_size);
        m_buffer = newBuffer;
        m_size++;
    }

    void insert(size_t pos, const T& val)
    {
        ASSERT(pos < m_size);
        T* newBuffer = Allocator().allocate(m_size + 1);
        for (size_t i = 0; i < pos; i ++) {
            newBuffer[i] = m_buffer[i];
        }
        newBuffer[pos] = val;
        for (size_t i = pos; i < m_size; i ++) {
            newBuffer[i + 1] = m_buffer[i];
        }
        if (!m_buffer)
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
            for (size_t i = 0; i < start; i ++) {
                newBuffer[i] = m_buffer[i];
            }

            for (size_t i = end + c; i < m_size; i ++) {
                newBuffer[i - c] = m_buffer[i];
            }

            if (!m_buffer)
                Allocator().deallocate(m_buffer, m_size);
            m_buffer = newBuffer;
            m_size = m_size - c;
        } else {
            m_size = 0;
            if (!m_buffer)
                Allocator().deallocate(m_buffer, m_size);
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
        erase(m_size  - 1);
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

    T& back()
    {
        return m_buffer[m_size - 1];
    }

    void clear()
    {
        m_size = 0;
        if (!m_buffer)
            Allocator().deallocate(m_buffer, m_size);
        m_buffer = nullptr;
    }

    void resize(size_t newSize, const T& val = T())
    {
        if (newSize) {
            T* newBuffer = Allocator().allocate(newSize);

            for (size_t i = 0; i < m_size && i < newSize ; i ++) {
                newBuffer[i] = m_buffer[i];
            }

            for (size_t i = m_size; i < newSize ; i ++) {
                newBuffer[i] = val;
            }

            m_size = newSize;
            if (!m_buffer)
                Allocator().deallocate(m_buffer, m_size);
            m_buffer = newBuffer;
        } else {
            m_size = newSize;
            if (!m_buffer)
                Allocator().deallocate(m_buffer, m_size);
            m_buffer = nullptr;
        }
    }

protected:
    T* m_buffer;
    size_t m_size;
};

}

#endif
