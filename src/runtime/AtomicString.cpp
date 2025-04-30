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
#include "AtomicString.h"
#include "Context.h"

namespace Escargot {

AtomicString::AtomicString(ExecutionState& ec, const char16_t* src, size_t len)
{
    init(ec.context()->m_atomicStringMap, src, len);
}

AtomicString::AtomicString(ExecutionState& ec, const char* src, size_t len)
{
    init(ec.context()->m_atomicStringMap, src, len);
}

AtomicString::AtomicString(Context* c, const char* src, size_t len)
{
    init(c->m_atomicStringMap, src, len);
}

AtomicString::AtomicString(Context* c, const LChar* src, size_t len)
{
    init(c->m_atomicStringMap, src, len);
}

AtomicString::AtomicString(Context* c, const char16_t* src, size_t len)
{
    init(c->m_atomicStringMap, src, len);
}

AtomicString::AtomicString(AtomicStringMap* map, const char* src, size_t len)
{
    init(map, src, len);
}

AtomicString::AtomicString(AtomicStringMap* map, const char* src, size_t len, AtomicString::FromExternalMemoryTag)
{
    init(map, src, len, true);
}

AtomicString::AtomicString(AtomicStringMap* map, const LChar* src, size_t len)
{
    init(map, src, len);
}

AtomicString::AtomicString(AtomicStringMap* map, const char16_t* src, size_t len)
{
    init(map, src, len);
}

class ASCIIStringOnStack : public String {
public:
    ASCIIStringOnStack(const char* str, size_t len)
        : String()
    {
        m_bufferData.has8BitContent = true;
        m_bufferData.length = len;
        m_bufferData.buffer = str;
    }

    virtual char16_t charAt(const size_t idx) const
    {
        return m_bufferData.uncheckedCharAtFor8Bit(idx);
    }

    virtual const LChar* characters8() const
    {
        return (const LChar*)m_bufferData.buffer;
    }

    // unused
    virtual UTF16StringData toUTF16StringData() const
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

    // unused
    virtual UTF8StringData toUTF8StringData() const
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

    // unused
    virtual UTF8StringDataNonGCStd toNonGCUTF8StringData(int options = StringWriteOption::NoOptions) const
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

    void* operator new(size_t size) = delete;
    void* operator new(size_t, void* ptr) = delete;
    void* operator new[](size_t size) = delete;
};

void AtomicString::init(AtomicStringMap* map, const char* src, size_t len, bool fromExternalMemory)
{
    ASCIIStringOnStack stringForSearch(src, len);

    auto iter = map->find(&stringForSearch);
    if (map->end() == iter) {
        String* newStr;
        if (fromExternalMemory) {
            newStr = new ASCIIStringFromExternalMemory(src, len);
        } else {
            newStr = String::fromLatin1(reinterpret_cast<const LChar*>(src), len);
        }
        map->insert(newStr);
        m_string = newStr;
        newStr->m_typeTag = (size_t)POINTER_VALUE_STRING_TAG_IN_DATA | (size_t)m_string;
    } else {
        m_string = iter.operator*();
    }
}

class Latin1StringOnStack : public String {
public:
    Latin1StringOnStack(const LChar* str, size_t len)
        : String()
    {
        m_bufferData.has8BitContent = true;
        m_bufferData.length = len;
        m_bufferData.buffer = str;
    }

    void* operator new(size_t size) = delete;
    void* operator new(size_t, void* ptr) = delete;
    void* operator new[](size_t size) = delete;

    virtual char16_t charAt(const size_t idx) const
    {
        return m_bufferData.uncheckedCharAtFor8Bit(idx);
    }

    virtual const LChar* characters8() const
    {
        return (const LChar*)m_bufferData.buffer;
    }

    // unused
    virtual UTF8StringData toUTF8StringData() const
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

    // unused
    virtual UTF16StringData toUTF16StringData() const
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

    // unused
    virtual UTF8StringDataNonGCStd toNonGCUTF8StringData(int options = StringWriteOption::NoOptions) const
    {
        RELEASE_ASSERT_NOT_REACHED();
    }
};

void AtomicString::init(AtomicStringMap* map, const LChar* src, size_t len)
{
    Latin1StringOnStack stringForSearch(src, len);

    auto iter = map->find(&stringForSearch);
    if (map->end() == iter) {
        Latin1String* newStr = new Latin1String(src, len);
        map->insert(newStr);
        m_string = newStr;
        newStr->m_typeTag = (size_t)POINTER_VALUE_STRING_TAG_IN_DATA | (size_t)m_string;
    } else {
        m_string = iter.operator*();
    }
}

class UTF16StringOnStack : public String {
public:
    UTF16StringOnStack(const char16_t* str, size_t len)
        : String()
    {
        m_bufferData.has8BitContent = false;
        m_bufferData.length = len;
        m_bufferData.buffer = str;
    }

    virtual char16_t charAt(const size_t idx) const
    {
        return m_bufferData.uncheckedCharAtFor16Bit(idx);
    }

    virtual const char16_t* characters16() const
    {
        return (const char16_t*)m_bufferData.buffer;
    }

    // unused
    virtual UTF16StringData toUTF16StringData() const
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

    // unused
    virtual UTF8StringData toUTF8StringData() const
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

    // unused
    virtual UTF8StringDataNonGCStd toNonGCUTF8StringData(int options = StringWriteOption::NoOptions) const
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

    void* operator new(size_t size) = delete;
    void* operator new(size_t, void* ptr) = delete;
    void* operator new[](size_t size) = delete;
};

void AtomicString::init(AtomicStringMap* map, const char16_t* src, size_t len)
{
    UTF16StringOnStack stringForSearch(src, len);

    auto iter = map->find(&stringForSearch);
    if (map->end() == iter) {
        String* newStr;
        if (isAllASCII(src, len)) {
            newStr = String::fromLatin1(src, len);
        } else {
            newStr = new UTF16String(src, len);
        }
        map->insert(newStr);
        m_string = newStr;
        newStr->m_typeTag = (size_t)POINTER_VALUE_STRING_TAG_IN_DATA | (size_t)m_string;
    } else {
        m_string = iter.operator*();
    }
}

AtomicString::AtomicString(Context* c, const StringView& sv)
{
    size_t v = sv.getTypeTag();
    if (v > POINTER_VALUE_STRING_TAG_IN_DATA) {
        m_string = (String*)(v & ~POINTER_VALUE_STRING_TAG_IN_DATA);
        return;
    }

    AtomicStringMap* ec = c->atomicStringMap();
    auto iter = ec->find(&const_cast<StringView&>(sv));
    if (ec->end() == iter) {
        String* newString;
        auto buffer = sv.bufferAccessData();
        if (buffer.has8BitContent) {
            newString = String::fromLatin1(reinterpret_cast<const LChar*>(buffer.buffer), buffer.length);
        } else {
            newString = new UTF16String((const char16_t*)buffer.buffer, buffer.length);
        }
        ec->insert(newString);
        ASSERT(ec->find(newString) != ec->end());
        m_string = newString;
        const_cast<StringView&>(sv).m_typeTag = (size_t)POINTER_VALUE_STRING_TAG_IN_DATA | (size_t)m_string;
    } else {
        m_string = iter.operator*();
        const_cast<StringView&>(sv).m_typeTag = (size_t)POINTER_VALUE_STRING_TAG_IN_DATA | (size_t)m_string;
    }
}

void AtomicString::initStaticString(AtomicStringMap* ec, String* name)
{
    ASSERT(ec->find(name) == ec->end());
    ec->insert(name);
    m_string = name;
    name->m_typeTag = (size_t)POINTER_VALUE_STRING_TAG_IN_DATA | (size_t)m_string;
}

void AtomicString::init(Context* c, String* name)
{
    AtomicStringMap* ec = c->atomicStringMap();
    auto iter = ec->find(name);
    if (ec->end() == iter) {
        if (name->isStringView()) {
            auto buffer = name->bufferAccessData();
            if (buffer.has8BitContent) {
                name = String::fromLatin1(reinterpret_cast<const LChar*>(buffer.buffer), buffer.length);
            } else {
                name = new UTF16String((const char16_t*)buffer.buffer, buffer.length);
            }
        }
        ASSERT(!name->isStringView());
        ec->insert(name);
        ASSERT(ec->find(name) != ec->end());
        m_string = name;
        name->m_typeTag = (size_t)POINTER_VALUE_STRING_TAG_IN_DATA | (size_t)m_string;
    } else {
        m_string = iter.operator*();
        name->m_typeTag = (size_t)POINTER_VALUE_STRING_TAG_IN_DATA | (size_t)m_string;
    }
}
} // namespace Escargot
