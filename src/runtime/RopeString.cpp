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

#include "Escargot.h"
#include "RopeString.h"
#include "StringBuilder.h"
#include "ErrorObject.h"

namespace Escargot {

void* RopeString::operator new(size_t size)
{
    static bool typeInited = false;
    static GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(RopeString)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(RopeString, m_left));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(RopeString, m_bufferData.buffer));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(RopeString));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

String* RopeString::createRopeString(String* lstr, String* rstr, ExecutionState* state)
{
    size_t llen = lstr->length();
    if (llen == 0) {
        return rstr;
    }

    size_t rlen = rstr->length();
    if (rlen == 0) {
        return lstr;
    }

    if (llen + rlen < ROPE_STRING_MIN_LENGTH) {
        const auto& lData = lstr->bufferAccessData();
        const auto& rData = rstr->bufferAccessData();
        if (LIKELY(lData.has8BitContent && rData.has8BitContent)) {
            Latin1StringData ret;
            size_t len = lData.length + rData.length;
            ret.resizeWithUninitializedValues(len);

            LChar* result = ret.data();
            const LChar* buffer = (const LChar*)lData.buffer;
            for (size_t i = 0; i < lData.length; i++) {
                result[i] = buffer[i];
            }

            result += lData.length;
            buffer = (const LChar*)rData.buffer;
            for (size_t i = 0; i < rData.length; i++) {
                result[i] = buffer[i];
            }
            return new Latin1String(std::move(ret));
        } else {
            StringBuilder builder;
            builder.appendString(lstr);
            builder.appendString(rstr);
            return builder.finalize();
        }
    }

    if (state && UNLIKELY((llen + rlen) > STRING_MAXIMUM_LENGTH)) {
        ErrorObject::throwBuiltinError(*state, ErrorObject::RangeError, ErrorObject::Messages::String_InvalidStringLength);
    }

    RopeString* rope = new RopeString();
    rope->m_bufferData.length = llen + rlen;
    rope->m_left = lstr;
    rope->m_bufferData.buffer = rstr;

    bool l8bit;
    if (lstr->isRopeString()) {
        l8bit = ((RopeString*)lstr)->m_bufferData.has8BitContent;
    } else {
        l8bit = lstr->has8BitContent();
    }

    bool r8bit;
    if (rstr->isRopeString()) {
        r8bit = ((RopeString*)rstr)->m_bufferData.has8BitContent;
    } else {
        r8bit = rstr->has8BitContent();
    }

    rope->m_bufferData.has8BitContent = l8bit & r8bit;
    return rope;
}

template <typename ResultType>
void RopeString::flattenRopeStringWorker()
{
    ResultType* result = (ResultType*)GC_MALLOC_ATOMIC(sizeof(ResultType) * m_bufferData.length);
    std::vector<String*> queue;
    queue.push_back(left());
    queue.push_back(right());
    size_t pos = m_bufferData.length;
    while (!queue.empty()) {
        String* cur = queue.back();
        queue.pop_back();
        if (cur->isRopeString()) {
            RopeString* cur2 = (RopeString*)cur;
            if (cur2->m_bufferData.hasSpecialImpl) {
                queue.push_back(cur2->left());
                queue.push_back(cur2->right());
                continue;
            }
        }
        String* sub = cur;
        const auto& data = sub->bufferAccessData();

        pos -= data.length;
        size_t subLength = data.length;

        if (data.has8BitContent) {
            auto ptr = (const LChar*)data.buffer;
            for (size_t i = 0; i < subLength; i++) {
                result[i + pos] = ptr[i];
            }
        } else {
            auto ptr = (const char16_t*)data.buffer;
            for (size_t i = 0; i < subLength; i++) {
                result[i + pos] = ptr[i];
            }
        }
    }

    m_bufferData.hasSpecialImpl = false;
    m_bufferData.buffer = result;

    m_left = nullptr;
}

void RopeString::flattenRopeString()
{
    ASSERT(m_left);
    if (m_bufferData.has8BitContent) {
        flattenRopeStringWorker<LChar>();
    } else {
        flattenRopeStringWorker<char16_t>();
    }
}

UTF8StringDataNonGCStd RopeString::toNonGCUTF8StringData(int options) const
{
    return bufferAccessData().toUTF8String<UTF8StringDataNonGCStd>();
}

UTF8StringData RopeString::toUTF8StringData() const
{
    return bufferAccessData().toUTF8String<UTF8StringData>();
}

UTF16StringData RopeString::toUTF16StringData() const
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
} // namespace Escargot
