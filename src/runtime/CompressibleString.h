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

#ifndef __EscargotCompressibleString__
#define __EscargotCompressibleString__

#if defined(ENABLE_COMPRESSIBLE_STRING)

#include "runtime/String.h"

namespace Escargot {

class VMInstance;

class CompressibleString : public String {
    friend class VMInstance;

public:
    // 8bit string constructor
    CompressibleString(VMInstance* instance, const char* str, size_t len);
    CompressibleString(VMInstance* instance, const LChar* str, size_t len);

    // 16bit string constructor
    CompressibleString(VMInstance* instance, const char16_t* str, size_t len);

    // from already allocated buffer
    CompressibleString(VMInstance* instance, void* buffer, size_t stringLength, bool is8bit);

    virtual bool isCompressibleString() override
    {
        return true;
    }

    virtual UTF16StringData toUTF16StringData() const override;
    virtual UTF8StringData toUTF8StringData() const override;
    virtual UTF8StringDataNonGCStd toNonGCUTF8StringData(int options = StringWriteOption::NoOptions) const override;

    virtual const LChar* characters8() const override
    {
        return (const LChar*)bufferAccessData().buffer;
    }

    virtual const char16_t* characters16() const override
    {
        return (const char16_t*)bufferAccessData().buffer;
    }

    virtual StringBufferAccessData bufferAccessDataSpecialImpl() override
    {
        m_lastUsedTickcount = fastTickCount();
        if (isCompressed()) {
            decompress();
        }

        // add refCount pointer to count its usage in StringBufferAccessData
        return StringBufferAccessData(m_bufferData.has8BitContent, m_bufferData.length, const_cast<void*>(m_bufferData.buffer), &m_refCount);
    }

    bool isCompressed()
    {
        return m_isCompressed;
    }

    size_t refCount() const
    {
        return m_refCount;
    }

    void* operator new(size_t);
    void* operator new[](size_t) = delete;
    void operator delete[](void*) = delete;

    static void* allocateStringDataBuffer(size_t byteLength);
    static void deallocateStringDataBuffer(void* ptr, size_t byteLength);

    bool compress();
    void decompress();

private:
    CompressibleString(VMInstance* instance);

    void initBufferAccessData(void* data, size_t len, bool is8bit);

    size_t decomressedBufferSize()
    {
        if (isCompressed()) {
            return 0;
        } else {
            return m_bufferData.length * (m_bufferData.has8BitContent ? 1 : 2);
        }
    }

    template <typename StringType>
    NEVER_INLINE bool compressWorker(void* callerSP);
    template <typename StringType>
    NEVER_INLINE void decompressWorker();

    bool m_isOwnerMayFreed;
    bool m_isCompressed;
    size_t m_refCount; // reference count representing the usage of this CompressibleString
    VMInstance* m_vmInstance;
    uint64_t m_lastUsedTickcount;
    typedef std::vector<std::vector<char>> CompressedDataVector;
    CompressedDataVector m_compressedData;
};
} // namespace Escargot

#endif // ENABLE_COMPRESSIBLE_STRING

#endif
