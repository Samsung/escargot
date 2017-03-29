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

#ifndef __EscargotRopeString__
#define __EscargotRopeString__

#include "runtime/String.h"

namespace Escargot {

class ExecutionState;

class RopeString : public String {
public:
    RopeString()
        : String()
    {
        m_left = String::emptyString;
        m_right = String::emptyString;
        m_contentLength = 0;
        m_has8BitContent = true;
    }

    // this function not always create RopeString.
    // if (l+r).length() < ROPE_STRING_MIN_LENGTH
    // then create just normalString
    // provide ExecutionState if you need limit of string length(exception can be thrown only in ExecutionState area)
    static String* createRopeString(String* lstr, String* rstr, ExecutionState* state = nullptr);

    virtual size_t length() const
    {
        return m_contentLength;
    }
    virtual char16_t charAt(const size_t& idx) const
    {
        return normalString()->charAt(idx);
    }
    virtual UTF16StringData toUTF16StringData() const
    {
        return normalString()->toUTF16StringData();
    }
    virtual UTF8StringData toUTF8StringData() const
    {
        return normalString()->toUTF8StringData();
    }

    virtual bool has8BitContent() const
    {
        return m_has8BitContent;
    }

    virtual bool isRopeString()
    {
        return true;
    }

    virtual const LChar* characters8() const
    {
        return normalString()->characters8();
    }

    virtual const char16_t* characters16() const
    {
        return normalString()->characters16();
    }

    virtual StringBufferAccessData bufferAccessData() const
    {
        return normalString()->bufferAccessData();
    }

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

protected:
    String* normalString() const
    {
        const_cast<RopeString*>(this)->flattenRopeString();
        return m_left;
    }
    template <typename A, typename B>
    void flattenRopeStringWorker();
    void flattenRopeString();

    String* m_left;
    String* m_right;
    struct {
        bool m_has8BitContent : 1;
#if ESCARGOT_32
        size_t m_contentLength : 31;
#else
        size_t m_contentLength : 63;
#endif
    };

    static_assert(STRING_MAXIMUM_LENGTH < (std::numeric_limits<size_t>::max() / 2), "");
};

static_assert(sizeof(RopeString) <= (sizeof(size_t) * 5), "");
}

#endif
