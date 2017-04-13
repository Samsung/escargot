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

#ifndef __EscargotStringView__
#define __EscargotStringView__

#include "runtime/String.h"

namespace Escargot {

class StringView : public String {
public:
    StringView(String* str, const size_t& s, const size_t& e)
        : m_string(str)
        , m_start(s)
        , m_end(e)
    {
        ASSERT(s <= e);
        ASSERT(e <= m_string->length());
    }

    StringView(const StringView& str, const size_t& s, const size_t& e)
        : m_string(str.string())
        , m_start(s + str.start())
        , m_end(e + str.start())
    {
    }

    StringView()
    {
        m_string = String::emptyString;
        m_start = 0;
        m_end = 0;
    }

    virtual bool isStringView()
    {
        return true;
    }

    virtual char16_t charAt(const size_t& idx) const
    {
        return m_string->charAt(idx + m_start);
    }

    virtual size_t length() const
    {
        return m_end - m_start;
    }

    bool operator==(const char* src) const
    {
        size_t srcLen = strlen(src);
        if (srcLen != length()) {
            return false;
        }

        auto data = bufferAccessData();
        if (data.has8BitContent) {
            for (size_t i = 0; i < srcLen; i++) {
                if (src[i] != ((const LChar*)data.buffer)[i]) {
                    return false;
                }
            }
        } else {
            for (size_t i = 0; i < srcLen; i++) {
                if (src[i] != ((const char16_t*)data.buffer)[i]) {
                    return false;
                }
            }
        }

        return true;
    }

    bool operator!=(const char* src) const
    {
        return !operator==(src);
    }

    virtual UTF16StringData toUTF16StringData() const
    {
        UTF16StringData ret;
        size_t len = length();
        ret.resizeWithUninitializedValues(len);

        for (size_t i = 0; i < len; i++) {
            ret[i] = charAt(i);
        }

        return ret;
    }

    virtual UTF8StringData toUTF8StringData() const
    {
        // FIXME optimze this function
        UTF16StringData s = toUTF16StringData();
        return utf16StringToUTF8String(s.data(), s.length());
    }

    String* string() const
    {
        return m_string;
    }

    size_t start() const
    {
        return m_start;
    }

    size_t end() const
    {
        return m_end;
    }

    virtual bool has8BitContent() const
    {
        return m_string->has8BitContent();
    }

    virtual const LChar* characters8() const
    {
        return m_string->characters8() + m_start;
    }

    virtual const char16_t* characters16() const
    {
        return m_string->characters16() + m_start;
    }

    virtual StringBufferAccessData bufferAccessData() const
    {
        auto srcData = m_string->bufferAccessData();
        StringBufferAccessData data;
        data.has8BitContent = srcData.has8BitContent;
        data.length = m_end - m_start;
        if (srcData.has8BitContent) {
            data.buffer = ((LChar*)srcData.buffer) + m_start;
        } else {
            data.buffer = ((char16_t*)srcData.buffer) + m_start;
        }
        return data;
    }

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

protected:
    String* m_string;
    size_t m_start, m_end;
};

class BufferedStringView : public String {
    MAKE_STACK_ALLOCATED();

public:
    BufferedStringView()
        : m_src(StringView())
    {
        init();
    }

    BufferedStringView(StringView src)
        : m_src(src)
    {
        init();
    }

    void init()
    {
        m_data = m_src.bufferAccessData();
    }

    char16_t bufferedCharAt(const size_t& idx) const
    {
        if (m_data.has8BitContent) {
            return ((const LChar*)m_data.buffer)[idx];
        } else {
            return ((const char16_t*)m_data.buffer)[idx];
        }
    }

    virtual char16_t charAt(const size_t& idx) const
    {
        return m_src.charAt(idx);
    }

    virtual size_t length() const
    {
        return m_src.length();
    }

    virtual UTF16StringData toUTF16StringData() const
    {
        UTF16StringData ret;
        size_t len = length();
        ret.resizeWithUninitializedValues(len);

        for (size_t i = 0; i < len; i++) {
            ret[i] = charAt(i);
        }

        return ret;
    }

    virtual UTF8StringData toUTF8StringData() const
    {
        // FIXME optimze this function
        UTF16StringData s = toUTF16StringData();
        return utf16StringToUTF8String(s.data(), s.length());
    }

    virtual bool has8BitContent() const
    {
        return m_data.has8BitContent;
    }

    virtual const LChar* characters8() const
    {
        return m_src.characters8();
    }

    virtual const char16_t* characters16() const
    {
        return m_src.characters16();
    }

    virtual StringBufferAccessData bufferAccessData() const
    {
        return m_src.bufferAccessData();
    }

    StringView src() const
    {
        return m_src;
    }

protected:
    StringBufferAccessData m_data;
    StringView m_src;
};
}

#endif
