#include "Escargot.h"
#include "RopeString.h"
#include "StringBuilder.h"

namespace Escargot {

String* RopeString::createRopeString(String* lstr, String* rstr)
{
    size_t llen = lstr->length();
    size_t rlen = rstr->length();

    if (llen + rlen < ESCARGOT_ROPE_STRING_MIN_LENGTH) {
        auto lData = lstr->bufferAccessData();
        auto rData = rstr->bufferAccessData();
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
    // TODO
    // if (static_cast<int64_t>(llen) > static_cast<int64_t>(ESString::maxLength() - rlen))
    //     ESVMInstance::currentInstance()->throwOOMError();

    RopeString* rope = new RopeString();
    rope->m_contentLength = llen + rlen;
    rope->m_left = lstr;
    rope->m_right = rstr;

    return rope;
}

template <typename A, typename B>
void RopeString::flattenRopeStringWorker()
{
    A result;
    result.resizeWithUninitializedValues(length());
    std::vector<String*, gc_malloc_atomic_ignore_off_page_allocator<String*>> queue;
    queue.push_back(m_left);
    queue.push_back(m_right);
    size_t pos = result.size();
    size_t k = 0;
    while (!queue.empty()) {
        String* cur = queue.back();
        queue.pop_back();
        if (cur->isRopeString()) {
            RopeString* cur2 = (RopeString*)cur;
            if (cur2->m_right != nullptr) {
                ASSERT(cur2->m_left);
                ASSERT(cur2->m_right);
                queue.push_back(cur2->m_left);
                queue.push_back(cur2->m_right);
                continue;
            }
        }
        String* sub = cur;
        auto data = sub->bufferAccessData();

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
    m_left = new B(std::move(result));
    m_right = nullptr;
}

void RopeString::flattenRopeString()
{
    if (m_right) {
        if (has8BitContent()) {
            flattenRopeStringWorker<Latin1StringData, Latin1String>();
        } else {
            flattenRopeStringWorker<UTF16StringData, UTF16String>();
        }
    }
}
}
