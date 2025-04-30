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

#ifndef __EscargotStringView__
#define __EscargotStringView__

#include "runtime/String.h"

namespace Escargot {

class StringView : public String {
public:
    // For temporal StringView that is allocated on the stack
    void* operator new(size_t size, void* p)
    {
        return p;
    }

    ALWAYS_INLINE StringView(String* str, const size_t s, const size_t e)
        : String()
    {
        initBufferAccessData(str, s, e);
    }

    ALWAYS_INLINE explicit StringView(String* str)
        : String()
    {
        initBufferAccessData(str, 0, str->length());
    }

    ALWAYS_INLINE StringView(const StringView& str, const size_t s, const size_t e)
        : String()
    {
        initBufferAccessData(str.m_bufferData.bufferAsString, s + str.m_start, e + str.m_start);
    }

    ALWAYS_INLINE StringView()
        : String()
    {
        initBufferAccessData(String::emptyString, 0, 0);
    }

    size_t start() const
    {
        return m_start;
    }

    size_t end() const
    {
        return m_start + m_bufferData.length;
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
        return (LChar*)bufferAccessData().buffer;
    }

    virtual const char16_t* characters16() const override
    {
        ASSERT(!has8BitContent());
        return (const char16_t*)bufferAccessData().buffer;
    }

    virtual bool isStringView() override
    {
        return true;
    }

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

protected:
    virtual StringBufferAccessData bufferAccessDataSpecialImpl() override
    {
        ASSERT(m_bufferData.hasSpecialImpl);

        StringBufferAccessData r = m_bufferData.bufferAsString->bufferAccessData();
        r.length = m_bufferData.length;
        if (r.has8BitContent) {
            r.bufferAs8Bit += m_start;
        } else {
            r.bufferAs16Bit += m_start;
        }

        return r;
    }

    ALWAYS_INLINE void initBufferAccessData(String* str, size_t start, size_t end)
    {
        m_bufferData.hasSpecialImpl = true;
        m_bufferData.bufferAsString = str;
        m_bufferData.has8BitContent = str->has8BitContent();
        m_bufferData.length = end - start;
        m_start = start;
    }

private:
    size_t m_start;
};
} // namespace Escargot

#endif
