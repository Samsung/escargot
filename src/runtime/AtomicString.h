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

#ifndef __EscargotAtomicString__
#define __EscargotAtomicString__

#include "runtime/ExecutionState.h"
#include "runtime/String.h"
#include "util/Vector.h"

namespace Escargot {

typedef std::unordered_set<String*,
                           std::hash<String*>, std::equal_to<String*>,
                           GCUtil::gc_malloc_ignore_off_page_allocator<String*> >
    AtomicStringMap;

class AtomicString : public gc {
    friend class StaticStrings;
    friend class PropertyName;
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
}


#endif
