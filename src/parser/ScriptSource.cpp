/*
 * Copyright (c) 2026-present Samsung Electronics Co., Ltd
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
#include "parser/ScriptSource.h"

namespace Escargot {

void* ScriptSource::operator new(size_t size)
{
    static MAY_THREAD_LOCAL bool typeInited = false;
    static MAY_THREAD_LOCAL GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(ScriptSource)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ScriptSource, m_string));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(ScriptSource));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

ScriptSource* ScriptSource::createFromASCII(const char* data, size_t length)
{
    return new ScriptSource(SourceEncoding::ASCII, String::fromASCII(data, length), length);
}

ScriptSource* ScriptSource::createFromUTF8(const char* data, size_t length)
{
    // String::fromUTF8 narrows all-ASCII input and replaces invalid sequences with U+FFFD
    return new ScriptSource(SourceEncoding::UTF8, String::fromUTF8(data, length), length);
}

ScriptSource* ScriptSource::createFromUTF16(const char16_t* data, size_t length)
{
    bool isAllLatin1 = true;
    for (size_t i = 0; i < length; i++) {
        if (data[i] >= 256) {
            isAllLatin1 = false;
            break;
        }
    }

    // halve the storage of sources that happen to be given as UTF-16 but hold no wide character
    String* string = isAllLatin1 ? String::fromLatin1(data, length) : new UTF16String(data, length);
    return new ScriptSource(SourceEncoding::UTF16, string, length);
}

ScriptSource* ScriptSource::createFromString(String* string)
{
    // a string has no input encoding of its own, report how its content is stored
    auto data = string->bufferAccessData();
    return new ScriptSource(data.has8BitContent ? SourceEncoding::Latin1 : SourceEncoding::UTF16, string, data.length);
}

size_t ScriptSource::hashValue()
{
    if (UNLIKELY(!m_hasHashValue)) {
        m_hashValue = m_string->hashValue<0, false>();
        m_hasHashValue = true;
    }
    return m_hashValue;
}

} // namespace Escargot
