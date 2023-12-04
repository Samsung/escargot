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

#ifndef __EscargotStringBuilder__
#define __EscargotStringBuilder__

#include "runtime/String.h"
#include "util/Vector.h"

namespace Escargot {

class ExecutionState;

class StringBuilderBase {
    MAKE_STACK_ALLOCATED();

public:
    StringBuilderBase()
    {
        m_has8BitContent = true;
        m_contentLength = 0;
        m_piecesInlineStorageUsage = 0;
    }

    void clear()
    {
        m_has8BitContent = true;
        m_contentLength = 0;
        m_piecesInlineStorageUsage = 0;
        m_pieces.clear();
    }

protected:
    struct StringBuilderPiece {
        StringBuilderPiece()
            : m_type(Char)
            , m_string(nullptr)
            , m_start(0)
            , m_end(0)
        {
        }

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

    String* finalizeBase(StringBuilderPiece* piecesInlineStorage, ExecutionState* state);

    bool m_has8BitContent : 1;
    size_t m_piecesInlineStorageUsage;
    size_t m_contentLength;
    Vector<StringBuilderPiece, GCUtil::gc_malloc_allocator<StringBuilderPiece>,
           ComputeReservedCapacityFunctionWithLog2<200>>
        m_pieces;
};

template <const size_t InlineStorageSize>
class StringBuilderImpl : public StringBuilderBase {
    void appendPiece(String* str, size_t s, size_t e)
    {
        if (e - s > 0) {
            StringBuilderPiece piece;
            piece.m_string = str;
            piece.m_start = s;
            piece.m_end = e;

            const auto& data = str->bufferAccessData();
            if (!data.has8BitContent) {
                bool has8 = true;
                for (size_t i = s; i < e; i++) {
                    if (((char16_t*)data.buffer)[i] > 255) {
                        has8 = false;
                        break;
                    }
                }

                if (!has8) {
                    m_has8BitContent = false;
                    piece.m_type = StringBuilderPiece::Type::UTF16StringStringPiece;
                } else {
                    piece.m_type = StringBuilderPiece::Type::UTF16StringStringButLatin1ContentPiece;
                }

            } else {
                piece.m_type = StringBuilderPiece::Type::Latin1StringPiece;
            }

            m_contentLength += e - s;
            if (m_piecesInlineStorageUsage < InlineStorageSize) {
                m_piecesInlineStorage[m_piecesInlineStorageUsage++] = piece;
            } else
                m_pieces.push_back(piece);
        }
    }

    void appendPiece(const char* str)
    {
        StringBuilderPiece piece;
        piece.m_start = 0;
        piece.m_end = strlen(str);
        piece.m_raw = str;
        piece.m_type = StringBuilderPiece::Type::ConstChar;
        if (piece.m_end) {
            m_contentLength += piece.m_end;
            if (m_piecesInlineStorageUsage < InlineStorageSize) {
                m_piecesInlineStorage[m_piecesInlineStorageUsage++] = piece;
            } else
                m_pieces.push_back(piece);
        }
    }

    void appendPiece(char16_t ch)
    {
        StringBuilderPiece piece;
        piece.m_start = 0;
        piece.m_end = 1;
        piece.m_ch = ch;
        piece.m_type = StringBuilderPiece::Type::Char;

        if (ch > 255) {
            m_has8BitContent = false;
        }

        m_contentLength += 1;
        if (m_piecesInlineStorageUsage < InlineStorageSize) {
            m_piecesInlineStorage[m_piecesInlineStorageUsage++] = piece;
        } else
            m_pieces.push_back(piece);
    }

public:
    StringBuilderImpl()
        : StringBuilderBase()
    {
    }

    size_t contentLength() const { return m_contentLength; }
    void appendString(const char* str)
    {
        appendPiece(str);
    }

    void appendChar(char16_t ch)
    {
        appendPiece(ch);
    }

    void appendChar(char32_t ch)
    {
        char16_t buf[2];
        auto c = utf32ToUtf16(ch, buf);
        appendPiece(buf[0]);
        if (c == 2) {
            appendPiece(buf[1]);
        }
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

    void appendStringBuilder(StringBuilderImpl<InlineStorageSize>& src)
    {
        reserveMoreSpaceForPieces(src.m_piecesInlineStorageUsage + src.m_pieces.size());

        for (size_t i = 0; i < src.m_piecesInlineStorageUsage; i++) {
            if (src.m_piecesInlineStorage[i].m_type == StringBuilderPiece::Type::Char) {
                appendPiece(src.m_piecesInlineStorage[i].m_ch);
            } else if (src.m_piecesInlineStorage[i].m_type == StringBuilderPiece::Type::ConstChar) {
                appendPiece(src.m_piecesInlineStorage[i].m_raw);
            } else {
                appendSubString(src.m_piecesInlineStorage[i].m_string, src.m_piecesInlineStorage[i].m_start, src.m_piecesInlineStorage[i].m_end);
            }
        }

        for (size_t i = 0; i < src.m_pieces.size(); i++) {
            if (src.m_pieces[i].m_type == StringBuilderPiece::Type::Char) {
                appendPiece(src.m_pieces[i].m_ch);
            } else if (src.m_pieces[i].m_type == StringBuilderPiece::Type::ConstChar) {
                appendPiece(src.m_pieces[i].m_raw);
            } else {
                appendSubString(src.m_pieces[i].m_string, src.m_pieces[i].m_start, src.m_pieces[i].m_end);
            }
        }
    }

    String* finalize(ExecutionState* state = nullptr) // provide ExecutionState if you need limit of string length(exception can be thrown only in ExecutionState area)
    {
        return finalizeBase(m_piecesInlineStorage, state);
    }

    void reserveMoreSpaceForPieces(size_t s)
    {
        if (s + m_piecesInlineStorageUsage > InlineStorageSize) {
            m_pieces.reserve(s + m_pieces.size());
        }
    }

private:
    StringBuilderPiece m_piecesInlineStorage[InlineStorageSize];
};

using StringBuilder = StringBuilderImpl<STRING_BUILDER_INLINE_STORAGE_DEFAULT>;
using LargeStringBuilder = StringBuilderImpl<STRING_BUILDER_INLINE_STORAGE_DEFAULT * 4>;

} // namespace Escargot

#endif
