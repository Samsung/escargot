/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include "Escargot.h"
#include "AtomicString.h"
#include "Context.h"

namespace Escargot {

AtomicString::AtomicString(ExecutionState& ec, const char16_t* src, size_t len)
{
    if (isAllASCII(src, len)) {
        init(ec.context()->m_atomicStringMap, new ASCIIString(src, len));
    } else {
        init(ec.context()->m_atomicStringMap, new UTF16String(src, len));
    }
}

AtomicString::AtomicString(ExecutionState& ec, const char* src, size_t len)
{
    init(ec.context()->m_atomicStringMap, new ASCIIString(src, len));
}

AtomicString::AtomicString(ExecutionState& ec, const char* src)
{
    init(ec.context()->m_atomicStringMap, new ASCIIString(src, strlen(src)));
}

AtomicString::AtomicString(Context* c, const char16_t* src, size_t len)
{
    if (isAllASCII(src, len)) {
        init(c->m_atomicStringMap, new ASCIIString(src, len));
    } else {
        init(c->m_atomicStringMap, new UTF16String(src, len));
    }
}

AtomicString::AtomicString(Context* c, const char* src, size_t len)
{
    init(c->m_atomicStringMap, new ASCIIString(src, len));
}

AtomicString::AtomicString(ExecutionState& ec, String* name)
{
    init(ec.context()->m_atomicStringMap, name);
}

AtomicString::AtomicString(Context* c, const StringView& sv)
{
    size_t v = sv.getTagInFirstDataArea();
    if (v > POINTER_VALUE_STRING_TAG_IN_DATA) {
        m_string = (String*)(v & ~POINTER_VALUE_STRING_TAG_IN_DATA);
        return;
    }

    AtomicStringMap* ec = c->atomicStringMap();
    StringView& str = const_cast<StringView&>(sv);
    String* name = &str;
    auto iter = ec->find(name);
    if (ec->end() == iter) {
        StringView* newSv = new StringView(sv);
        ec->insert(newSv);
        ASSERT(ec->find(newSv) != ec->end());
        m_string = newSv;
        str.m_tag = (size_t)POINTER_VALUE_STRING_TAG_IN_DATA | (size_t)m_string;
    } else {
        m_string = iter.operator*();
        str.m_tag = (size_t)POINTER_VALUE_STRING_TAG_IN_DATA | (size_t)m_string;
    }
}

AtomicString::AtomicString(Context* c, String* name)
{
    init(c->m_atomicStringMap, name);
}

void AtomicString::init(AtomicStringMap* ec, String* name)
{
    size_t v = name->getTagInFirstDataArea();
    if (v > POINTER_VALUE_STRING_TAG_IN_DATA) {
        m_string = (String*)(v & ~POINTER_VALUE_STRING_TAG_IN_DATA);
        return;
    }
    auto iter = ec->find(name);
    if (ec->end() == iter) {
        ec->insert(name);
        ASSERT(ec->find(name) != ec->end());
        m_string = name;
        name->m_tag = (size_t)POINTER_VALUE_STRING_TAG_IN_DATA | (size_t)m_string;
    } else {
        m_string = iter.operator*();
        name->m_tag = (size_t)POINTER_VALUE_STRING_TAG_IN_DATA | (size_t)m_string;
    }
}
}
