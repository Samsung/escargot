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

#ifndef __EscargotStringBuilder__
#define __EscargotStringBuilder__

#include "runtime/String.h"
#include "util/Vector.h"

namespace Escargot {

class ExecutionState;

class StringBuilder {
    MAKE_STACK_ALLOCATED();
    struct StringBuilderPiece {
        enum Type {
            Latin1StringPiece,
            UTF16StringStringPiece,
            UTF16StringStringButLatin1ContentPiece,
            ConstChar,
            Char,
        };
        Type m_type;
        union {
            String* m_string;
            const char* m_raw;
            char16_t m_ch;
        };
        size_t m_start, m_end;
    };

    void appendPiece(char16_t ch);
    void appendPiece(const char* str);
    void appendPiece(String* str, size_t s, size_t e);

public:
    StringBuilder()
    {
        m_has8BitContent = true;
        m_contentLength = 0;
        m_piecesInlineStorageUsage = 0;
    }

    size_t contentLength() { return m_contentLength; }
    void appendString(const char* str)
    {
        appendPiece(str);
    }

    void appendChar(char16_t ch)
    {
        appendPiece(ch);
    }

    void appendChar(char ch)
    {
        appendPiece(ch);
    }

    void appendString(String* str)
    {
        appendPiece(str, 0, str->length());
    }

    void appendSubString(String* str, size_t s, size_t e)
    {
        appendPiece(str, s, e);
    }

    String* finalize(ExecutionState* state = nullptr); // provide ExecutionState if you need limit of string length(exception can be thrown only in ExecutionState area)

private:
    bool m_has8BitContent : 1;
    size_t m_piecesInlineStorageUsage;
    size_t m_contentLength;
    StringBuilderPiece m_piecesInlineStorage[STRING_BUILDER_INLINE_STORAGE_MAX];
    Vector<StringBuilderPiece, gc_allocator_ignore_off_page<StringBuilderPiece>, 200> m_pieces;
};
}

#endif
