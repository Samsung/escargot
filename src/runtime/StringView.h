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
    inline static String* createStringView(const StringView& v)
    {
        return createStringView(v.string(), v.start(), v.end());
    }
    inline static String* createStringView(String* str, const size_t& s, const size_t& e);

    StringView(String* str, const size_t& s, const size_t& e)
        : m_string(str)
        , m_start(s)
        , m_end(e)
    {
        RELEASE_ASSERT(s <= e);
        RELEASE_ASSERT(e <= m_string->length());
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
        UTF8StringDataNonGCStd ret;
        auto accessData = bufferAccessData();
        for (size_t i = 0; i < accessData.length; i++) {
            char16_t ch = accessData.charAt(i);
            if (ch < 0x80) {
                ret.append((char*)&ch, 1);
            } else {
                char buf[8];
                auto len = utf32ToUtf8(ch, buf);
                ret.append(buf, len);
            }
        }
        return UTF8StringData(ret.data(), ret.length());
    }

    virtual UTF8StringDataNonGCStd toNonGCUTF8StringData() const
    {
        UTF8StringDataNonGCStd ret;
        auto accessData = bufferAccessData();
        for (size_t i = 0; i < accessData.length; i++) {
            char16_t ch = accessData.charAt(i);
            if (ch < 0x80) {
                ret.append((char*)&ch, 1);
            } else {
                char buf[8];
                auto len = utf32ToUtf8(ch, buf);
                ret.append(buf, len);
            }
        }
        return ret;
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
        UTF8StringDataNonGCStd ret;
        auto accessData = bufferAccessData();
        for (size_t i = 0; i < accessData.length; i++) {
            char16_t ch = accessData.charAt(i);
            if (ch < 0x80) {
                ret.append((char*)&ch, 1);
            } else {
                char buf[8];
                auto len = utf32ToUtf8(ch, buf);
                ret.append(buf, len);
            }
        }
        return UTF8StringData(ret.data(), ret.length());
    }

    virtual UTF8StringDataNonGCStd toNonGCUTF8StringData() const
    {
        UTF8StringDataNonGCStd ret;
        auto accessData = bufferAccessData();
        for (size_t i = 0; i < accessData.length; i++) {
            char16_t ch = accessData.charAt(i);
            if (ch < 0x80) {
                ret.append((char*)&ch, 1);
            } else {
                char buf[8];
                auto len = utf32ToUtf8(ch, buf);
                ret.append(buf, len);
            }
        }
        return ret;
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

class ShortStringView : public String {
public:
    ShortStringView(String* str, const uint16_t& s, const uint16_t& e)
        : m_string(str)
        , m_start(s)
        , m_end(e)
    {
        ASSERT(s <= e);
        ASSERT(e <= m_string->length());
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
        UTF8StringDataNonGCStd ret;
        auto accessData = bufferAccessData();
        for (size_t i = 0; i < accessData.length; i++) {
            char16_t ch = accessData.charAt(i);
            if (ch < 0x80) {
                ret.append((char*)&ch, 1);
            } else {
                char buf[8];
                auto len = utf32ToUtf8(ch, buf);
                ret.append(buf, len);
            }
        }
        return UTF8StringData(ret.data(), ret.length());
    }

    virtual UTF8StringDataNonGCStd toNonGCUTF8StringData() const
    {
        UTF8StringDataNonGCStd ret;
        auto accessData = bufferAccessData();
        for (size_t i = 0; i < accessData.length; i++) {
            char16_t ch = accessData.charAt(i);
            if (ch < 0x80) {
                ret.append((char*)&ch, 1);
            } else {
                char buf[8];
                auto len = utf32ToUtf8(ch, buf);
                ret.append(buf, len);
            }
        }
        return ret;
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
    uint16_t m_start, m_end;
};

inline String* StringView::createStringView(String* str, const size_t& s, const size_t& e)
{
    if (s <= std::numeric_limits<uint16_t>::max() && e <= std::numeric_limits<uint16_t>::max()) {
        return new ShortStringView(str, s, e);
    }
    return new StringView(str, s, e);
}
}

#endif
