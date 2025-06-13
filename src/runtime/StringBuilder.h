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
class StringView;

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
        m_rootedStringSet.clear();
    }

    struct StringBuilderPiece {
        StringBuilderPiece()
            : m_type(Type::Char)
            , m_start(0)
            , m_length(0)
            , m_string(nullptr)
        {
        }

        enum class Type : uint8_t {
            Latin1StringPiece,
            UTF16StringStringPiece,
            UTF16StringStringButLatin1ContentPiece,
            String,
            ConstChar,
            Char,
        };
        Type m_type : 8;
        uint8_t m_start : 8;
        uint16_t m_length : 16;
        union {
            String* m_string;
            const char* m_raw;
            char16_t m_ch;
        };
    };

protected:
    static StringView* initLongPiece(String*, size_t s, size_t e);
    void checkStringLengthLimit(Optional<ExecutionState*> state, size_t extraLength = 0)
    {
        if (state && UNLIKELY((m_contentLength + extraLength) > STRING_MAXIMUM_LENGTH)) {
            throwStringLengthInvalidError(*state.value());
        }
    }

    void throwStringLengthInvalidError(ExecutionState& state);

    String* finalizeBase(StringBuilderPiece* piecesInlineStorage, Optional<ExecutionState*> state);

    bool m_has8BitContent : 1;
    size_t m_piecesInlineStorageUsage;
    size_t m_contentLength;
    std::vector<StringBuilderPiece> m_pieces;
    HashSet<void*, std::hash<void*>, std::equal_to<void*>, GCUtil::gc_malloc_allocator<void*>> m_rootedStringSet;
};

template <const size_t InlineStorageSize>
class StringBuilderImpl : public StringBuilderBase {
    void appendPiece(String* str, size_t s, size_t e, Optional<ExecutionState*> state = nullptr)
    {
        size_t pieceLen = e - s;
        if (pieceLen == 1) {
            appendPiece(str->charAt(s), state);
        } else if (pieceLen > 0) {
            checkStringLengthLimit(state, pieceLen);
            StringBuilderPiece piece;
            piece.m_string = str;
            piece.m_start = s;
            piece.m_length = pieceLen;
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

            if (UNLIKELY(s > std::numeric_limits<uint8_t>::max() || pieceLen > std::numeric_limits<uint16_t>::max())) {
                piece.m_type = StringBuilderPiece::Type::String;
                piece.m_start = piece.m_length = 0;
                if (s != 0 || e != str->length()) {
                    piece.m_string = reinterpret_cast<String*>(initLongPiece(str, s, e));
                }
            }

            m_contentLength += pieceLen;
            if (m_piecesInlineStorageUsage < InlineStorageSize) {
                m_piecesInlineStorage[m_piecesInlineStorageUsage++] = piece;
            } else {
                m_pieces.push_back(piece);
                m_rootedStringSet.insert(piece.m_string);
            }
        }
    }

    void appendPiece(const char* str, size_t len, Optional<ExecutionState*> state = nullptr)
    {
        checkStringLengthLimit(state, len);

        StringBuilderPiece piece;
        piece.m_start = 0;
        piece.m_length = len;
        piece.m_raw = str;
        piece.m_type = StringBuilderPiece::Type::ConstChar;
        if (piece.m_length) {
            m_contentLength += piece.m_length;
            if (m_piecesInlineStorageUsage < InlineStorageSize) {
                m_piecesInlineStorage[m_piecesInlineStorageUsage++] = piece;
            } else {
                m_pieces.push_back(piece);
            }
        }
    }

    void appendPiece(char16_t ch, Optional<ExecutionState*> state = nullptr)
    {
        checkStringLengthLimit(state, 1);

        StringBuilderPiece piece;
        piece.m_start = 0;
        piece.m_length = 1;
        piece.m_ch = ch;
        piece.m_type = StringBuilderPiece::Type::Char;

        if (ch > 255) {
            m_has8BitContent = false;
        }

        m_contentLength += 1;
        if (m_piecesInlineStorageUsage < InlineStorageSize) {
            m_piecesInlineStorage[m_piecesInlineStorageUsage++] = piece;
        } else {
            m_pieces.push_back(piece);
        }
    }

public:
    StringBuilderImpl()
        : StringBuilderBase()
    {
    }

    size_t contentLength() const { return m_contentLength; }

    template <const size_t srcLen>
    void appendString(const char (&src)[srcLen], Optional<ExecutionState*> state = nullptr)
    {
        appendString(src, srcLen - 1, state);
    }

    void appendString(const char* str, size_t len, Optional<ExecutionState*> state = nullptr)
    {
        appendPiece(str, len, state);
    }

    void appendChar(char16_t ch, Optional<ExecutionState*> state = nullptr)
    {
        appendPiece(ch, state);
    }

    void appendChar(char32_t ch, Optional<ExecutionState*> state = nullptr)
    {
        char16_t buf[2];
        auto c = utf32ToUtf16(ch, buf);
        appendPiece(buf[0], state);
        if (c == 2) {
            appendPiece(buf[1], state);
        }
    }

    void appendChar(char ch, Optional<ExecutionState*> state = nullptr)
    {
        appendPiece(ch, state);
    }

    void appendString(String* str, Optional<ExecutionState*> state = nullptr)
    {
        appendPiece(str, 0, str->length(), state);
    }

    void appendSubString(String* str, size_t s, size_t e, Optional<ExecutionState*> state = nullptr)
    {
        appendPiece(str, s, e, state);
    }

    String* finalize(Optional<ExecutionState*> state = nullptr) // provide ExecutionState if you need limit of string length(exception can be thrown only in ExecutionState area)
    {
        return finalizeBase(m_piecesInlineStorage, state);
    }

private:
    StringBuilderPiece m_piecesInlineStorage[InlineStorageSize];
};

using StringBuilder = StringBuilderImpl<STRING_BUILDER_INLINE_STORAGE_DEFAULT>;
using LargeStringBuilder = StringBuilderImpl<STRING_BUILDER_INLINE_STORAGE_DEFAULT * 4>;

} // namespace Escargot

#endif
