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

    ALWAYS_INLINE StringView(String* str, const size_t& s, const size_t& e)
        : m_string(str)
    {
        RELEASE_ASSERT(s <= e);
        RELEASE_ASSERT(e <= m_string->length());
        initBufferAccessData(s, e);
    }

    ALWAYS_INLINE StringView(const StringView& str, const size_t& s, const size_t& e)
        : m_string(str.string())
    {
        initBufferAccessData(s + str.start(), e + str.start());
    }

    ALWAYS_INLINE StringView()
    {
        m_string = String::emptyString;
        initBufferAccessData(0, 0);
    }

    ALWAYS_INLINE void initBufferAccessData(size_t start, size_t end)
    {
        const auto& srcData = m_string->bufferAccessData();
        StringBufferAccessData data;
        data.has8BitContent = srcData.has8BitContent;
        data.length = end - start;
        if (srcData.has8BitContent) {
            data.buffer = ((LChar*)srcData.buffer) + start;
        } else {
            data.buffer = ((char16_t*)srcData.buffer) + start;
        }
        m_bufferAccessData = data;
    }

    virtual char16_t charAt(const size_t& idx) const
    {
        return bufferAccessData().charAt(idx);
    }

    virtual size_t length() const
    {
        return m_bufferAccessData.length;
    }

    bool operator==(const char* src) const
    {
        size_t srcLen = strlen(src);
        if (srcLen != length()) {
            return false;
        }

        const auto& data = bufferAccessData();
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
        const auto& accessData = bufferAccessData();
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
        const auto& accessData = bufferAccessData();
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
        size_t src = (size_t)string()->bufferAccessData().buffer;
        size_t my = (size_t)bufferAccessData().buffer;

        size_t diff = my - src;
        if (!bufferAccessData().has8BitContent) {
            diff /= 2;
        }

        return diff;
    }

    size_t end() const
    {
        return start() + length();
    }

    virtual const LChar* characters8() const
    {
        return m_string->characters8() + start();
    }

    virtual const char16_t* characters16() const
    {
        return m_string->characters16() + start();
    }

    char16_t bufferedCharAt(const size_t& idx) const
    {
        if (m_bufferAccessData.has8BitContent) {
            return ((const LChar*)m_bufferAccessData.buffer)[idx];
        } else {
            return ((const char16_t*)m_bufferAccessData.buffer)[idx];
        }
    }

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

protected:
    String* m_string;
};

inline String* StringView::createStringView(String* str, const size_t& s, const size_t& e)
{
    return new StringView(str, s, e);
}
}

#endif
