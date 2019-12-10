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
        m_bufferAccessData.has8BitContent = true;
        m_bufferAccessData.hasSpecialImpl = true;
        m_bufferAccessData.length = 0;
        m_bufferAccessData.buffer = nullptr;
    }

    // this function not always create RopeString.
    // if (l+r).length() < ROPE_STRING_MIN_LENGTH
    // then create just normalString
    // provide ExecutionState if you need limit of string length(exception can be thrown only in ExecutionState area)
    static String* createRopeString(String* lstr, String* rstr, ExecutionState* state = nullptr);

    virtual char16_t charAt(const size_t idx) const
    {
        return bufferAccessData().charAt(idx);
    }
    virtual UTF16StringData toUTF16StringData() const;
    virtual UTF8StringData toUTF8StringData() const;
    virtual UTF8StringDataNonGCStd toNonGCUTF8StringData() const;

    virtual bool isRopeString()
    {
        return true;
    }

    virtual const LChar* characters8() const
    {
        return (const LChar*)bufferAccessData().buffer;
    }

    virtual const char16_t* characters16() const
    {
        return (const char16_t*)bufferAccessData().buffer;
    }

    virtual void bufferAccessDataSpecialImpl()
    {
        ASSERT(m_bufferAccessData.hasSpecialImpl);
        flattenRopeString();
        ASSERT(!m_bufferAccessData.hasSpecialImpl);
    }

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

protected:
    template <typename ResultType>
    void flattenRopeStringWorker();
    void flattenRopeString();

private:
    String* m_left;
    // String* m_right; // Right String is stored in m_bufferAccessData.buffer if string is not flattened
};
}

#endif
