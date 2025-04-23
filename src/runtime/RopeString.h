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

#ifndef __EscargotRopeString__
#define __EscargotRopeString__

#include "runtime/String.h"
#include "util/Optional.h"

namespace Escargot {

class ExecutionState;

class RopeString : public String {
    RopeString()
        : String()
    {
        m_left = String::emptyString;
        m_bufferData.has8BitContent = true;
        m_bufferData.hasSpecialImpl = true;
        m_bufferData.length = 0;
        m_bufferData.buffer = nullptr;
    }

public:
    // this function not always create RopeString.
    // if (l+r).length() < ROPE_STRING_MIN_LENGTH
    // then create just normalString
    // provide ExecutionState if you need limit of string length(exception can be thrown only in ExecutionState area)
    static String* createRopeString(String* lstr, String* rstr, Optional<ExecutionState*> state = nullptr);

    virtual UTF16StringData toUTF16StringData() const override;
    virtual UTF8StringData toUTF8StringData() const override;
    virtual UTF8StringDataNonGCStd toNonGCUTF8StringData(int options = StringWriteOption::NoOptions) const override;

    virtual bool isRopeString() override
    {
        return true;
    }

    virtual const LChar* characters8() const override
    {
        return (const LChar*)bufferAccessData().buffer;
    }

    virtual const char16_t* characters16() const override
    {
        return (const char16_t*)bufferAccessData().buffer;
    }

    bool wasFlattened() const
    {
        return !m_bufferData.hasSpecialImpl;
    }

    String* left() const
    {
        ASSERT(!wasFlattened());
        return m_left;
    }

    String* right() const
    {
        ASSERT(!wasFlattened());
        return (String*)m_bufferData.buffer;
    }

    virtual char16_t charAt(const size_t idx) const override;

    void* operator new(size_t size, bool is8Bit);
    void* operator new[](size_t size) = delete;

protected:
    virtual StringBufferAccessData bufferAccessDataSpecialImpl() override
    {
        ASSERT(m_bufferData.hasSpecialImpl);
        flattenRopeString();
        ASSERT(!m_bufferData.hasSpecialImpl);

        return m_bufferData;
    }

    template <typename ResultType>
    void flattenRopeStringWorker();
    void flattenRopeString();

private:
    String* m_left;
    // String* m_right; // Right String is stored in m_bufferAccessData.buffer if string is not flattened
};
} // namespace Escargot

#endif
