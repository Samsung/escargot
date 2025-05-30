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

void* RopeString::operator new(size_t size, bool is8Bit)
{
    if (is8Bit) {
        // if 8-bit string, we don't needs typed malloc
        return GC_MALLOC(size);
    }
    static MAY_THREAD_LOCAL bool typeInited = false;
    static MAY_THREAD_LOCAL GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(RopeString)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(RopeString, m_left));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(RopeString, m_bufferData.buffer));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(RopeString));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

String* RopeString::createRopeString(String* lstr, String* rstr, Optional<ExecutionState*> state)
{
    size_t llen = lstr->length();
    if (llen == 0) {
        return rstr;
    }

    size_t rlen = rstr->length();
    if (rlen == 0) {
        return lstr;
    }

    if (llen + rlen <= LATIN1_LARGE_INLINE_BUFFER_MAX_SIZE) {
        const auto& lData = lstr->bufferAccessData();
        const auto& rData = rstr->bufferAccessData();
        if (LIKELY(lData.has8BitContent && rData.has8BitContent)) {
            size_t len = lData.length + rData.length;
            LChar* result = reinterpret_cast<LChar*>(alloca(len));
            memcpy(result, lData.buffer, lData.length);
            memcpy(result + lData.length, rData.buffer, rData.length);
            return String::fromLatin1(result, len);
        } else {
            StringBuilder builder;
            builder.appendString(lstr, state);
            builder.appendString(rstr, state);
            return builder.finalize();
        }
    }

    if (state && UNLIKELY((llen + rlen) > STRING_MAXIMUM_LENGTH)) {
        ErrorObject::throwBuiltinError(*state.value(), ErrorCode::RangeError, ErrorObject::Messages::String_InvalidStringLength);
    }

    bool l8bit = lstr->has8BitContent();
    bool r8bit = rstr->has8BitContent();
    bool result8Bit = l8bit & r8bit;
    RopeString* rope = new (result8Bit) RopeString();
    rope->m_bufferData.length = llen + rlen;
    rope->m_left = lstr;
    rope->m_bufferData.buffer = rstr;
    rope->m_bufferData.has8BitContent = result8Bit;
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

static MAY_THREAD_LOCAL const RopeString* g_lastUsedString;
static MAY_THREAD_LOCAL bool g_headOfCharAt = true;
class RopeStringUsageChecker {
public:
    RopeStringUsageChecker()
    {
        m_headOfCharAt = g_headOfCharAt;
        g_headOfCharAt = false;
    }

    ~RopeStringUsageChecker()
    {
        g_headOfCharAt = m_headOfCharAt;
    }

    bool isOftenUsed(const RopeString* rs)
    {
        auto old = g_lastUsedString;
        if (m_headOfCharAt) {
            g_lastUsedString = rs;
        }
        return rs == old;
    }

private:
    bool m_headOfCharAt;
};

// worker for preventing too many recursive calls with RopeString
static char16_t charAtWorker(size_t idx, const RopeString* self)
{
    while (true) {
        size_t leftLength = self->left()->length();
        if (idx < leftLength) {
            if (self->left()->isRopeString() && !self->left()->asRopeString()->wasFlattened()) {
                self = self->left()->asRopeString();
                continue;
            }
            return self->left()->charAt(idx);
        } else {
            if (self->right()->isRopeString() && !self->right()->asRopeString()->wasFlattened()) {
                self = self->right()->asRopeString();
                idx = idx - leftLength;
                continue;
            }
            return self->right()->charAt(idx - leftLength);
        }
    }
}

char16_t RopeString::charAt(const size_t idx) const
{
    if (wasFlattened()) {
        return bufferAccessData().charAt(idx);
    }

    RopeStringUsageChecker checker;

    if (checker.isOftenUsed(this)) {
        return bufferAccessData().charAt(idx);
    }

    return charAtWorker(idx, this);
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
