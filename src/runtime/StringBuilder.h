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

#include "Escargot.h"
#include "runtime/String.h"
#include "util/Optional.h"
#include "util/Vector.h"

namespace Escargot {

class ExecutionState;

class StringBuilderBase {
    MAKE_STACK_ALLOCATED();

public:
    StringBuilderBase()
    {
        m_has8BitContent = true;
        m_internalStorageContentLength = 0;
        m_piecesInlineStorageUsage = 0;
        m_processedString = String::emptyString;
    }

    void clear()
    {
        clearInternal();
        m_processedString = String::emptyString;
    }

    struct StringBuilderPiece {
        StringBuilderPiece()
            : m_type(Char)
            , m_start(0)
            , m_length(0)
            , m_string(nullptr)
        {
        }

        enum Type : uint8_t {
            Latin1StringPiece,
            UTF16StringStringPiece,
            UTF16StringStringButLatin1ContentPiece,
            ConstChar,
            Char,
        };

        Type m_type : 3;
        size_t m_start;
        size_t m_length;
        union {
            String* m_string;
            const char* m_raw;
            char16_t m_ch;
        };
    };

    size_t contentLength() const { return m_internalStorageContentLength + m_processedString->length(); }

protected:
    void checkStringLengthLimit(Optional<ExecutionState*> state, size_t extraLength = 0)
    {
        if (state && UNLIKELY((contentLength() + extraLength) > STRING_MAXIMUM_LENGTH)) {
            throwStringLengthInvalidError(*state.value());
        }
    }

    void throwStringLengthInvalidError(ExecutionState& state);

    void clearInternal()
    {
        m_has8BitContent = true;
        m_internalStorageContentLength = 0;
        m_piecesInlineStorageUsage = 0;
    }
    String* finalizeBase(StringBuilderPiece* piecesInlineStorage, Optional<ExecutionState*> state);
    void processInlineBufferAndMergeWithProcessedString(StringBuilderPiece* piecesInlineStorage);

    bool m_has8BitContent : 1;
    size_t m_piecesInlineStorageUsage;
    size_t m_internalStorageContentLength;
    String* m_processedString;
};

template <const size_t InlineStorageSize>
class StringBuilderImpl : public StringBuilderBase {
    void processInteralBufferIfNeeds()
    {
        if (m_piecesInlineStorageUsage == InlineStorageSize) {
            processInlineBufferAndMergeWithProcessedString(m_piecesInlineStorage);
            ASSERT(m_piecesInlineStorageUsage == 0);
            ASSERT(m_internalStorageContentLength == 0);
            ASSERT(m_has8BitContent);
        }
    }
    void appendPiece(String* str, size_t s, size_t e, Optional<ExecutionState*> state)
    {
        size_t len = e - s;
        if (len > 0) {
            checkStringLengthLimit(state, len);
            processInteralBufferIfNeeds();

            StringBuilderPiece piece;
            piece.m_string = str;
            piece.m_start = s;
            piece.m_length = len;

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

            m_internalStorageContentLength += len;
            m_piecesInlineStorage[m_piecesInlineStorageUsage++] = piece;
        }
    }

    void appendPiece(const char* str, size_t len, Optional<ExecutionState*> state)
    {
        if (!len) {
            return;
        }

        checkStringLengthLimit(state, len);
        processInteralBufferIfNeeds();

        StringBuilderPiece piece;
        piece.m_start = 0;
        piece.m_length = len;
        piece.m_raw = str;
        piece.m_type = StringBuilderPiece::Type::ConstChar;
        m_internalStorageContentLength += len;
        m_piecesInlineStorage[m_piecesInlineStorageUsage++] = piece;
    }

    void appendPiece(char16_t ch, Optional<ExecutionState*> state)
    {
        checkStringLengthLimit(state, 1);
        processInteralBufferIfNeeds();

        StringBuilderPiece piece;
        piece.m_start = 0;
        piece.m_length = 1;
        piece.m_ch = ch;
        piece.m_type = StringBuilderPiece::Type::Char;

        if (ch > 255) {
            m_has8BitContent = false;
        }

        m_internalStorageContentLength += 1;
        m_piecesInlineStorage[m_piecesInlineStorageUsage++] = piece;
    }

public:
    StringBuilderImpl()
        : StringBuilderBase()
    {
    }

    // provide ExecutionState if you need limit of string length(exception can be thrown only in ExecutionState area)
    template <const size_t srcLen>
    void appendString(const char (&src)[srcLen], Optional<ExecutionState*> state = nullptr)
    {
        appendPiece(src, srcLen - 1, state);
    }

    void appendString(const char* src, size_t len, Optional<ExecutionState*> state = nullptr)
    {
        appendPiece(src, len, state);
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

    void appendStringPiece(const StringBuilderPiece& piece, Optional<ExecutionState*> state = nullptr)
    {
        if (piece.m_type == StringBuilderPiece::Type::Char) {
            appendPiece(piece.m_ch, state);
        } else if (piece.m_type == StringBuilderPiece::Type::ConstChar) {
            appendPiece(piece.m_raw, piece.m_length, state);
        } else {
            appendSubString(piece.m_string, piece.m_start, piece.m_length, state);
        }
    }

    void appendStringBuilder(StringBuilderImpl<InlineStorageSize>& src, Optional<ExecutionState*> state = nullptr)
    {
        appendString(src.m_processedString, state);

        for (size_t i = 0; i < src.m_piecesInlineStorageUsage; i++) {
            appendStringPiece(src.m_piecesInlineStorage[i], state);
        }
    }

    String* finalize(Optional<ExecutionState*> state = nullptr)
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
