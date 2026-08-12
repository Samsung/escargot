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
        m_rootedStringSet.reset();
        m_numberScratch.reset();
    }

    struct StringBuilderPiece {
        // intentionally left uninitialized (NOLINT(cppcoreguidelines-pro-type-member-init)):
        // every appendXxxPiece() call site sets m_type, m_start, m_length, and the active
        // union member before the piece is ever pushed to storage or read back, so zero-
        // initializing them here is provably dead work - and since m_piecesInlineStorage[]
        // default-constructs InlineStorageSize (24/96) of these per StringBuilder regardless
        // of how many actually get used, that dead work was paid on every single construction.
        StringBuilderPiece() = default;

        enum class Type : uint8_t {
            Latin1StringPiece,
            UTF16StringStringPiece,
            UTF16StringStringButLatin1ContentPiece,
            String,
            ConstChar,
            Char,
            // a plain int32 formatted lazily (at finalize time) straight into the
            // destination buffer - no allocation, no dtoa (a simple decimal-digit
            // loop is enough, no shortest-round-trip float logic needed). m_int32Value
            // fits in the union's existing pointer-sized slot on every platform
            // (4 bytes <= sizeof(void*) whether that's 4 or 8), so this costs nothing
            // extra even on 32-bit targets like arm32.
            Int32Digits,
            // a non-integer double, dtoa'd once at append time into
            // StringBuilderBase::m_numberScratch instead of a GC-allocated String.
            // m_digitsOffset is an offset into that buffer, not a pointer, since the
            // buffer can grow (and reallocate) across further appends - and unlike a
            // raw `double` member, an offset is pointer-sized so it doesn't grow the
            // union either (a `double` would, on 32-bit platforms where a pointer is
            // only 4 bytes).
            Digits,
        };
        Type m_type;
        size_t m_start;
        size_t m_length;
        union {
            String* m_string;
            const char* m_raw;
            char16_t m_ch;
            int32_t m_int32Value;
            size_t m_digitsOffset;
        };
    };

protected:
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
    using StringSet = HashSet<void*, std::hash<void*>, std::equal_to<void*>, GCUtil::gc_malloc_allocator<void*>>;
    Optional<StringSet*> m_rootedStringSet;
    // backing storage for Digits pieces - plain (non-GC) bytes, never a GC pointer,
    // so (like m_rootedStringSet) it costs nothing beyond one pointer's worth of
    // space until the first non-integer appendNumber() call actually allocates it
    Optional<std::vector<char>*> m_numberScratch;
};

// number of decimal digits int32Digits() will write, including a leading '-'
// for negative values - must stay in sync with it, since finalizeBase() sizes
// its output buffer from this before any digit is actually written
static inline uint8_t decimalDigitLengthOf(int32_t value)
{
    uint32_t v = value < 0 ? (uint32_t)(-(int64_t)value) : (uint32_t)value;
    uint8_t digits = 1;
    while (v >= 10) {
        v /= 10;
        digits++;
    }
    return digits + (value < 0 ? 1 : 0);
}

template <const size_t InlineStorageSize>
class StringBuilderImpl : public StringBuilderBase {
    // returns the slot a new piece should be written into: the next inline
    // slot while there's room, otherwise a freshly default-constructed
    // (uninitialized, see StringBuilderPiece's comment) entry in the spilled
    // vector. Callers write fields directly into the returned slot instead of
    // building a StringBuilderPiece temporary and copying it in afterward -
    // with a local temp, the compiler has to store each field (bitfields +
    // union) to the stack and then reload the whole 16 bytes to copy it, a
    // classic blocked store-forwarding stall. Writing straight into the final
    // slot avoids that reload entirely.
    StringBuilderPiece& nextPieceSlot()
    {
        if (m_piecesInlineStorageUsage < InlineStorageSize) {
            return m_piecesInlineStorage[m_piecesInlineStorageUsage++];
        }
        m_pieces.emplace_back();
        return m_pieces.back();
    }

    void appendPiece(String* str, size_t s, size_t e, Optional<ExecutionState*> state = nullptr)
    {
        size_t pieceLen = e - s;
        if (pieceLen == 1) {
            appendPiece(str->charAt(s), state);
        } else if (pieceLen > 0) {
            checkStringLengthLimit(state, pieceLen);
            StringBuilderPiece& piece = nextPieceSlot();
            piece.m_string = str;
            if (m_piecesInlineStorageUsage >= InlineStorageSize) {
                if (!m_rootedStringSet) {
                    m_rootedStringSet = new (GC) StringSet;
                }
                m_rootedStringSet->insert(str);
            }
            piece.m_start = s;
            piece.m_length = pieceLen;
            m_contentLength += pieceLen;
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
        }
    }

    void appendPiece(const char* str, size_t len, Optional<ExecutionState*> state = nullptr)
    {
        checkStringLengthLimit(state, len);

        uint16_t length = static_cast<uint16_t>(len);
        if (length) {
            m_contentLength += length;

            StringBuilderPiece& piece = nextPieceSlot();
            piece.m_start = 0;
            piece.m_length = length;
            piece.m_raw = str;
            piece.m_type = StringBuilderPiece::Type::ConstChar;
        }
    }

    void appendPiece(char16_t ch, Optional<ExecutionState*> state = nullptr)
    {
        checkStringLengthLimit(state, 1);

        if (ch > 255) {
            m_has8BitContent = false;
        }

        m_contentLength += 1;
        StringBuilderPiece& piece = nextPieceSlot();
        piece.m_start = 0;
        piece.m_length = 1;
        piece.m_ch = ch;
        piece.m_type = StringBuilderPiece::Type::Char;
    }

    // no allocation, no dtoa - just remember the int32 and its digit count now,
    // format the actual characters straight into the destination buffer at
    // finalize() time (see processPiece() in StringBuilder.cpp)
    void appendInt32Piece(int32_t value, Optional<ExecutionState*> state = nullptr)
    {
        uint8_t len = decimalDigitLengthOf(value);
        checkStringLengthLimit(state, len);

        m_contentLength += len;
        StringBuilderPiece& piece = nextPieceSlot();
        piece.m_start = 0;
        piece.m_length = len;
        piece.m_int32Value = value;
        piece.m_type = StringBuilderPiece::Type::Int32Digits;
    }

    // named differently from the appendPiece() overload set on purpose: a plain
    // `char` converts to both char16_t and double with no better match, so adding
    // this as an appendPiece(double) overload made appendChar(char) ambiguous.
    // caller must have already excluded NaN/Infinity (dtoa() below only handles
    // finite doubles) and integers that fit in int32 (use appendInt32Piece for
    // those - no allocation needed there at all)
    void appendDigitsPiece(double d, Optional<ExecutionState*> state = nullptr)
    {
        ASCIIStringDataNonGCStd digits = dtoa(d);
        checkStringLengthLimit(state, digits.length());

        if (!m_numberScratch) {
            m_numberScratch = new (GC) std::vector<char>;
        }
        size_t offset = m_numberScratch.value()->size();
        m_numberScratch.value()->insert(m_numberScratch.value()->end(), digits.data(), digits.data() + digits.length());

        uint16_t length = static_cast<uint16_t>(digits.length());
        m_contentLength += length;
        StringBuilderPiece& piece = nextPieceSlot();
        piece.m_start = 0;
        piece.m_length = length;
        piece.m_digitsOffset = offset;
        piece.m_type = StringBuilderPiece::Type::Digits;
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

    // zero-allocation path for a plain int32 - see appendInt32Piece()'s comment
    void appendInt32(int32_t value, Optional<ExecutionState*> state = nullptr)
    {
        appendInt32Piece(value, state);
    }

    // caller is responsible for excluding NaN/Infinity and int32-representable
    // values beforehand (see appendDigitsPiece()'s comment - the latter should
    // go through appendInt32() instead, which needs no allocation at all)
    void appendNumber(double d, Optional<ExecutionState*> state = nullptr)
    {
        appendDigitsPiece(d, state);
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
