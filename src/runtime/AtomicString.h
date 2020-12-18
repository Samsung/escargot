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

#ifndef __EscargotAtomicString__
#define __EscargotAtomicString__

#include "runtime/ExecutionState.h"
#include "runtime/String.h"
#include "util/Vector.h"
#include "util/TightVector.h"

namespace Escargot {

typedef std::unordered_set<String*, std::hash<String*>, std::equal_to<String*>, GCUtil::gc_malloc_allocator<String*> > AtomicStringMap;

class AtomicString : public gc {
    friend class StaticStrings;
    friend class ObjectStructurePropertyName;
    friend class ASTBlockContextNameInfo;
    friend class AtomicStringRef;
    inline explicit AtomicString(String* str)
    {
        m_string = str;
    }

public:
    inline AtomicString()
        : m_string(String::emptyString)
    {
    }

    inline AtomicString(const AtomicString& src)
        : m_string(src.m_string)
    {
    }

    static AtomicString fromPayload(void* payload)
    {
        return AtomicString((String*)payload);
    }

    AtomicString(AtomicStringMap* map, const char* src, size_t len);
    enum FromExternalMemoryTag {
        FromExternalMemory
    };

    AtomicString(AtomicStringMap* map, const char* src, size_t len, FromExternalMemoryTag);
    AtomicString(AtomicStringMap* map, const LChar* src, size_t len);
    AtomicString(AtomicStringMap* map, const char16_t* src, size_t len);
    AtomicString(ExecutionState& ec, String* name);
    AtomicString(ExecutionState& ec, const char16_t* src, size_t len);
    AtomicString(ExecutionState& ec, const char* src, size_t len);
    template <const size_t srcLen>
    AtomicString(ExecutionState& ec, const char (&src)[srcLen])
        : AtomicString(ec, src, srcLen - 1)
    {
    }
    AtomicString(ExecutionState& ec, const char* src);
    AtomicString(Context* c, const char16_t* src, size_t len);
    AtomicString(Context* c, const LChar* src, size_t len);
    AtomicString(Context* c, const char* src, size_t len);
    template <const size_t srcLen>
    AtomicString(Context* c, const char (&src)[srcLen])
        : AtomicString(c, src, srcLen - 1)
    {
    }
    AtomicString(Context* c, const StringView& sv);
    AtomicString(Context* c, String* name);

    inline String* string() const
    {
        return m_string;
    }

    inline friend bool operator==(const AtomicString& a, const AtomicString& b);
    inline friend bool operator!=(const AtomicString& a, const AtomicString& b);

    bool operator==(const char* src) const
    {
        size_t srcLen = strlen(src);
        if (srcLen != m_string->length()) {
            return false;
        }

        for (size_t i = 0; i < srcLen; i++) {
            if (src[i] != m_string->charAt(i)) {
                return false;
            }
        }

        return true;
    }

    StringBufferAccessData bufferAccessData() const
    {
        return m_string->bufferAccessData();
    }

private:
    void init(AtomicStringMap* ec, const char* src, size_t len, bool fromExternalMemory = false);
    void init(AtomicStringMap* ec, const LChar* str, size_t len);
    void init(AtomicStringMap* ec, const char16_t* src, size_t len);
    void init(AtomicStringMap* ec, String* name);
    void initStaticString(AtomicStringMap* ec, String* name);
    String* m_string;
};

COMPILE_ASSERT(sizeof(AtomicString) == sizeof(size_t), "");

inline bool operator==(const AtomicString& a, const AtomicString& b)
{
    return a.string() == b.string();
}

inline bool operator!=(const AtomicString& a, const AtomicString& b)
{
    return !operator==(a, b);
}

typedef Vector<AtomicString, GCUtil::gc_malloc_atomic_allocator<AtomicString> > AtomicStringVector;
typedef TightVector<AtomicString, GCUtil::gc_malloc_atomic_allocator<AtomicString> > AtomicStringTightVector;

template <>
class Optional<AtomicString> {
public:
    Optional()
        : m_value(AtomicString::fromPayload(nullptr))
    {
    }

    Optional(AtomicString value)
        : m_value(value)
    {
    }

    AtomicString value()
    {
        ASSERT(hasValue());
        return m_value;
    }

    AtomicString value() const
    {
        ASSERT(hasValue());
        return m_value;
    }

    bool hasValue() const
    {
        return m_value.string();
    }

    operator bool() const
    {
        return hasValue();
    }

    bool operator==(const Optional<AtomicString>& other) const
    {
        if (hasValue() != other.hasValue()) {
            return false;
        }
        return hasValue() ? m_value == other.m_value : true;
    }

    bool operator!=(const Optional<AtomicString>& other) const
    {
        return !this->operator==(other);
    }

    bool operator==(const AtomicString& other) const
    {
        if (hasValue()) {
            return value() == other;
        }
        return false;
    }

    bool operator!=(const AtomicString& other) const
    {
        return !operator==(other);
    }

protected:
    AtomicString m_value;
};
} // namespace Escargot

namespace std {

template <>
struct is_fundamental<Escargot::AtomicString> : public true_type {
};

template <>
struct hash<Escargot::AtomicString> {
    size_t operator()(Escargot::AtomicString const& x) const
    {
        return std::hash<size_t*>{}((size_t*)x.string());
    }
};

template <>
struct equal_to<Escargot::AtomicString> {
    bool operator()(Escargot::AtomicString const& a, Escargot::AtomicString const& b) const
    {
        return a == b;
    }
};
} // namespace std

#endif
