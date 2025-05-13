/*
 * Copyright (c) 2024-present Samsung Electronics Co., Ltd
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#pragma once

#include "runtime/String.h"

namespace WTF {

typedef unsigned char LChar;

enum TextCaseSensitivity {
    TextCaseSensitive,
    TextCaseInsensitive
};

class String {
public:
    String()
        : m_impl()
        , m_implBufferAccessData(Escargot::String::emptyString()->bufferAccessData())
    {
    }

    String(::Escargot::String* impl)
        : m_impl(impl)
        , m_implBufferAccessData(impl->bufferAccessData())
    {
    }

    ALWAYS_INLINE char16_t operator[](const size_t idx) const
    {
        if (isNull()) {
            return 0;
        }
        return m_implBufferAccessData.charAt(idx);
    }

    ALWAYS_INLINE size_t length() const
    {
        if (isNull()) {
            return 0;
        }
        return m_implBufferAccessData.length;
    }

    bool equals(const String& src) const
    {
        if (isNull() && src.isNull()) {
            return true;
        }
        if (isNull() || src.isNull()) {
            return false;
        }
        return m_impl.value()->equals(src.m_impl.value());
    }

    ALWAYS_INLINE size_t hash() const
    {
        if (isNull()) {
            return 0;
        }
        if (m_implBufferAccessData.has8BitContent) {
            return StringHasher::computeHashAndMaskTop8Bits(m_implBufferAccessData.bufferAs8Bit, m_implBufferAccessData.length);
        } else {
            return StringHasher::computeHashAndMaskTop8Bits(m_implBufferAccessData.bufferAs16Bit, m_implBufferAccessData.length);
        }
    }

    bool contains(char c) const
    {
        if (isNull()) {
            return false;
        }

        for (size_t i = 0; i < m_implBufferAccessData.length; i ++) {
            if (m_implBufferAccessData.charAt(i) == c) {
                return true;
            }
        }
        return false;
    }

    bool isNull() const
    {
        return !m_impl;
    }

    ALWAYS_INLINE bool is8Bit() const
    {
        if (isNull()) {
            return true;
        }
        return m_implBufferAccessData.has8BitContent;
    }

    template <typename Any>
    const Any* characters() const
    {
        if (isNull()) {
            return nullptr;
        }
        if (is8Bit()) {
            return (Any*)m_implBufferAccessData.bufferAs8Bit;
        } else {
            return (Any*)m_implBufferAccessData.bufferAs16Bit;
        }
    }

    ALWAYS_INLINE const LChar* characters8() const
    {
        if (isNull()) {
            return nullptr;
        }
        return reinterpret_cast<const LChar*>(m_implBufferAccessData.bufferAs8Bit);
    }

    ALWAYS_INLINE const char16_t* characters16() const
    {
        if (isNull()) {
            return nullptr;
        }
        return m_implBufferAccessData.bufferAs16Bit;
    }

    ALWAYS_INLINE::Escargot::String* impl()
    {
        return m_impl.value();
    }

    template <const size_t srcLen>
    ALWAYS_INLINE bool operator==(const char (&src)[srcLen]) const
    {
        if (isNull()) {
            return !srcLen;
        }
        return m_impl->equals(src);
    }

private:
    Optional<::Escargot::String*> m_impl;
    ::Escargot::StringBufferAccessData m_implBufferAccessData;
};

using StringView = const String&;

class StringBuilder {
public:
    void append(int c)
    {
        char16_t buf[2];
        auto cnt = ::Escargot::utf32ToUtf16((char32_t)c, buf);
        m_impl.appendChar(buf[0]);
        if (cnt == 2) {
            m_impl.appendChar(buf[1]);
        }
    }

    String toString()
    {
        return m_impl.finalize();
    }

    void clear()
    {
        m_impl.clear();
    }

private:
    ::Escargot::StringBuilder m_impl;
};

} // namespace WTF

using WTF::LChar;
using WTF::String;
using WTF::StringView;
using WTF::StringBuilder;
