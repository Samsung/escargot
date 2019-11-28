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

    PropertyName(Symbol* symbol)
    {
        m_data = (size_t)symbol;
    }

    PropertyName(ExecutionState& state, const Value& value);
    size_t hashValue() const
    {
        if (LIKELY(hasAtomicString())) {
            std::hash<Escargot::AtomicString> hasher;
            return hasher(asAtomicString());
        } else if (hasSymbol()) {
            return m_data;
        }
        return ((String*)m_data)->hashValue();
    }

    ALWAYS_INLINE friend bool operator==(const PropertyName& a, const PropertyName& b);
    ALWAYS_INLINE friend bool operator!=(const PropertyName& a, const PropertyName& b);

    ALWAYS_INLINE friend bool operator==(const PropertyName& a, const AtomicString& b);
    ALWAYS_INLINE friend bool operator!=(const PropertyName& a, const AtomicString& b);

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

    AtomicString asAtomicString() const
    {
        ASSERT(hasAtomicString());
        return AtomicString(((String*)(m_data - PROPERTY_NAME_ATOMIC_STRING_VIAS)));
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
            auto ps = plainString();
            return s == ps || plainString()->equals(s);
        }
        return false;
    }

    String* toExceptionString() const
    {
        if (isPlainString()) {
            return plainString();
        }
        return ((Symbol*)m_data)->symbolDescriptiveString();
    }

    size_t rawValue() const
    {
        return m_data;
    }

private:
    size_t m_data;

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
        if (UNLIKELY(sa && sb)) { // a, b both are symbol
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

ALWAYS_INLINE bool operator==(const PropertyName& a, const AtomicString& b)
{
    bool aa = a.hasAtomicString();
    if (LIKELY(aa)) {
        return a.asAtomicString() == b;
    } else {
        bool sa = a.hasSymbol();
        if (LIKELY(!sa)) { // a is string
            return a.plainString()->equals(b.string());
        } else {
            return false;
        }
    }
}

ALWAYS_INLINE bool operator!=(const PropertyName& a, const AtomicString& b)
{
    return !operator==(a, b);
}

typedef std::unordered_map<PropertyName, size_t, std::hash<PropertyName>, std::equal_to<PropertyName>, GCUtil::gc_malloc_allocator<std::pair<const PropertyName, size_t>>> PropertyNameMap;
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
