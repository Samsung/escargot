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
#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
    friend int getValidValueInRopeString(void*, GC_mark_pair*);
#endif
    RopeString()
        : String()
    {
#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
        setChildren(String::emptyString(), nullptr);
#else
        m_left = String::emptyString();
#endif
        m_bufferData.has8BitContent = true;
        m_bufferData.hasSpecialImpl = true;
        m_bufferData.length = 0;
#if !defined(ESCARGOT_USE_32BIT_IN_64BIT)
        m_bufferData.buffer = nullptr;
#endif
    }

public:
    // this function not always create RopeString.
    // if (l+r).length() < LATIN1_LARGE_INLINE_BUFFER_MAX_SIZE
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

    bool wasFlattened() const
    {
        return !m_bufferData.hasSpecialImpl;
    }

    ALWAYS_INLINE String* left() const;
    ALWAYS_INLINE String* right() const;

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
    void setChildren(String* left, String* right)
    {
#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
        ASSERT(!left || reinterpret_cast<uintptr_t>(left) - ThreadLocal::cageBase() <= UINT32_MAX);
        ASSERT(!right || reinterpret_cast<uintptr_t>(right) - ThreadLocal::cageBase() <= UINT32_MAX);
        m_bufferData.compressedStringData[0] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(left));
        m_bufferData.compressedStringData[1] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(right));
#else
        m_left = left;
        m_bufferData.buffer = right;
#endif
    }
#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
    static String* decodeChild(uint32_t offset)
    {
        return offset ? reinterpret_cast<String*>(ThreadLocal::cageBase() + offset) : nullptr;
    }
#else
    String* m_left;
#endif
    // String* m_right; // Right String is stored in m_bufferAccessData.buffer if string is not flattened
};

ALWAYS_INLINE String* RopeString::left() const
{
    ASSERT(!wasFlattened());
#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
    return decodeChild(m_bufferData.compressedStringData[0]);
#else
    return m_left;
#endif
}

ALWAYS_INLINE String* RopeString::right() const
{
    ASSERT(!wasFlattened());
#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
    return decodeChild(m_bufferData.compressedStringData[1]);
#else
    return (String*)m_bufferData.buffer;
#endif
}

#if defined(ESCARGOT_USE_32BIT_IN_64BIT)
static_assert(sizeof(RopeString) == sizeof(String), "Rope children must fit in the buffer slot");
#endif
} // namespace Escargot

#endif
