/*
 * Copyright (c) 2019-present Samsung Electronics Co., Ltd
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

#ifndef __EscargotCompressibleString__
#define __EscargotCompressibleString__

#if defined(ENABLE_SOURCE_COMPRESSION)

#include "runtime/String.h"

namespace Escargot {

class CompressibleString : public String {
public:
    CompressibleString()
        : String()
        , m_compressedLength(0)
    {
    }

    // 8bit string constructor

    explicit CompressibleString(Latin1StringData&& src)
        : String()
        , m_compressedLength(0)
    {
        Latin1StringData data = std::move(src);
        initBufferAccessData(data);
    }

    explicit CompressibleString(const char* str)
        : String()
        , m_compressedLength(0)
    {
        Latin1StringData data;
        data.append((const LChar*)str, strlen(str));
        initBufferAccessData(data);
    }

    CompressibleString(const char* str, size_t len)
        : String()
        , m_compressedLength(0)
    {
        Latin1StringData data;
        data.append((const LChar*)str, len);
        initBufferAccessData(data);
    }

    CompressibleString(const LChar* str, size_t len)
        : String()
        , m_compressedLength(0)
    {
        Latin1StringData data;
        data.append(str, len);
        initBufferAccessData(data);
    }

    // 16bit string constructor

    explicit CompressibleString(UTF16StringData&& src)
        : String()
        , m_compressedLength(0)
    {
        UTF16StringData data = std::move(src);
        initBufferAccessData(data);
    }

    CompressibleString(const char16_t* str, size_t len)
        : String()
        , m_compressedLength(0)
    {
        UTF16StringData data;
        data.append(str, len);
        initBufferAccessData(data);
    }

    virtual bool isCompressibleString()
    {
        return true;
    }

    virtual char16_t charAt(const size_t idx) const
    {
        return bufferAccessData().charAt(idx);
    }
    virtual UTF16StringData toUTF16StringData() const;
    virtual UTF8StringData toUTF8StringData() const;
    virtual UTF8StringDataNonGCStd toNonGCUTF8StringData() const;

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
        decompress();
        ASSERT(!m_bufferAccessData.hasSpecialImpl);
    }

    bool isCompressed()
    {
        // m_bufferAccessData.hasSpecialImpl represents compression status
        return m_bufferAccessData.hasSpecialImpl;
    }

    void* operator new(size_t);
    void* operator new[](size_t) = delete;
    void operator delete[](void*) = delete;

    bool compress();
    bool decompress();

private:
    void initBufferAccessData(Latin1StringData& stringData)
    {
        m_bufferAccessData.has8BitContent = true;
        m_bufferAccessData.length = stringData.length();
        m_bufferAccessData.buffer = stringData.takeBuffer();
    }

    void initBufferAccessData(UTF16StringData& stringData)
    {
        m_bufferAccessData.has8BitContent = false;
        m_bufferAccessData.length = stringData.length();
        m_bufferAccessData.buffer = stringData.takeBuffer();
    }

    template <typename StringType>
    bool compressWorker();
    template <typename StringType>
    bool decompressWorker();

    int m_compressedLength; // compressed length in byte
};
}

#endif // ENABLE_SOURCE_COMPRESSION

#endif
