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

#ifndef __EscargotAtomicString__
#define __EscargotAtomicString__

#include "runtime/ExecutionState.h"
#include "runtime/String.h"
#include "util/Vector.h"
#include "util/TightVector.h"

namespace Escargot {

typedef std::unordered_set<String*,
                           std::hash<String*>, std::equal_to<String*>,
                           GCUtil::gc_malloc_ignore_off_page_allocator<String*> >
    AtomicStringMap;

class AtomicString : public gc {
    friend class StaticStrings;
    friend class PropertyName;
    friend class ASTScopeContextNameInfo;
    friend class AtomicStringRef;
    inline AtomicString(String* str)
    {
        m_string = str;
    }

public:
    inline AtomicString()
    {
        m_string = String::emptyString;
    }

    inline AtomicString(const AtomicString& src)
    {
        m_string = src.m_string;
    }

    static AtomicString fromPayload(void* payload)
    {
        return AtomicString((String*)payload);
    }

    AtomicString(ExecutionState& ec, String* name);
    AtomicString(ExecutionState& ec, const char16_t* src, size_t len);
    AtomicString(ExecutionState& ec, const char* src, size_t len);
    AtomicString(ExecutionState& ec, const char* src);
    AtomicString(Context* c, const char16_t* src, size_t len);
    AtomicString(Context* c, const char* src, size_t len);
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

    const StringBufferAccessData& bufferAccessData() const
    {
        return m_string->bufferAccessData();
    }

protected:
    void init(AtomicStringMap* ec, String* name);
    void init(AtomicStringMap* ec, const LChar* str, size_t len)
    {
        init(ec, new Latin1String(str, len));
    }
    void init(AtomicStringMap* ec, const char* str, size_t len)
    {
        init(ec, new ASCIIString(str, len));
    }
    void init(AtomicStringMap* ec, const char16_t* src, size_t len)
    {
        if (isAllASCII(src, len)) {
            init(ec, new ASCIIString(src, len));
        } else {
            init(ec, new UTF16String(src, len));
        }
    }
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

typedef Vector<AtomicString, GCUtil::gc_malloc_atomic_ignore_off_page_allocator<AtomicString> > AtomicStringVector;
typedef TightVector<AtomicString, GCUtil::gc_malloc_atomic_ignore_off_page_allocator<AtomicString> > AtomicStringTightVector;
}


#endif
