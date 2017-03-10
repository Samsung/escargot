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

#ifndef __EscargotPropertyName__
#define __EscargotPropertyName__

#include "runtime/AtomicString.h"
#include "runtime/ExecutionState.h"
#include "runtime/String.h"

namespace Escargot {

class PropertyName {
public:
    PropertyName()
    {
        m_data = ((size_t)AtomicString().string()) + 1;
        ASSERT(m_data);
    }

    PropertyName(ExecutionState& state, const AtomicString& atomicString)
    {
        m_data = ((size_t)atomicString.string() + 1);
        ASSERT(m_data);
    }

    PropertyName(const AtomicString& atomicString)
    {
        m_data = ((size_t)atomicString.string() + 1);
        ASSERT(m_data);
    }

    PropertyName(ExecutionState& state, String* string);
    String* string() const
    {
        if (hasAtomicString()) {
            return (String*)(m_data - 1);
        } else {
            return (String*)m_data;
        }
    }

    AtomicString atomicString(ExecutionState& state)
    {
        if (hasAtomicString()) {
            return AtomicString((String*)(m_data - 1));
        } else {
            return AtomicString(state, (String*)m_data);
        }
    }

    ALWAYS_INLINE friend bool operator==(const PropertyName& a, const PropertyName& b);
    ALWAYS_INLINE friend bool operator!=(const PropertyName& a, const PropertyName& b);

    size_t rawData() const
    {
        return m_data;
    }

    ALWAYS_INLINE bool hasAtomicString() const
    {
        return LIKELY(m_data & 1);
    }

protected:
    size_t m_data;
    // AtomicString <- saves its (String* | 1)
    // String* <- saves pointer
};

ALWAYS_INLINE bool operator==(const PropertyName& a, const PropertyName& b)
{
    if (LIKELY(LIKELY(a.hasAtomicString()) && LIKELY(b.hasAtomicString()))) {
        return a.m_data == b.m_data;
    } else {
        return a.string()->equals(b.string());
    }
}

ALWAYS_INLINE bool operator!=(const PropertyName& a, const PropertyName& b)
{
    return !operator==(a, b);
}

typedef std::unordered_map<PropertyName, size_t, std::hash<PropertyName>, std::equal_to<PropertyName>, gc_allocator<std::pair<PropertyName, size_t>>> PropertyNameMap;
}

namespace std {
template <>
struct hash<Escargot::PropertyName> {
    size_t operator()(Escargot::PropertyName const& x) const
    {
        return x.string()->hashValue();
    }
};

template <>
struct equal_to<Escargot::PropertyName> {
    bool operator()(Escargot::PropertyName const& a, Escargot::PropertyName const& b) const
    {
        return a == b;
    }
};
}

#endif
