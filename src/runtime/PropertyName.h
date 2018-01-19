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
#include "runtime/Symbol.h"
#include "runtime/Value.h"

namespace Escargot {

#define PROPERTY_NAME_ATOMIC_STRING_VIAS 1

class PropertyName {
public:
    PropertyName(const AtomicString& atomicString)
    {
        m_data = ((size_t)atomicString.string() + PROPERTY_NAME_ATOMIC_STRING_VIAS);
        ASSERT(m_data);
    }

    PropertyName(ExecutionState& state, const Value& value);
    size_t hashValue() const
    {
        if (hasAtomicString()) {
            return ((String*)(m_data - PROPERTY_NAME_ATOMIC_STRING_VIAS))->hashValue();
        } else if (hasSymbol()) {
            return m_data;
        }
        return ((String*)m_data)->hashValue();
    }

    ALWAYS_INLINE friend bool operator==(const PropertyName& a, const PropertyName& b);
    ALWAYS_INLINE friend bool operator!=(const PropertyName& a, const PropertyName& b);

    ALWAYS_INLINE bool isSymbol() const
    {
        if (hasAtomicString()) {
            return false;
        }
        PointerValue* pa = (PointerValue*)m_data;
        if (UNLIKELY(pa->hasTag(g_symbolTag))) {
            return true;
        }
        return false;
    }

    ALWAYS_INLINE bool hasAtomicString() const
    {
        return LIKELY(m_data & PROPERTY_NAME_ATOMIC_STRING_VIAS);
    }

    bool isPlainString() const
    {
        if (hasAtomicString()) {
            return true;
        } else if (hasSymbol()) {
            return false;
        }
        return true;
    }

    String* plainString() const
    {
        ASSERT(isPlainString());
        if (hasAtomicString()) {
            return ((String*)(m_data - PROPERTY_NAME_ATOMIC_STRING_VIAS));
        }
        return ((String*)m_data);
    }

    bool isIndexString() const
    {
        return isPlainString() && ::Escargot::isIndexString(plainString());
    }

    Value toValue() const
    {
        if (hasAtomicString()) {
            return Value((String*)(m_data - PROPERTY_NAME_ATOMIC_STRING_VIAS));
        } else {
            return Value((PointerValue*)m_data);
        }
    }

    uint64_t tryToUseAsIndex() const
    {
        if (isPlainString()) {
            return plainString()->tryToUseAsIndex();
        }
        return Value::InvalidIndexValue;
    }

    uint64_t tryToUseAsArrayIndex() const
    {
        if (isPlainString()) {
            return plainString()->tryToUseAsArrayIndex();
        }
        return Value::InvalidArrayIndexValue;
    }

    bool equals(String* s) const
    {
        if (isPlainString()) {
            return plainString()->equals(s);
        }
        return false;
    }

    String* toExceptionString() const
    {
        if (isPlainString()) {
            return plainString();
        }
        return ((Symbol*)m_data)->getSymbolDescriptiveString();
    }

protected:
    ALWAYS_INLINE bool hasSymbol() const
    {
        ASSERT(!hasAtomicString());
        PointerValue* pa = (PointerValue*)m_data;
        if (UNLIKELY(pa->hasTag(g_symbolTag))) {
            return true;
        }
        return false;
    }
    size_t m_data;
    // AtomicString <- saves its (String* | PROPERTY_NAME_ATOMIC_STRING_VIAS)
    // String*, Symbol* <- saves pointer
};

ALWAYS_INLINE bool operator==(const PropertyName& a, const PropertyName& b)
{
    bool aa = a.hasAtomicString();
    bool ab = b.hasAtomicString();
    if (LIKELY(LIKELY(aa) && LIKELY(ab))) {
        return a.m_data == b.m_data;
    } else {
        bool sa = !aa && a.hasSymbol();
        bool sb = !ab && b.hasSymbol();
        if (sa && sb) { // a, b both are symbol
            return a.m_data == b.m_data;
        } else if (!sa && !sb) { // a, b both are string
            return a.plainString()->equals(b.plainString());
        } else {
            return false;
        }
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
        return x.hashValue();
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
