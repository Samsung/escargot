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

#if defined(ENABLE_SOURCE_COMPRESSION)

#include "Escargot.h"
#include "CompressibleString.h"
#include "lz4.h"

namespace Escargot {

void* CompressibleString::operator new(size_t size)
{
    static bool typeInited = false;
    static GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(CompressibleString)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(CompressibleString, m_bufferAccessData.buffer));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(CompressibleString));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

UTF8StringDataNonGCStd CompressibleString::toNonGCUTF8StringData() const
{
    return bufferAccessData().toUTF8String<UTF8StringDataNonGCStd>();
}

UTF8StringData CompressibleString::toUTF8StringData() const
{
    return bufferAccessData().toUTF8String<UTF8StringData>();
}

UTF16StringData CompressibleString::toUTF16StringData() const
{
    auto data = bufferAccessData();
    if (data.has8BitContent) {
        UTF16StringData ret;
        ret.resizeWithUninitializedValues(data.length);
        for (size_t i = 0; i < data.length; i++) {
            ret[i] = data.uncheckedCharAtFor8Bit(i);
        }
        return ret;
    } else {
        return UTF16StringData(data.bufferAs16Bit, data.length);
    }
}

bool CompressibleString::compress()
{
    ASSERT(!m_bufferAccessData.hasSpecialImpl);
    if (UNLIKELY(!m_bufferAccessData.length)) {
        return false;
    }

    bool has8Bit = m_bufferAccessData.has8BitContent;
    if (has8Bit) {
        return compressWorker<LChar>();
    } else {
        return compressWorker<char16_t>();
    }
}

bool CompressibleString::decompress()
{
    ASSERT(m_bufferAccessData.hasSpecialImpl);
    ASSERT(m_bufferAccessData.length);

    bool has8Bit = m_bufferAccessData.has8BitContent;
    if (has8Bit) {
        return decompressWorker<LChar>();
    } else {
        return decompressWorker<char16_t>();
    }
}

template <typename StringType>
bool CompressibleString::compressWorker()
{
    ASSERT(!m_bufferAccessData.hasSpecialImpl);
    ASSERT(m_bufferAccessData.length > 0);

    int originByteLength = m_bufferAccessData.length * sizeof(StringType);

    int boundLength = LZ4::LZ4_compressBound(originByteLength);
    char* compBuffer = new char[boundLength];

    int compressedLength = LZ4::LZ4_compress_default(m_bufferAccessData.bufferAs8Bit, compBuffer, originByteLength, boundLength);
    if (!compressedLength) {
        // compression fail
        return false;
    }

    ASSERT(compressedLength > 0);
    // immediately free the original string after compression
    GC_FREE(const_cast<void*>(m_bufferAccessData.buffer));

    char* data = (char*)GC_MALLOC_ATOMIC(compressedLength);
    memcpy(data, compBuffer, compressedLength);
    m_bufferAccessData.bufferAs8Bit = const_cast<const char*>(data);
    m_bufferAccessData.hasSpecialImpl = true;
    m_compressedLength = compressedLength;

    delete[] compBuffer;

    return true;
}


template <typename StringType>
bool CompressibleString::decompressWorker()
{
    ASSERT(m_bufferAccessData.hasSpecialImpl);

    int originByteLength = m_bufferAccessData.length * sizeof(StringType);
    int compressedLength = m_compressedLength;

    char* data = (char*)GC_MALLOC_ATOMIC(originByteLength);

    int decompressedLength = LZ4::LZ4_decompress_safe(m_bufferAccessData.bufferAs8Bit, data, compressedLength, originByteLength);
    if (!decompressedLength) {
        // decompress fail
        return false;
    }

    ASSERT(decompressedLength == originByteLength);
    // immediately free the decompressed string after decompression
    GC_FREE(const_cast<void*>(m_bufferAccessData.buffer));

    m_bufferAccessData.bufferAs8Bit = const_cast<const char*>(data);
    m_bufferAccessData.hasSpecialImpl = false;

    return true;
}
}

#endif // ENABLE_SOURCE_COMPRESSION
