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
    static StringView* create(String* str, size_t start, size_t end);
    static StringView* create(String* str) { return create(str, 0, str->length()); }
    static StringView* create(const StringView& str) { return create(str.underlyingString(), str.start(), str.end()); }
    static StringView* create(const StringView& str, size_t start, size_t end)
    {
        return create(str.underlyingString(), start + str.start(), end + str.start());
    }

    StringView(const StringView& str)
        : String()
    {
        initBufferAccessData(str.underlyingString(), str.start(), str.end());
    }

    StringView& operator=(const StringView& str)
    {
        if (this != &str) {
            initBufferAccessData(str.underlyingString(), str.start(), str.end());
        }
        return *this;
    }

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
        initBufferAccessData(str.underlyingString(), s + str.start(), e + str.start());
    }

    ALWAYS_INLINE StringView()
        : String()
    {
        initBufferAccessData(String::emptyString(), 0, 0);
    }

    size_t start() const
    {
#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
        return m_bufferData.compressedStringData[1];
#else
        return m_start;
#endif
    }

    size_t end() const
    {
        return start() + m_bufferData.length;
    }

    bool hasUnderlyingString() const
    {
        return m_bufferData.hasSpecialImpl;
    }

    ALWAYS_INLINE String* underlyingString() const;

    virtual UTF16StringData toUTF16StringData() const override
    {
        UTF16StringData ret;
        size_t len = length();
        ret.resizeWithUninitializedValues(len);
        auto bad = bufferAccessData();

        for (size_t i = 0; i < len; i++) {
            ret[i] = bad.charAt(i);
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

    virtual bool isStringView() override
    {
        return true;
    }

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

protected:
    virtual StringBufferAccessData bufferAccessDataSpecialImplForRange(size_t rangeStart, size_t rangeLength) override
    {
        ASSERT(m_bufferData.hasSpecialImpl);

        StringBufferAccessData r = underlyingString()->bufferAccessDataForRange(this->start() + rangeStart, rangeLength);
        r.length = m_bufferData.length;
        if (r.has8BitContent) {
            r.bufferAs8Bit += this->start();
        } else {
            r.bufferAs16Bit += this->start();
        }

        return r;
    }

    virtual StringBufferAccessData bufferAccessDataSpecialImpl() override
    {
        return bufferAccessDataSpecialImplForRange(0, length());
    }

    ALWAYS_INLINE void initBufferAccessData(String* str, size_t start, size_t end);

private:
#if !defined(ESCARGOT_USE_32BIT_IN_64BIT)
    size_t m_start;
#endif
};
ALWAYS_INLINE String* StringView::underlyingString() const
{
    ASSERT(m_bufferData.hasSpecialImpl);
#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
    return reinterpret_cast<String*>(ThreadLocal::cageBase() + m_bufferData.compressedStringData[0]);
#else
    return m_bufferData.bufferAsString;
#endif
}

ALWAYS_INLINE void StringView::initBufferAccessData(String* str, size_t start, size_t end)
{
    ASSERT(start <= end && end <= str->length());
    // ParserStringView stores a direct buffer and does not have a parent.
    while (str->hasSpecialImpl() && str->isStringView()) {
        auto* view = static_cast<StringView*>(str);
        start += view->start();
        end += view->start();
        str = view->underlyingString();
    }
#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
    ASSERT(reinterpret_cast<uintptr_t>(str) - ThreadLocal::cageBase() <= UINT32_MAX);
    ASSERT(start <= UINT32_MAX);
    m_bufferData.compressedStringData[0] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(str));
    m_bufferData.compressedStringData[1] = static_cast<uint32_t>(start);
#else
    m_bufferData.bufferAsString = str;
    m_start = start;
#endif
    m_bufferData.hasSpecialImpl = true;
    m_bufferData.has8BitContent = str->has8BitContent();
    m_bufferData.length = end - start;
}

#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
static_assert(sizeof(StringView) == sizeof(String), "StringView metadata must fit in the buffer slot");
#endif
} // namespace Escargot

#endif
