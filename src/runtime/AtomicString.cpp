#include "Escargot.h"
#include "AtomicString.h"
#include "Context.h"

namespace Escargot {

AtomicString::AtomicString(ExecutionState& ec, const char16_t* src, size_t len)
{
    if (isAllASCII(src, len)) {
        init(&ec.context()->m_atomicStringMap, new ASCIIString(src, len));
    } else {
        init(&ec.context()->m_atomicStringMap, new UTF16String(src, len));
    }
}

AtomicString::AtomicString(ExecutionState& ec, const char* src, size_t len)
{
    init(&ec.context()->m_atomicStringMap, new ASCIIString(src, len));
}

AtomicString::AtomicString(Context* c, const char16_t* src, size_t len)
{
    if (isAllASCII(src, len)) {
        init(&c->m_atomicStringMap, new ASCIIString(src, len));
    } else {
        init(&c->m_atomicStringMap, new UTF16String(src, len));
    }
}

AtomicString::AtomicString(Context* c, const char* src, size_t len)
{
    init(&c->m_atomicStringMap, new ASCIIString(src, len));
}

AtomicString::AtomicString(ExecutionState& ec, String* name)
{
    init(&ec.context()->m_atomicStringMap, name);
}

AtomicString::AtomicString(Context* c, const StringView& sv)
{
    if (sv.hasASCIIContent()) {
        init(&c->m_atomicStringMap, sv.characters8(), sv.length());
    } else {
        init(&c->m_atomicStringMap, sv.characters16(), sv.length());
    }
}

AtomicString::AtomicString(Context* c, String* name)
{
    init(&c->m_atomicStringMap, name);
}

void AtomicString::init(AtomicStringMap* ec, String* name)
{
    if (UNLIKELY(name->length() == 0)) {
        m_string = String::emptyString;
        return;
    }

    auto iter = ec->find(name);
    if (ec->end() == iter) {
        ec->insert(name);
        ASSERT(ec->find(name) != ec->end());
        m_string = name;
    } else {
        m_string = iter.operator*();
    }
}
}
