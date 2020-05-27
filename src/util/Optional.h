/*
 * Copyright (c) 2019-present Samsung Electronics Co., Ltd
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

#ifndef __EscargotOptional__
#define __EscargotOptional__

namespace Escargot {

template <typename T>
class Optional {
public:
    Optional()
        : m_hasValue(false)
        , m_value()
    {
    }

    Optional(T value)
        : m_hasValue(true)
        , m_value(value)
    {
    }

    Optional(std::nullptr_t value)
        : m_hasValue(false)
        , m_value()
    {
    }

    T& value()
    {
        ASSERT(m_hasValue);
        return m_value;
    }

    const T& value() const
    {
        ASSERT(m_hasValue);
        return m_value;
    }

    bool hasValue() const
    {
        return m_hasValue;
    }

    operator bool() const
    {
        return hasValue();
    }

    void reset()
    {
        m_value = T();
        m_hasValue = false;
    }

    bool operator==(const Optional<T>& other) const
    {
        if (m_hasValue != other.hasValue()) {
            return false;
        }
        return m_hasValue ? m_value == other.m_value : true;
    }

    bool operator!=(const Optional<T>& other) const
    {
        return !this->operator==(other);
    }

    bool operator==(const T& other) const
    {
        if (m_hasValue) {
            return value() == other;
        }
        return false;
    }

    bool operator!=(const T& other) const
    {
        return !operator==(other);
    }

protected:
    bool m_hasValue;
    T m_value;
};

template <typename T>
inline bool operator==(const T& a, const Optional<T>& b)
{
    return b == a;
}

template <typename T>
inline bool operator!=(const T& a, const Optional<T>& b)
{
    return b != a;
}

template <typename T>
class Optional<T*> {
public:
    Optional()
        : m_value(nullptr)
    {
    }

    Optional(T* value)
        : m_value(value)
    {
    }

    Optional(std::nullptr_t value)
        : m_value(nullptr)
    {
    }

    T* value()
    {
        ASSERT(hasValue());
        return m_value;
    }

    const T* value() const
    {
        ASSERT(hasValue());
        return m_value;
    }

    bool hasValue() const
    {
        return !!m_value;
    }

    operator bool() const
    {
        return hasValue();
    }

    void reset()
    {
        m_value = nullptr;
    }

    T* operator->()
    {
        ASSERT(hasValue());
        return m_value;
    }

    const T* operator->() const
    {
        ASSERT(hasValue());
        return m_value;
    }

    bool operator==(const Optional<T*>& other) const
    {
        if (hasValue() != other.hasValue()) {
            return false;
        }
        return hasValue() ? m_value == other.m_value : true;
    }

    bool operator!=(const Optional<T*>& other) const
    {
        return !this->operator==(other);
    }

    bool operator==(const T*& other) const
    {
        if (hasValue()) {
            return value() == other;
        }
        return false;
    }

    bool operator!=(const T*& other) const
    {
        return !operator==(other);
    }

protected:
    T* m_value;
};

template <typename T>
inline bool operator==(const T*& a, const Optional<T*>& b)
{
    return b == a;
}

template <typename T>
inline bool operator!=(const T*& a, const Optional<T*>& b)
{
    return b != a;
}
}

#endif
