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
#if defined(ENABLE_COMPRESSIBLE_STRING)
#include "runtime/CompressibleString.h"
#endif
#include "runtime/VMInstance.h"
#include "util/OSMemory.h"

namespace Escargot {

static inline bool isAllASCIIBytes(const char* bytes, size_t length)
{
    size_t i = 0;
    for (; i + sizeof(uint64_t) <= length; i += sizeof(uint64_t)) {
        uint64_t word;
        memcpy(&word, bytes + i, sizeof(word));
        if (word & UINT64_C(0x8080808080808080)) {
            return false;
        }
    }
    for (; i < length; i++) {
        if (static_cast<unsigned char>(bytes[i]) & 0x80) {
            return false;
        }
    }
    return true;
}

#if defined(ENABLE_RELOADABLE_STRING)
void* ReloadableSourceString::operator new(size_t size)
{
    static MAY_THREAD_LOCAL GC_descr descr = 0;
    if (!descr) {
        GC_word bitmap[GC_BITMAP_SIZE(ReloadableSourceString)] = { 0 };
        GC_set_bit(bitmap, GC_WORD_OFFSET(ReloadableSourceString, m_vmInstance));
        GC_set_bit(bitmap, GC_WORD_OFFSET(ReloadableSourceString, m_callbackData));
        GC_set_bit(bitmap, GC_WORD_OFFSET(ReloadableSourceString, m_buffer));
        descr = GC_make_descriptor(bitmap, GC_WORD_LEN(ReloadableSourceString));
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

ReloadableSourceString::ReloadableSourceString(VMInstance* instance, size_t byteLength, Optional<void*> callbackData,
                                               void* (*loadCallback)(void*), void (*unloadCallback)(void*, void*))
    : m_vmInstance(instance)
    , m_callbackData(callbackData)
    , m_buffer(nullptr)
    , m_byteLength(byteLength)
    , m_refCount(0)
    , m_isOwnerMayFreed(false)
    , m_loadCallback(loadCallback)
    , m_unloadCallback(unloadCallback)
{
    instance->reloadableSourceStrings().push_back(this);
    GC_REGISTER_FINALIZER_NO_ORDER(this, [](void* object, void*) {
        ReloadableSourceString* storage = static_cast<ReloadableSourceString*>(object);
        ASSERT(!storage->m_refCount);
        storage->unload();
        if (!storage->m_isOwnerMayFreed) {
            auto& list = storage->m_vmInstance->reloadableSourceStrings();
            auto it = std::find(list.begin(), list.end(), storage);
            if (it != list.end()) {
                list.erase(it);
            }
        } }, nullptr, nullptr, nullptr);
}

StringBufferAccessData ReloadableSourceString::bufferAccessData() const
{
    if (!m_buffer) {
        m_buffer = m_loadCallback(m_callbackData.unwrap());
        RELEASE_ASSERT(m_buffer);
    }
    return StringBufferAccessData(true, m_byteLength, m_buffer.value(), &m_refCount);
}

bool ReloadableSourceString::unload()
{
    if (!m_buffer || m_refCount) {
        return false;
    }
    m_unloadCallback(m_buffer.value(), m_callbackData.unwrap());
    m_buffer = nullptr;
    return true;
}
#endif

bool ScriptSource::hasUTF8Storage() const
{
    return m_utf8Data || m_utf8Storage;
}

StringBufferAccessData ScriptSource::utf8BufferAccessData(size_t start, size_t count) const
{
    ASSERT(m_utf8Storage);
#if defined(ENABLE_RELOADABLE_STRING)
    if (m_isReloadableUTF8) {
        return static_cast<const ReloadableSourceString*>(m_utf8Storage.value())->bufferAccessData();
    }
#endif
    return static_cast<const String*>(m_utf8Storage.value())->bufferAccessDataForRange(start, count);
}

void* ScriptSource::operator new(size_t size)
{
    static MAY_THREAD_LOCAL bool typeInited = false;
    static MAY_THREAD_LOCAL GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(ScriptSource)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ScriptSource, m_string));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ScriptSource, m_utf8Data));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ScriptSource, m_utf8Index));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ScriptSource, m_utf8Storage));
        // m_decodedString is a weak cache and is left out on purpose
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(ScriptSource));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

void ScriptSource::UTF8Cursor::ensureBytes(size_t count)
{
    if (!m_source) {
        return;
    }
    if (LIKELY(!!m_source->m_utf8Data)) {
        m_buffer = m_source->m_utf8Data.value();
        m_spanEnd = m_bufferLength;
        return;
    }
    ASSERT(m_source->hasUTF8Storage());
    size_t needed = std::min(count, m_bufferLength - m_bytePosition);
    if (m_buffer && m_bytePosition + needed <= m_spanEnd) {
        return;
    }
    size_t accessLength = std::min<size_t>(4096, m_bufferLength - m_bytePosition);
    accessLength = std::max(accessLength, needed);
    m_accessData = m_source->utf8BufferAccessData(m_bytePosition, accessLength);
    m_buffer = m_accessData.bufferAs8Bit;
    m_spanEnd = m_bytePosition + accessLength;
}

char16_t ScriptSource::UTF8Cursor::next()
{
    ASSERT(!atEnd());

    if (UNLIKELY(m_pendingTrail != 0)) {
        char16_t trail = m_pendingTrail;
        m_pendingTrail = 0;
        return trail;
    }

    ensureBytes(4);
    const char* sequence = m_buffer.value() + m_bytePosition;
    unsigned char lead = static_cast<unsigned char>(*sequence);
    if (LIKELY(lead < 0x80)) {
        m_bytePosition++;
        return lead;
    }

    bool valid;
    int charlen;
    char32_t ch = readUTF8Sequence(sequence, valid, charlen, m_bufferLength - m_bytePosition);
    size_t afterSequence = sequence - m_buffer.value();

    if (!valid) {
        m_bytePosition = afterSequence;
        return 0xFFFD;
    }
    if (static_cast<uint32_t>(ch) <= 0xffff) {
        if ((ch & 0xfffff800) == 0xd800) { // a surrogate is not a valid UTF-8 encoded character
            m_bytePosition = afterSequence - (charlen - 1);
            return 0xFFFD;
        }
        m_bytePosition = afterSequence;
        return static_cast<char16_t>(ch);
    }
    if (static_cast<uint32_t>(ch - 0x10000) <= 0xfffff) {
        m_bytePosition = afterSequence;
        m_pendingTrail = static_cast<char16_t>((ch & 0x3ff) | 0xdc00);
        return static_cast<char16_t>((ch >> 10) + 0xd7c0);
    }
    m_bytePosition = afterSequence - (charlen - 1);
    return 0xFFFD;
}

ScriptSource::UTF8Cursor ScriptSource::cursorAt(size_t utf16Offset) const
{
    ASSERT(hasUTF8Storage());
    ASSERT(utf16Offset <= m_length);

    size_t entry = utf16Offset / utf8IndexGranularity;
    uint32_t packed = m_utf8Index.value()[entry];
    UTF8Cursor cursor(this, packed & ~utf8IndexPendingTrailFlag,
                      (packed & utf8IndexPendingTrailFlag) ? 1 : 0);

    // a cursor sitting on a trail surrogate only has to know that it does, the value
    // itself comes back out of the lead it belongs to, which is re-read below
    if (UNLIKELY(cursor.pendingTrail() != 0)) {
        UTF8Cursor leadCursor(this, cursor.bytePosition() - 4, 0);
        leadCursor.next();
        cursor = leadCursor;
    }

    size_t remaining = utf16Offset - entry * utf8IndexGranularity;
    if (remaining && !cursor.pendingTrail() && remaining <= m_storageLength - cursor.bytePosition()) {
        if (m_utf8Data) {
            if (isAllASCIIBytes(m_utf8Data.value() + cursor.bytePosition(), remaining)) {
                cursor.skipASCII(remaining);
                return cursor;
            }
        } else {
            auto access = utf8BufferAccessData(cursor.bytePosition(), remaining);
            if (isAllASCIIBytes(access.bufferAs8Bit + cursor.bytePosition(), remaining)) {
                cursor.skipASCII(remaining);
                return cursor;
            }
        }
    }
    for (size_t i = 0; i < remaining; i++) {
        cursor.next();
    }
    return cursor;
}

String* ScriptSource::decodeRange(size_t start, size_t end) const
{
    ASSERT(start <= end && end <= m_length);

    size_t length = end - start;
    if (!length) {
        return String::emptyString();
    }

    auto ascii = asciiBytes(start, length);
    if (ascii) {
        return String::fromASCII(ascii.value(), length);
    }

    char16_t* buffer = static_cast<char16_t*>(GC_MALLOC_ATOMIC(sizeof(char16_t) * length));
    UTF8Cursor cursor = cursorAt(start);
    bool isAllLatin1 = true;
    for (size_t i = 0; i < length; i++) {
        char16_t c = cursor.next();
        buffer[i] = c;
        if (c >= 256) {
            isAllLatin1 = false;
        }
    }

    // a range of a mostly ASCII source is usually all ASCII, halve its storage
    String* result = isAllLatin1 ? String::fromLatin1(buffer, length) : new UTF16String(buffer, length);
    GC_FREE(buffer);
    return result;
}

String* ScriptSource::string() const
{
    if (LIKELY(!!m_string)) {
        return m_string.unwrap();
    }
    if (m_decodedString) {
        return m_decodedString.unwrap();
    }

    String* decoded = decodeRange(0, m_length);
    void* base = GC_base(decoded);
    if (LIKELY(!!base)) {
        m_decodedString = decoded;
        GC_general_register_disappearing_link(reinterpret_cast<void**>(&m_decodedString), base);
    }
    return decoded;
}

String* ScriptSource::substring(size_t start, size_t end) const
{
    ASSERT(start <= end && end <= m_length);

    if (LIKELY(!!m_string)) {
        if (start == 0 && end == m_length) {
            return m_string.unwrap();
        }
        return new StringView(m_string.unwrap(), start, end);
    }
    if (start == 0 && end == m_length) {
        return string();
    }
    return decodeRange(start, end);
}

char16_t ScriptSource::charAt(size_t utf16Offset) const
{
    ASSERT(utf16Offset < m_length);

    if (LIKELY(!!m_string)) {
        return m_string->charAt(utf16Offset);
    }
    return cursorAt(utf16Offset).next();
}

Optional<const char*> ScriptSource::asciiBytes(size_t start, size_t count) const
{
    ASSERT(start <= m_length && count <= m_length - start);
    if (!m_utf8Data) {
        // A parser keeps collection disabled while a window points into the
        // decompressed buffer. Reuse that buffer for ASCII windows as well.
        if (!hasUTF8Storage() || !GC_is_disabled()) {
            return nullptr;
        }
    }
    UTF8Cursor cursor = cursorAt(start);
    size_t byteStart = cursor.bytePosition();
    if (cursor.pendingTrail() || count > m_storageLength - byteStart) {
        return nullptr;
    }
    const char* bytes = m_utf8Data ? m_utf8Data.value() + byteStart : rawUTF8Data(byteStart, count);
    return isAllASCIIBytes(bytes, count) ? bytes : nullptr;
}

const char* ScriptSource::rawUTF8Data(size_t byteStart, size_t count) const
{
    ASSERT(byteStart <= m_storageLength && count <= m_storageLength - byteStart);
    if (LIKELY(!!m_utf8Data)) {
        return m_utf8Data.value() + byteStart;
    }
    // The scanner calls this only while collection is disabled. A returned
    // pointer remains valid for its token's lifetime even after this access
    // descriptor releases the compressor's reference count.
    ASSERT(GC_is_disabled());
    auto access = utf8BufferAccessData(byteStart, count);
    return access.bufferAs8Bit + byteStart;
}

void ScriptSource::compactUTF8(VMInstance* instance)
{
#if defined(ENABLE_RELOADABLE_STRING)
    if (m_isReloadableUTF8 && m_utf8Storage) {
        static_cast<ReloadableSourceString*>(m_utf8Storage.value())->unload();
        return;
    }
#endif
#if defined(ENABLE_COMPRESSIBLE_STRING)
    if (m_storageLength < minimumCompressibleUTF8Length) {
        return;
    }
    if (m_utf8Data) {
        // Transfer ownership without copying the source bytes.
        m_utf8Storage = new CompressibleString(instance, const_cast<char*>(m_utf8Data.value()), m_storageLength, true, m_utf8BufferIsOSAllocated);
        m_utf8Data = nullptr;
    }
    Optional<String*> storage = m_string;
    if (m_utf8Storage) {
        storage = static_cast<String*>(m_utf8Storage.value());
    }
    if (storage && storage.value()->isCompressibleString()) {
        CompressibleString* compressed = static_cast<CompressibleString*>(storage.value());
        if (!compressed->isCompressed()) {
            compressed->compress(16);
        }
    }
#else
    UNUSED_PARAMETER(instance);
#endif
}

void ScriptSource::copyUTF16(size_t start, size_t count, char16_t* buffer, uint32_t* byteOffsets) const
{
    ASSERT(start <= m_length && count <= m_length - start);
    if (LIKELY(!!m_string)) {
        for (size_t i = 0; i < count; i++) {
            buffer[i] = m_string->charAt(start + i);
        }
        return;
    }

    auto ascii = asciiBytes(start, count);
    if (ascii) {
        const unsigned char* bytes = reinterpret_cast<const unsigned char*>(ascii.value());
        for (size_t i = 0; i < count; i++) {
            buffer[i] = bytes[i];
        }
        if (byteOffsets) {
            size_t byteStart = ascii.value() - m_utf8Data.value();
            for (size_t i = 0; i <= count; i++) {
                byteOffsets[i] = static_cast<uint32_t>(byteStart + i);
            }
        }
        return;
    }
    UTF8Cursor cursor = cursorAt(start);
    for (size_t i = 0; i < count; i++) {
        if (byteOffsets) {
            byteOffsets[i] = static_cast<uint32_t>(cursor.bytePosition());
        }
        buffer[i] = cursor.next();
    }
    if (byteOffsets) {
        byteOffsets[count] = static_cast<uint32_t>(cursor.bytePosition());
    }
}

ScriptSource* ScriptSource::createFromASCII(const char* data, size_t length)
{
    return new ScriptSource(SourceEncoding::ASCII, String::fromASCII(data, length), length);
}

static bool useOSForUTF8Buffer(size_t length)
{
#if defined(OS_POSIX) || defined(OS_WINDOWS)
    return length >= 65536;
#else
    return false;
#endif
}

void* ScriptSource::allocateUTF8Buffer(size_t byteLength)
{
    if (useOSForUTF8Buffer(byteLength)) {
        return OSMemory::reserve(byteLength);
    }
    void* buffer = malloc(std::max<size_t>(byteLength, 1));
    RELEASE_ASSERT(buffer);
    return buffer;
}

void ScriptSource::deallocateUTF8Buffer(void* buffer, size_t byteLength)
{
    if (useOSForUTF8Buffer(byteLength)) {
        OSMemory::release(buffer, byteLength);
    } else {
        free(buffer);
    }
}

ScriptSource* ScriptSource::createFromUTF8(const char* data, size_t length)
{
    if (isAllASCIIBytes(data, length)) {
        return new ScriptSource(SourceEncoding::UTF8, String::fromASCII(data, length), length);
    }
    if (length > maxUTF8StorageLength) {
        auto decoded = utf8StringToUTF16String(data, length);
        return new ScriptSource(SourceEncoding::UTF8, new UTF16String(std::move(decoded)), length);
    }
    char* bytes = static_cast<char*>(allocateUTF8Buffer(length));
    memcpy(bytes, data, length);
    return createFromUTF8Buffer(bytes, length, nullptr);
}

ScriptSource* ScriptSource::createFromAlreadyAllocatedUTF8Buffer(VMInstance* instance, void* buffer, size_t byteLength)
{
    RELEASE_ASSERT(buffer);
    if (!byteLength) {
        deallocateUTF8Buffer(buffer, byteLength);
        return new ScriptSource(SourceEncoding::UTF8, String::emptyString(), 0);
    }
#if defined(ENABLE_COMPRESSIBLE_STRING)
    if (byteLength >= minimumCompressibleUTF8Length) {
        String* storage = new CompressibleString(instance, buffer, byteLength, true, useOSForUTF8Buffer(byteLength));
        auto access = storage->bufferAccessData();
        return createFromUTF8Buffer(const_cast<char*>(access.bufferAs8Bit), byteLength, storage);
    }
#else
    UNUSED_PARAMETER(instance);
#endif
    return createFromUTF8Buffer(static_cast<char*>(buffer), byteLength, nullptr);
}

ScriptSource* ScriptSource::createReloadableUTF8(VMInstance* instance, size_t byteLength, Optional<void*> callbackData,
                                                 void* (*loadCallback)(void*), void (*unloadCallback)(void*, void*))
{
#if defined(ENABLE_RELOADABLE_STRING)
    if (!byteLength) {
        return new ScriptSource(SourceEncoding::UTF8, String::emptyString(), 0);
    }
    RELEASE_ASSERT(loadCallback && unloadCallback);
    ReloadableSourceString* storage = new ReloadableSourceString(instance, byteLength, callbackData, loadCallback, unloadCallback);
    auto access = storage->bufferAccessData();
    return createFromUTF8Buffer(const_cast<char*>(access.bufferAs8Bit), byteLength, nullptr, storage);
#else
    RELEASE_ASSERT_NOT_REACHED();
#endif
}

ScriptSource* ScriptSource::createFromUTF8Buffer(char* bytes, size_t length, Optional<String*> byteStorage,
                                                 Optional<ReloadableSourceString*> reloadableStorage)
{
    if ((byteStorage && isAllASCIIBytes(bytes, length)) || length > maxUTF8StorageLength) {
        String* string = length <= maxUTF8StorageLength && byteStorage
            ? byteStorage.value()
            : String::fromUTF8(bytes, length);
        if (!byteStorage && !reloadableStorage) {
            deallocateUTF8Buffer(bytes, length);
        }
        return new ScriptSource(SourceEncoding::UTF8, string, length);
    }

    // A UTF-8 byte sequence produces at most one UTF-16 unit per byte. Reserve
    // that upper bound so length and sparse offsets can be found in one pass.
    size_t indexCapacity = length / utf8IndexGranularity + 1;
    uint32_t* index = static_cast<uint32_t*>(GC_MALLOC_ATOMIC(sizeof(uint32_t) * indexCapacity));
    UTF8Cursor cursor(bytes, length, 0, 0);
    index[0] = 0;
    size_t utf16Length = 0;
    while (!cursor.atEnd()) {
        if (!(utf16Length % utf8IndexGranularity) && !cursor.pendingTrail()
            && length - cursor.bytePosition() >= utf8IndexGranularity
            && isAllASCIIBytes(bytes + cursor.bytePosition(), utf8IndexGranularity)) {
            cursor.skipASCII(utf8IndexGranularity);
            utf16Length += utf8IndexGranularity;
        } else {
            cursor.next();
            utf16Length++;
        }
        if (!(utf16Length % utf8IndexGranularity)) {
            ASSERT(cursor.bytePosition() <= maxUTF8StorageLength);
            index[utf16Length / utf8IndexGranularity] = static_cast<uint32_t>(cursor.bytePosition()) | (cursor.pendingTrail() ? utf8IndexPendingTrailFlag : 0);
        }
    }

    ScriptSource* source = new ScriptSource(bytes, length, utf16Length, index);
#if defined(ENABLE_RELOADABLE_STRING)
    if (reloadableStorage) {
        source->m_utf8Storage = reloadableStorage.value();
        source->m_isReloadableUTF8 = true;
        source->m_utf8Data = nullptr;
    } else
#endif
        if (byteStorage) {
        source->m_utf8Storage = byteStorage.value();
        source->m_utf8Data = nullptr;
    } else {
        source->m_utf8BufferIsOSAllocated = useOSForUTF8Buffer(length);
        GC_REGISTER_FINALIZER_NO_ORDER(source, [](void* object, void*) {
            ScriptSource* source = static_cast<ScriptSource*>(object);
            if (source->m_utf8Data) {
                deallocateUTF8Buffer(const_cast<char*>(source->m_utf8Data.value()), source->m_storageLength);
            } }, nullptr, nullptr, nullptr);
    }
    return source;
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
        if (LIKELY(!!m_string)) {
            m_hashValue = m_string->hashValue<0, false>();
        } else {
            uint32_t hash = 0x811c9dc5;
            UTF8Cursor cursor(this, 0, 0);
            while (!cursor.atEnd()) {
                hash ^= cursor.next();
                hash *= 0x01000193;
            }
            m_hashValue = hash;
        }
        m_hasHashValue = true;
    }
    return m_hashValue;
}

} // namespace Escargot
