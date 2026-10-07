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
class VMInstance;
class ReloadableSourceString;
#if defined(ENABLE_COMPRESSIBLE_STRING)
class CompressibleString;
#endif

#if defined(ENABLE_RELOADABLE_STRING)
// Raw UTF-8 source bytes. This storage is never exposed as a JavaScript String.
class ReloadableSourceString : public gc {
    friend class VMInstance;

public:
    ReloadableSourceString(VMInstance* instance, size_t byteLength, Optional<void*> callbackData,
                           void* (*loadCallback)(void*), void (*unloadCallback)(void*, void*));
    void* operator new(size_t size);
    StringBufferAccessData bufferAccessData() const;
    bool unload();

private:
    VMInstance* m_vmInstance;
    Optional<void*> m_callbackData;
    mutable Optional<void*> m_buffer;
    size_t m_byteLength;
    mutable size_t m_refCount;
    bool m_isOwnerMayFreed;
    void* (*m_loadCallback)(void*);
    void (*m_unloadCallback)(void*, void*);
};
#endif

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
    // a view over the storage of the source when the source is flat, a copy of the
    // range when it is held as raw UTF-8 bytes -- see ScriptSource below
    String* toString() const;
    StringView toStringView() const;
};

// Script source text with the provenance of the input it was built from.
//
// The text is held in one of two ways:
//  - flat storage: a String whose buffer is a random access array of UTF-16 code
//    units(or of Latin-1 characters). Every position maps to an index of that
//    buffer, so a subrange of the source is a free StringView over it.
//  - raw UTF-8 storage: the bytes of the input as they were given. A source read
//    from a UTF-8 file is often mostly ASCII with a few non-ASCII characters
//    in comments and literals. Keeping the bytes avoids a second decoded copy
//    of the whole text for as long as the script lives. A subrange is decoded
//    into a string of its own instead,
//    which is what every consumer of the text(Function.prototype.toString, a lazy
//    function parse, the debugger, ...) asks for anyway.
//
// Positions are UTF-16 code unit offsets in both cases, so neither the lexer nor
// anything holding a position has to know which storage is in use.
class ScriptSource : public gc {
public:
    // ASCII input is validated by the caller, a non-ASCII byte is read as Latin-1
    static ScriptSource* createFromASCII(const char* data, size_t length);
    // invalid UTF-8 sequences are replaced by U+FFFD, like String::fromUTF8 does
    static ScriptSource* createFromUTF8(const char* data, size_t length);
    static void* allocateUTF8Buffer(size_t byteLength);
    static void deallocateUTF8Buffer(void* buffer, size_t byteLength);
    static ScriptSource* createFromAlreadyAllocatedUTF8Buffer(VMInstance* instance, void* buffer, size_t byteLength);
    static ScriptSource* createReloadableUTF8(VMInstance* instance, size_t byteLength, Optional<void*> callbackData,
                                              void* (*loadCallback)(void*), void (*unloadCallback)(void*, void*));
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
        return m_length;
    }

    // number of units(bytes for ASCII/UTF-8, char16_t for UTF-16) of the input
    size_t storageLength() const
    {
        return m_storageLength;
    }

    // true when the text is held by a flat String, in which case string() is free
    // and every subrange of the source is a view instead of a copy
    bool hasFlatString() const
    {
        return !!m_string;
    }

    String* flatString() const
    {
        ASSERT(hasFlatString());
        return m_string.unwrap();
    }

    // the whole text as a String. free with flat storage; with raw UTF-8 storage
    // the text is decoded on the first call and the result is cached weakly, so
    // this costs a decode of the whole source whenever nobody else holds it --
    // ask for the smallest range that is needed instead whenever that is possible
    String* string() const;

    // [start, end) as a String. a view with flat storage, a copy with raw UTF-8
    String* substring(size_t start, size_t end) const;

    char16_t charAt(size_t utf16Offset) const;

    // Decode a bounded run for the parser's sliding window. The caller owns the
    // buffer; no String containing the whole source is created.
    void copyUTF16(size_t start, size_t count, char16_t* buffer, uint32_t* byteOffsets = nullptr) const;

    // A source window with one byte per UTF-16 unit can be read directly.
    Optional<const char*> asciiBytes(size_t start, size_t count) const;

    const char* rawUTF8Data(size_t byteStart, size_t count) const;

    // Transfer a large raw byte buffer to the VM's chunked string compressor
    // once the initial parse has finished. Future reads decompress only the
    // byte ranges requested by a parser window.
    void compactUTF8(VMInstance* instance);

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
    // a position of the raw UTF-8 storage, able to sit between the two code units
    // of a surrogate pair. decodes exactly like utf8StringToUTF16String does, so
    // that the two storages describe the same text
    class UTF8Cursor {
    public:
        UTF8Cursor(const ScriptSource* source, size_t bytePosition, char16_t pendingTrail)
            : m_source(source)
            , m_buffer(nullptr)
            , m_bufferLength(source->m_storageLength)
            , m_bytePosition(bytePosition)
            , m_pendingTrail(pendingTrail)
            , m_accessData(true, 0, nullptr)
            , m_spanEnd(0)
        {
        }

        UTF8Cursor(const char* buffer, size_t bufferLength, size_t bytePosition, char16_t pendingTrail)
            : m_source(nullptr)
            , m_buffer(buffer)
            , m_bufferLength(bufferLength)
            , m_bytePosition(bytePosition)
            , m_pendingTrail(pendingTrail)
            , m_accessData(true, 0, nullptr)
            , m_spanEnd(bufferLength)
        {
        }

        bool atEnd() const
        {
            return !m_pendingTrail && m_bytePosition >= m_bufferLength;
        }

        size_t bytePosition() const
        {
            return m_bytePosition;
        }

        char16_t pendingTrail() const
        {
            return m_pendingTrail;
        }

        void skipASCII(size_t count)
        {
            ASSERT(!m_pendingTrail && count <= m_bufferLength - m_bytePosition);
            m_bytePosition += count;
        }

        char16_t next();

    private:
        void ensureBytes(size_t count);

        Optional<const ScriptSource*> m_source;
        Optional<const char*> m_buffer;
        size_t m_bufferLength;
        size_t m_bytePosition;
        char16_t m_pendingTrail;
        StringBufferAccessData m_accessData;
        size_t m_spanEnd;
    };

    // one entry of m_utf8Index per this many UTF-16 code units
    static const size_t utf8IndexGranularity = 512;
    static const size_t minimumCompressibleUTF8Length = 8 * 1024 * 1024;
    // the byte offset an entry holds is packed with the flag below, which caps the
    // size of a raw UTF-8 storage. a bigger source falls back to flat storage
    static const size_t maxUTF8StorageLength = 0x7fffffff;
    static const uint32_t utf8IndexPendingTrailFlag = 0x80000000;

    ScriptSource(SourceEncoding encoding, String* string, size_t storageLength)
        : m_encoding(encoding)
        , m_hasHashValue(false)
        , m_isReloadableUTF8(false)
        , m_utf8BufferIsOSAllocated(false)
        , m_storageLength(storageLength)
        , m_length(string->length())
        , m_hashValue(0)
        , m_string(string)
        , m_decodedString(nullptr)
        , m_utf8Data(nullptr)
        , m_utf8Index(nullptr)
        , m_utf8Storage(nullptr)
    {
        ASSERT(!!string);
    }

    ScriptSource(const char* utf8Data, size_t byteLength, size_t utf16Length, uint32_t* utf8Index)
        : m_encoding(SourceEncoding::UTF8)
        , m_hasHashValue(false)
        , m_isReloadableUTF8(false)
        , m_utf8BufferIsOSAllocated(false)
        , m_storageLength(byteLength)
        , m_length(utf16Length)
        , m_hashValue(0)
        , m_string(nullptr)
        , m_decodedString(nullptr)
        , m_utf8Data(utf8Data)
        , m_utf8Index(utf8Index)
        , m_utf8Storage(nullptr)
    {
    }

    UTF8Cursor cursorAt(size_t utf16Offset) const;
    String* decodeRange(size_t start, size_t end) const;
    static ScriptSource* createFromUTF8Buffer(char* bytes, size_t byteLength, Optional<String*> byteStorage,
                                              Optional<ReloadableSourceString*> reloadableStorage = nullptr);
    bool hasUTF8Storage() const;
    StringBufferAccessData utf8BufferAccessData(size_t start, size_t count) const;

    SourceEncoding m_encoding;
    bool m_hasHashValue;
    bool m_isReloadableUTF8;
    bool m_utf8BufferIsOSAllocated;
    size_t m_storageLength;
    size_t m_length;
    size_t m_hashValue;
    // Flat text or raw UTF-8; compactUTF8 can transfer raw bytes to the
    // compressed store after the first parse.
    Optional<String*> m_string;
    // weak cache of the whole text decoded out of m_utf8Data, deliberately left
    // out of the GC descriptor of this class so that it does not retain the text
    mutable Optional<String*> m_decodedString;
    Optional<const char*> m_utf8Data;
    // byte offset of every utf8IndexGranularity-th UTF-16 code unit of m_utf8Data
    Optional<uint32_t*> m_utf8Index;
    // A String of raw UTF-8 bytes or, when m_isReloadableUTF8 is set, a
    // ReloadableSourceString. Neither represents decoded JavaScript text.
    Optional<void*> m_utf8Storage;
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
    return source->substring(start, end);
}

inline StringView SourceRange::toStringView() const
{
    if (LIKELY(source->hasFlatString())) {
        return StringView(source->flatString(), start, end);
    }
    // the view covers the whole decoded range, its indices stay relative to `start`
    String* decoded = source->substring(start, end);
    return StringView(decoded, 0, decoded->length());
}

class ClassSourceText : public gc {
public:
    explicit ClassSourceText(SourceRange range)
        : m_range(range)
        , m_string(nullptr)
    {
        ASSERT(!!range.source);
    }

    explicit ClassSourceText(String* string)
        : m_range{ nullptr, 0, 0 }
        , m_string(string)
    {
        ASSERT(!!string);
    }

    String* string()
    {
        if (!m_string) {
            m_string = m_range.toString();
        }
        return m_string.value();
    }

private:
    SourceRange m_range;
    Optional<String*> m_string;
};

} // namespace Escargot

#endif
