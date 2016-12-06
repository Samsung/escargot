#include "Escargot.h"
#include "AtomicString.h"
#include "Context.h"

namespace Escargot {

AtomicString::AtomicString(ExecutionState& ec, const char16_t* src, size_t len)
{
    init(&ec.context()->m_atomicStringMap, src, len);
}

AtomicString::AtomicString(ExecutionState& ec, const char* src, size_t len)
{
    init(&ec.context()->m_atomicStringMap, src, len);
}

AtomicString::AtomicString(Context* c, const char16_t* src, size_t len)
{
    init(&c->m_atomicStringMap, src, len);
}

AtomicString::AtomicString(Context* c, const char* src, size_t len)
{
    init(&c->m_atomicStringMap, src, len);
}

AtomicString::AtomicString(ExecutionState& ec, String* name)
{
    if (name->isASCIIString()) {
        init(&ec.context()->m_atomicStringMap, name->asASCIIStringData(), name->length());
    } else {
        UTF8StringData data = name->toUTF8StringData();
        init(&ec.context()->m_atomicStringMap, data.data(), data.length());
    }
}

AtomicString::AtomicString(Context* c, const StringView& sv)
{
    UTF16StringData data = sv.toUTF16StringData();
    init(&c->m_atomicStringMap, data.data(), data.length());
}

AtomicString::AtomicString(Context* c, String* name)
{
    if (name->isASCIIString()) {
        init(&c->m_atomicStringMap, name->asASCIIStringData(), name->length());
    } else {
        UTF8StringData data = name->toUTF8StringData();
        init(&c->m_atomicStringMap, data.data(), data.length());
    }
}

void AtomicString::init(AtomicStringMap* ec, const char* src, size_t len)
{
    if (!len) {
        m_string = String::emptyString;
        return;
    }

    auto iter = ec->find(std::make_pair(src, len));
    if (iter == ec->end()) {
        ASCIIStringData s(src, len);
        String* newData = new ASCIIString(std::move(s));
        m_string = newData;
        ec->insert(std::make_pair(std::make_pair(newData->asASCIIStringData(), len), newData));
    } else {
        m_string = iter->second;
    }
}

void AtomicString::init(AtomicStringMap* ec, const char16_t* src, size_t len)
{
    if (!len) {
        m_string = String::emptyString;
        return;
    }

    if (isAllASCII(src, len)) {
        char* abuf = ALLOCA(len, char, ec);
        for (unsigned i = 0 ; i < len ; i ++) {
            abuf[i] = src[i];
        }
        init(ec, abuf, len);
        return;
    }
    UTF8StringData buf = utf16StringToUTF8String(src, len);
    auto iter = ec->find(std::make_pair(buf.data(), buf.size()));
    if (iter == ec->end()) {
        UTF16StringData s(src, len);
        String* newData = new UTF16String(std::move(s));
        m_string = newData;
        char* ptr = gc_malloc_atomic_ignore_off_page_allocator<char>().allocate(buf.size());
        memcpy(ptr, buf.data(), buf.size());
        ec->insert(std::make_pair(std::make_pair(ptr, buf.size()), newData));
    } else {
        m_string = iter->second;
    }
}

}
