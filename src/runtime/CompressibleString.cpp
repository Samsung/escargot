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

#if defined(ENABLE_COMPRESSIBLE_STRING)

#include "Escargot.h"
#include "CompressibleString.h"
#include "runtime/Context.h"
#include "runtime/VMInstance.h"
#include "lz4.h"

namespace Escargot {

void* CompressibleString::operator new(size_t size)
{
    static bool typeInited = false;
    static GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(CompressibleString)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(CompressibleString, m_context));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(CompressibleString));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

CompressibleString::CompressibleString(Context* context)
    : String()
    , m_isOwnerMayFreed(false)
    , m_isCompressed(false)
    , m_context(context)
    , m_lastUsedTickcount(fastTickCount())
{
    m_bufferData.hasSpecialImpl = true;

    auto& v = context->vmInstance()->compressibleStrings();
    v.push_back(this);
    GC_REGISTER_FINALIZER_NO_ORDER(this, [](void* obj, void*) {
        CompressibleString* self = (CompressibleString*)obj;
        if (self->isCompressed()) {
            self->m_compressedData.~CompressedDataVector();
        } else {
            deallocateStringDataBuffer(const_cast<void*>(self->m_bufferData.buffer));
        }

        if (!self->m_isOwnerMayFreed) {
            self->m_context->vmInstance()->compressibleStringsUncomressedBufferSize() -= self->decomressedBufferSize();

            auto& v = self->m_context->vmInstance()->compressibleStrings();
            v.erase(std::find(v.begin(), v.end(), self));
        }
    },
                                   nullptr, nullptr, nullptr);
}

CompressibleString::CompressibleString(Context* context, const char* str, size_t len)
    : CompressibleString(context)
{
    char* buf = (char*)allocateStringDataBuffer(sizeof(char) * len);
    memcpy(buf, str, len);
    initBufferAccessData(buf, len, true);
}

CompressibleString::CompressibleString(Context* context, const LChar* str, size_t len)
    : CompressibleString(context)
{
    char* buf = (char*)allocateStringDataBuffer(sizeof(char) * len);
    memcpy(buf, str, len);
    initBufferAccessData(buf, len, true);
}

CompressibleString::CompressibleString(Context* context, const char16_t* str, size_t len)
    : CompressibleString(context)
{
    char* buf = (char*)allocateStringDataBuffer(sizeof(char) * len * 2);
    memcpy(buf, str, len * 2);
    initBufferAccessData(buf, len, false);
}

CompressibleString::CompressibleString(Context* context, void* buffer, size_t stringLength, bool is8bit)
    : CompressibleString(context)
{
    initBufferAccessData(buffer, stringLength, is8bit);
}

void CompressibleString::initBufferAccessData(void* data, size_t len, bool is8bit)
{
    m_bufferData.has8BitContent = is8bit;
    m_bufferData.length = len;
    m_bufferData.buffer = data;

    m_context->vmInstance()->compressibleStringsUncomressedBufferSize() += decomressedBufferSize();
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

void* CompressibleString::allocateStringDataBuffer(size_t byteLength)
{
    return malloc(byteLength);
}

void CompressibleString::deallocateStringDataBuffer(void* ptr)
{
    free(ptr);
}

bool CompressibleString::compress()
{
    ASSERT(!m_isCompressed);
    if (UNLIKELY(!m_bufferData.length)) {
        return false;
    }

    bool has8Bit = m_bufferData.has8BitContent;
    if (has8Bit) {
        return compressWorker<LChar>(currentStackPointer());
    } else {
        return compressWorker<char16_t>(currentStackPointer());
    }
}

void CompressibleString::decompress()
{
    ASSERT(m_isCompressed);
    ASSERT(m_bufferData.length);

    bool has8Bit = m_bufferData.has8BitContent;
    if (has8Bit) {
        decompressWorker<LChar>();
    } else {
        decompressWorker<char16_t>();
    }
}

constexpr static const size_t g_compressChunkSize = 1044465;
static_assert(LZ4_COMPRESSBOUND(g_compressChunkSize) == 1024 * 1024, "");

template <typename StringType>
bool CompressibleString::compressWorker(void* callerSP)
{
    ASSERT(!m_isCompressed);
    ASSERT(m_bufferData.length > 0);

#if defined(STACK_GROWS_DOWN)
    size_t* start = (size_t*)((size_t)callerSP & ~(sizeof(size_t) - 1));
    size_t* end = (size_t*)m_context->vmInstance()->stackStartAddress();
#else
    size_t* start = (size_t*)m_context->vmInstance()->stackStartAddress();
    size_t* end = (size_t*)((size_t)callerSP & ~(sizeof(size_t) - 1));
#endif

    while (start != end) {
        if (UNLIKELY(*start == (size_t)m_bufferData.buffer)) {
            // if there is reference on stack, we cannot compress string.
            return false;
        }
        start++;
    }

    size_t originByteLength = m_bufferData.length * sizeof(StringType);
    int lastBoundLength = 0;
    std::unique_ptr<char[]> compBuffer;
    for (size_t srcIndex = 0; srcIndex < originByteLength; srcIndex += g_compressChunkSize) {
        int srcSize = (int)std::min(g_compressChunkSize, originByteLength - srcIndex);
        int boundLength = LZ4::LZ4_compressBound(srcSize);
        if (boundLength > lastBoundLength) {
            compBuffer.reset(new char[boundLength]);
            lastBoundLength = boundLength;
        }

        int compressedLength = LZ4::LZ4_compress_default(m_bufferData.bufferAs8Bit + srcIndex, (char*)compBuffer.get(), srcSize, boundLength);
        if (!compressedLength) {
            // compression fail
            return false;
        }

        ASSERT(compressedLength > 0);
        m_compressedData.push_back(std::vector<char>(compBuffer.get(), compBuffer.get() + compressedLength));
    }

    m_context->vmInstance()->compressibleStringsUncomressedBufferSize() -= decomressedBufferSize();

    // immediately free the original string after compression when there is no reference on stack
    deallocateStringDataBuffer(const_cast<void*>(m_bufferData.buffer));

    m_bufferData.bufferAs8Bit = nullptr;
    m_isCompressed = true;
    return true;
}


template <typename StringType>
void CompressibleString::decompressWorker()
{
    ASSERT(m_isCompressed);

    size_t originByteLength = m_bufferData.length * sizeof(StringType);

    char* dstBuffer = (char*)allocateStringDataBuffer(originByteLength);
    int dstIndex = 0;

    for (size_t srcIndex = 0, bufIndex = 0; srcIndex < originByteLength; srcIndex += g_compressChunkSize, bufIndex++) {
        int srcSize = (int)std::min(g_compressChunkSize, originByteLength - srcIndex);

        int decompressedLength = LZ4::LZ4_decompress_safe(m_compressedData[bufIndex].data(), dstBuffer + dstIndex, m_compressedData[bufIndex].size(), srcSize);
        if (!decompressedLength) {
            // decompress fail
            RELEASE_ASSERT_NOT_REACHED();
        }

        dstIndex += srcSize;
    }

    CompressedDataVector().swap(m_compressedData);

    m_bufferData.bufferAs8Bit = const_cast<const char*>(dstBuffer);
    m_isCompressed = false;

    m_context->vmInstance()->compressibleStringsUncomressedBufferSize() += decomressedBufferSize();
}
}

#endif // ENABLE_COMPRESSIBLE_STRING
