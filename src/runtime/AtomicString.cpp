/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
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

AtomicString::AtomicString(Context* c, const SourceStringView& sv)
{
    size_t v = sv.getTagInFirstDataArea();
    if (v > POINTER_VALUE_STRING_SYMBOL_TAG_IN_DATA) {
        m_string = (String*)(v & ~POINTER_VALUE_STRING_SYMBOL_TAG_IN_DATA);
        return;
    }

    AtomicStringMap* ec = c->atomicStringMap();
    SourceStringView& str = const_cast<SourceStringView&>(sv);
    String* name = &str;
    auto iter = ec->find(name);
    if (ec->end() == iter) {
        SourceStringView* newSv = new SourceStringView(sv);
        ec->insert(newSv);
        ASSERT(ec->find(newSv) != ec->end());
        m_string = newSv;
        str.m_tag = (size_t)POINTER_VALUE_STRING_SYMBOL_TAG_IN_DATA | (size_t)m_string;
    } else {
        m_string = iter.operator*();
        str.m_tag = (size_t)POINTER_VALUE_STRING_SYMBOL_TAG_IN_DATA | (size_t)m_string;
    }
}

AtomicString::AtomicString(Context* c, const StringView& sv)
{
    size_t v = sv.getTagInFirstDataArea();
    if (v > POINTER_VALUE_STRING_SYMBOL_TAG_IN_DATA) {
        m_string = (String*)(v & ~POINTER_VALUE_STRING_SYMBOL_TAG_IN_DATA);
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
        str.m_tag = (size_t)POINTER_VALUE_STRING_SYMBOL_TAG_IN_DATA | (size_t)m_string;
    } else {
        m_string = iter.operator*();
        str.m_tag = (size_t)POINTER_VALUE_STRING_SYMBOL_TAG_IN_DATA | (size_t)m_string;
    }
}

AtomicString::AtomicString(Context* c, String* name)
{
    init(c->m_atomicStringMap, name);
}

void AtomicString::initStaticString(AtomicStringMap* ec, String* name)
{
    ASSERT(ec->find(name) == ec->end());
    ec->insert(name);
    m_string = name;
    name->m_tag = (size_t)POINTER_VALUE_STRING_SYMBOL_TAG_IN_DATA | (size_t)m_string;
}

void AtomicString::init(AtomicStringMap* ec, String* name)
{
    size_t v = name->getTagInFirstDataArea();
    if (v > POINTER_VALUE_STRING_SYMBOL_TAG_IN_DATA) {
        m_string = (String*)(v & ~POINTER_VALUE_STRING_SYMBOL_TAG_IN_DATA);
        return;
    }
    auto iter = ec->find(name);
    if (ec->end() == iter) {
        ec->insert(name);
        ASSERT(ec->find(name) != ec->end());
        m_string = name;
        name->m_tag = (size_t)POINTER_VALUE_STRING_SYMBOL_TAG_IN_DATA | (size_t)m_string;
    } else {
        m_string = iter.operator*();
        name->m_tag = (size_t)POINTER_VALUE_STRING_SYMBOL_TAG_IN_DATA | (size_t)m_string;
    }
}
}
