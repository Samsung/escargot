/*
 * Copyright (c) 2019-present Samsung Electronics Co., Ltd
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

#ifndef __EscargotParserStringView__
#define __EscargotParserStringView__

namespace Escargot {

class ParserStringView : public String {
public:
    ALWAYS_INLINE ParserStringView(const ParserStringView& str, const size_t s, const size_t e)
        : String()
    {
        initBufferAccessData(str.bufferAccessData(), s, e);
    }

    ALWAYS_INLINE ParserStringView(const StringView& str, const size_t s, const size_t e)
        : String()
    {
        initBufferAccessData(str.bufferAccessData(), s, e);
    }

    ALWAYS_INLINE ParserStringView(String* str)
        : String()
    {
        initBufferAccessData(str->bufferAccessData(), 0, str->length());
    }

    ALWAYS_INLINE ParserStringView(String* str, size_t s, size_t e)
        : String()
    {
        initBufferAccessData(str->bufferAccessData(), s, e);
    }

    ALWAYS_INLINE ParserStringView()
        : String()
    {
        initBufferAccessData(String::emptyString->bufferAccessData(), 0, 0);
    }

    template <const size_t srcLen>
    bool operator==(const char (&src)[srcLen]) const
    {
        return equals(src, srcLen - 1);
    }

    template <const size_t srcLen>
    bool equals(const char (&src)[srcLen]) const
    {
        return equals(src, srcLen - 1);
    }

    bool equals(const char* src, size_t srcLen) const
    {
        if (srcLen != length()) {
            return false;
        }

        const auto& data = m_bufferData;
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

    template <const size_t srcLen>
    bool operator!=(const char (&src)[srcLen]) const
    {
        return !operator==(src);
    }

    virtual UTF16StringData toUTF16StringData() const override
    {
        UTF16StringData ret;
        size_t len = length();
        ret.resizeWithUninitializedValues(len);

        for (size_t i = 0; i < len; i++) {
            ret[i] = charAt(i);
        }

        return ret;
    }

    virtual UTF8StringData toUTF8StringData() const override
    {
        return bufferAccessData().toUTF8String<UTF8StringData, UTF8StringDataNonGCStd>();
    }

    virtual UTF8StringDataNonGCStd toNonGCUTF8StringData(int options = StringWriteOption::NoOptions) const override
    {
        return bufferAccessData().toUTF8String<UTF8StringDataNonGCStd>();
    }

    virtual const LChar* characters8() const override
    {
        ASSERT(has8BitContent());
        return (LChar*)m_bufferData.buffer;
    }

    virtual const char16_t* characters16() const override
    {
        ASSERT(!has8BitContent());
        return (const char16_t*)m_bufferData.buffer;
    }

    virtual bool isStringView() override
    {
        return true;
    }

    char16_t bufferedCharAt(const size_t idx) const
    {
        if (m_bufferData.has8BitContent) {
            return ((const LChar*)m_bufferData.buffer)[idx];
        } else {
            return ((const char16_t*)m_bufferData.buffer)[idx];
        }
    }

    void* operator new(size_t size) = delete;
    void* operator new[](size_t size) = delete;

protected:
    ALWAYS_INLINE void initBufferAccessData(const StringBufferAccessData& srcData, size_t start, size_t end)
    {
        m_bufferData.has8BitContent = srcData.has8BitContent;
        m_bufferData.length = end - start;
        if (srcData.has8BitContent) {
            m_bufferData.buffer = ((LChar*)srcData.buffer) + start;
        } else {
            m_bufferData.buffer = ((char16_t*)srcData.buffer) + start;
        }
    }
};

} // namespace Escargot

#endif
