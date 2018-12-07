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
    ALWAYS_INLINE StringView(const char* str)
    {
        m_string = nullptr;
        m_bufferAccessData.has8BitContent = true;
        m_bufferAccessData.length = strlen(str);
        m_bufferAccessData.buffer = str;
    }

    ALWAYS_INLINE StringView(const char* str, size_t len)
    {
        m_string = nullptr;
        m_bufferAccessData.has8BitContent = true;
        m_bufferAccessData.length = len;
        m_bufferAccessData.buffer = str;
    }

    ALWAYS_INLINE StringView(String* str, const size_t& s, const size_t& e)
        : String()
        , m_string(str)
    {
        ASSERT(s <= e);
        ASSERT(e <= str->length());
        initBufferAccessData(str->bufferAccessData(), s, e);
    }

    ALWAYS_INLINE StringView(const StringView& str, const size_t& s, const size_t& e)
        : String()
        , m_string(str.string())
    {
        initBufferAccessData(str.bufferAccessData(), s, e);
    }

    ALWAYS_INLINE StringView()
        : String()
        , m_string(String::emptyString)
    {
        initBufferAccessData(String::emptyString->bufferAccessData(), 0, 0);
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
        return bufferAccessData().toUTF8String<UTF8StringData, UTF8StringDataNonGCStd>();
    }

    virtual UTF8StringDataNonGCStd toNonGCUTF8StringData() const
    {
        return bufferAccessData().toUTF8String<UTF8StringDataNonGCStd>();
    }

    virtual const LChar* characters8() const
    {
        ASSERT(has8BitContent());
        return (LChar*)m_bufferAccessData.buffer;
    }

    virtual const char16_t* characters16() const
    {
        ASSERT(!has8BitContent());
        return (const char16_t*)m_bufferAccessData.buffer;
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

protected:
    ALWAYS_INLINE void initBufferAccessData(const StringBufferAccessData& srcData, size_t start, size_t end)
    {
        m_bufferAccessData.has8BitContent = srcData.has8BitContent;
        m_bufferAccessData.length = end - start;
        if (srcData.has8BitContent) {
            m_bufferAccessData.buffer = ((LChar*)srcData.buffer) + start;
        } else {
            m_bufferAccessData.buffer = ((char16_t*)srcData.buffer) + start;
        }
    }

    String* m_string;
};

class SourceStringView : public String {
public:
    ALWAYS_INLINE SourceStringView(const StringView& str)
    {
        m_bufferAccessData = str.bufferAccessData();
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
        return bufferAccessData().toUTF8String<UTF8StringData, UTF8StringDataNonGCStd>();
    }

    virtual UTF8StringDataNonGCStd toNonGCUTF8StringData() const
    {
        return bufferAccessData().toUTF8String<UTF8StringDataNonGCStd>();
    }

    virtual const LChar* characters8() const
    {
        ASSERT(has8BitContent());
        return (LChar*)m_bufferAccessData.buffer;
    }

    virtual const char16_t* characters16() const
    {
        ASSERT(!has8BitContent());
        return (const char16_t*)m_bufferAccessData.buffer;
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
};
}

#endif
