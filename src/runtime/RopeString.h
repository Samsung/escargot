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
        m_bufferAccessData.hasSpecialImpl = true;
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

    virtual UTF8StringDataNonGCStd toNonGCUTF8StringData() const;

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

    virtual void bufferAccessDataSpecialImpl()
    {
        m_bufferAccessData = normalString()->bufferAccessData();
    }

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

protected:
    String* normalString() const
    {
        if (m_right)
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
}

#endif
