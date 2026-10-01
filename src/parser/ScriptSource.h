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

#ifndef __EscargotScriptSource__
#define __EscargotScriptSource__

#include "runtime/String.h"
#include "runtime/StringView.h"

namespace Escargot {

class ScriptSource;

// Encoding of the input a ScriptSource was created from.
// This only describes the original input; every position ScriptSource exposes
// is a UTF-16 code unit offset regardless of how the text is stored.
enum class SourceEncoding : uint8_t {
    ASCII,
    Latin1,
    UTF8,
    UTF16,
};

// Half-open [start, end) UTF-16 code unit range of a ScriptSource.
struct SourceRange {
    // null only in a default constructed InterpretedCodeBlock
    Optional<ScriptSource*> source;
    size_t start;
    size_t end;

    size_t length() const
    {
        ASSERT(end >= start);
        return end - start;
    }

    // `offsetInRange` is relative to `start`
    char16_t charAt(size_t offsetInRange) const;
    SourceRange subrange(size_t localStart, size_t localEnd) const;
    // both share the storage of the underlying string, they never copy the text
    String* toString() const;
    StringView toStringView() const;
};

// Script source text with the provenance of the input it was built from.
// The text itself is always held by a String, so that every consumer
// (lexer, Function.prototype.toString, the debugger, ...) can keep using plain
// string views over it without any copy or re-decoding.
class ScriptSource : public gc {
public:
    // ASCII input is validated by the caller, a non-ASCII byte is read as Latin-1
    static ScriptSource* createFromASCII(const char* data, size_t length);
    // invalid UTF-8 sequences are replaced by U+FFFD, like String::fromUTF8 does
    static ScriptSource* createFromUTF8(const char* data, size_t length);
    static ScriptSource* createFromUTF16(const char16_t* data, size_t length);
    // keeps `string` as is, without copying it
    static ScriptSource* createFromString(String* string);

    SourceEncoding encoding() const
    {
        return m_encoding;
    }

    // length in UTF-16 code units
    size_t length() const
    {
        return m_string->length();
    }

    // number of units(bytes for ASCII/UTF-8, char16_t for UTF-16) of the input
    size_t storageLength() const
    {
        return m_storageLength;
    }

    String* string() const
    {
        return m_string;
    }

    char16_t charAt(size_t utf16Offset) const
    {
        ASSERT(utf16Offset < length());
        return m_string->charAt(utf16Offset);
    }

    SourceRange range() const
    {
        return SourceRange{ const_cast<ScriptSource*>(this), 0, length() };
    }

    SourceRange range(size_t start, size_t end) const
    {
        ASSERT(start <= end && end <= length());
        return SourceRange{ const_cast<ScriptSource*>(this), start, end };
    }

    // hash of the whole source text, computed on the first call
    size_t hashValue();

    void* operator new(size_t size);
    void* operator new[](size_t) = delete;

private:
    ScriptSource(SourceEncoding encoding, String* string, size_t storageLength)
        : m_encoding(encoding)
        , m_storageLength(storageLength)
        , m_hashValue(0)
        , m_hasHashValue(false)
        , m_string(string)
    {
        ASSERT(!!string);
    }

    SourceEncoding m_encoding;
    size_t m_storageLength;
    size_t m_hashValue;
    bool m_hasHashValue;
    String* m_string;
};

inline char16_t SourceRange::charAt(size_t offsetInRange) const
{
    ASSERT(offsetInRange < length());
    return source->charAt(start + offsetInRange);
}

inline SourceRange SourceRange::subrange(size_t localStart, size_t localEnd) const
{
    ASSERT(localStart <= localEnd && localEnd <= length());
    return source->range(start + localStart, start + localEnd);
}

inline String* SourceRange::toString() const
{
    String* whole = source->string();
    if (start == 0 && end == whole->length()) {
        return whole;
    }
    return new StringView(whole, start, end);
}

inline StringView SourceRange::toStringView() const
{
    return StringView(source->string(), start, end);
}

} // namespace Escargot

#endif
