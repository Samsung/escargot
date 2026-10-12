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
#include "runtime/JSON.h"
#include "runtime/Context.h"
#include "runtime/StringObject.h"
#include "runtime/StringView.h"
#include "runtime/ArrayObject.h"
#include "runtime/TypedArrayObject.h"
#include "runtime/BooleanObject.h"
#include "runtime/BigIntObject.h"
#include "runtime/RawJSONObject.h"
#include "runtime/JSONStructureCache.h"
#include "util/CharacterSpan.h"
#include "runtime/VMInstance.h"

#define RAPIDJSON_PARSE_DEFAULT_FLAGS kParseFullPrecisionFlag
#define RAPIDJSON_ERROR_CHARTYPE char
#include <rapidjson/document.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/memorystream.h>
#include <rapidjson/internal/dtoa.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <rapidjson/error/en.h>

#include <simdutf.h>

namespace Escargot {

#if !defined(ESCARGOT_SMALL_CONFIG)
// JavaScript one-byte strings contain Latin1, not UTF-8. Keep both raw
// characters and Latin1 escapes in a one-byte RapidJSON reader and DOM.
struct JSONLatin1Encoding : rapidjson::ASCII<LChar> {
    template <typename OutputStream>
    static bool Encode(OutputStream& output, unsigned codepoint)
    {
        if (UNLIKELY(codepoint > 0xFF)) {
            return false;
        }
        output.Put(static_cast<LChar>(codepoint));
        return true;
    }

    template <typename InputStream>
    static bool Decode(InputStream& input, unsigned* codepoint)
    {
        *codepoint = input.Take();
        return true;
    }

    template <typename InputStream, typename OutputStream>
    static bool Validate(InputStream& input, OutputStream& output)
    {
        output.Put(input.Take());
        return true;
    }
};

template <typename OutputStream>
inline bool EncodeJSONCodePoint(JSONLatin1Encoding, OutputStream& output, unsigned codepoint)
{
    return JSONLatin1Encoding::Encode(output, codepoint);
}
#endif

template <typename CharType>
static inline void copyJSONCharacters(const CharType* src, CharType* dst, size_t count)
{
    memmove(dst, src, count * sizeof(CharType));
}

static inline void copyJSONCharacters(const LChar* src, char16_t* dst, size_t count)
{
    if (count >= 32) {
        const size_t written = simdutf::convert_latin1_to_utf16(reinterpret_cast<const char*>(src), count, dst);
        ASSERT(written == count);
    } else {
        for (size_t i = 0; i < count; i++) {
            dst[i] = src[i];
        }
    }
}

static inline void copyJSONCharacters(const char16_t* src, LChar* dst, size_t count)
{
    if (count >= 32) {
        const size_t written = simdutf::convert_utf16_to_latin1(src, count, reinterpret_cast<char*>(dst));
        ASSERT(written == count);
    } else {
        for (size_t i = 0; i < count; i++) {
            dst[i] = src[i];
        }
    }
}

template <typename SourceCharType, typename OutputCharType = char16_t>
struct JSONStringStream {
    using Ch = OutputCharType;
    static_assert(std::is_same<Ch, LChar>::value || std::is_same<Ch, char16_t>::value, "Expected Latin1 or UTF-16 output");
    static_assert(sizeof(SourceCharType) <= sizeof(Ch), "Cannot narrow raw input code units");

    JSONStringStream(const SourceCharType* src, size_t length, Ch* output)
        : src_(src)
        , head_(src)
        , tail_(src + length)
        , dst_(output)
        , outputHead_(output)
    {
    }

    Ch Peek() const
    {
        if (UNLIKELY(tail_ <= src_)) {
            return 0;
        }
        return *src_;
    }
    Ch Take()
    {
        if (UNLIKELY(tail_ <= src_)) {
            return 0;
        }
        return *src_++;
    }
    size_t Tell() const { return static_cast<size_t>(src_ - head_); }
    Ch* PutBegin() { return dst_; }
    void Put(Ch c)
    {
        ASSERT(static_cast<size_t>(dst_ - outputHead_) < Tell());
        *dst_++ = c;
    }
    void Flush() {}
    size_t PutEnd(Ch* begin) { return static_cast<size_t>(dst_ - begin); }

    void scanUnescapedString()
    {
        // Decode short spans in one pass, without scanning then copying the
        // same code units. Long spans use the shared word scan and converter.
        const size_t prefixLength = std::min(static_cast<size_t>(tail_ - src_), size_t(8));
        size_t prefix = 0;
        while (prefix < prefixLength) {
            const Ch c = src_[prefix];
            if (c == '"' || c == '\\' || c < 0x20) {
                break;
            }
            dst_[prefix] = c;
            prefix++;
        }
        src_ += prefix;
        dst_ += prefix;
        ASSERT(static_cast<size_t>(dst_ - outputHead_) <= Tell());
        if (prefix < prefixLength || prefixLength < 8) {
            return;
        }
        const size_t count = findFirstOfOrLessThan(src_, static_cast<size_t>(tail_ - src_),
                                                   SourceCharType('"'), SourceCharType('\\'), SourceCharType(0x20));
        copyUnescaped(count);
    }

    void copyUnescaped(size_t count)
    {
        if (!count) {
            return;
        }
        copyJSONCharacters(src_, dst_, count);
        src_ += count;
        dst_ += count;
        ASSERT(static_cast<size_t>(dst_ - outputHead_) <= Tell());
    }

    const SourceCharType* src_; //!< Current read position.
    const SourceCharType* head_; //!< Original head of the string.
    const SourceCharType* tail_;
    Ch* dst_; //!< Packed decoded strings in the owned output buffer.
    Ch* outputHead_;
};

template <typename SourceCharType, typename OutputCharType>
inline void ScanCopyUnescapedString(JSONStringStream<SourceCharType, OutputCharType>& input, JSONStringStream<SourceCharType, OutputCharType>& output)
{
    ASSERT(&input == &output);
    input.scanUnescapedString();
}

template <typename JSONCharType>
class JSONDocument : public rapidjson::GenericDocument<JSONCharType> {
public:
    JSONDocument(ExecutionState& state)
        : m_state(state)
    {
    }

    bool StartObject()
    {
        CHECK_STACK_OVERFLOW(m_state);
        return rapidjson::GenericDocument<JSONCharType>::StartObject();
    }

    bool StartArray()
    {
        CHECK_STACK_OVERFLOW(m_state);
        return rapidjson::GenericDocument<JSONCharType>::StartArray();
    }

private:
    ExecutionState& m_state;
};

struct JSONKey {
    const void* characters;
    size_t length;
    bool has8BitContent;

    char16_t charAt(size_t index) const
    {
        return has8BitContent ? static_cast<const LChar*>(characters)[index] : static_cast<const char16_t*>(characters)[index];
    }

    AtomicString intern(ExecutionState& state, std::basic_string<LChar>& latin1Key) const
    {
        if (has8BitContent) {
            const auto* chars = static_cast<const LChar*>(characters);
            if (isAllASCII(reinterpret_cast<const char*>(chars), length)) {
                return AtomicString(state.context(), reinterpret_cast<const char*>(chars), length);
            }
            return AtomicString(state.context(), chars, length);
        }
        const auto* chars = static_cast<const char16_t*>(characters);
        // Match ordinary Latin1 property-name lookup, including non-ASCII keys.
        if (!isAllASCII(chars, length) && isAllLatin1(chars, length)) {
            latin1Key.resize(length);
            for (size_t i = 0; i < length; ++i) {
                latin1Key[i] = static_cast<LChar>(chars[i]);
            }
            return AtomicString(state.context(), latin1Key.data(), length);
        }
        return AtomicString(state, chars, length);
    }
};

template <typename KeyAt, typename ValueAt>
static Object* createJSONObject(ExecutionState& state, size_t memberCount, KeyAt&& keyAt, ValueAt&& valueAt)
{
    if (!memberCount) {
        return Object::createWithStructure(state, state.context()->defaultStructureForObject(), ObjectPropertyValueVector());
    }

    // Hash decoded keys before interning them. The hash is only a filter:
    // a candidate must also match every key's length and code units.
    size_t signature = memberCount;
    size_t keyBytes = 0;
    for (size_t ordinal = 0; ordinal < memberCount; ++ordinal) {
        const auto key = keyAt(ordinal);
        const size_t length = key.length;
        signature = (signature ^ length) * 16777619;
        for (size_t i = 0; i < length; ++i) {
            signature = (signature ^ key.charAt(i)) * 16777619;
        }
        // Saturate the charge rather than overflowing on a large input.
        if (length > (JSONStructureCache::MaxEntryBytes - keyBytes) / sizeof(char16_t)) {
            keyBytes = JSONStructureCache::MaxEntryBytes;
        } else {
            keyBytes += length * sizeof(char16_t);
        }
    }

    auto* cache = state.context()->vmInstance()->jsonStructureCache();
    Optional<JSONStructureCache::Entry*> cached = cache->find(signature, memberCount, [&](const JSONStructureCache::Entry& entry) {
        for (size_t ordinal = 0; ordinal < memberCount; ++ordinal) {
            const auto key = keyAt(ordinal);
            const size_t length = key.length;
            if (entry.m_keys[ordinal]->length() != length) {
                return false;
            }
            const auto chars = entry.m_keys[ordinal]->bufferAccessData();
            for (size_t i = 0; i < length; ++i) {
                if (chars.charAt(i) != key.charAt(i)) {
                    return false;
                }
            }
        }
        return true;
    });
    if (cached) {
        ObjectPropertyValueVector values;
        values.resize(0, cached.value()->m_structure->propertyCount(), Value());
        for (size_t ordinal = 0; ordinal < memberCount; ++ordinal) {
            values[cached.value()->m_valueIndexes[ordinal]] = valueAt(ordinal);
        }
        // Keep the entry rooted through recursive conversions, which can
        // evict it from the cache while the parent is being materialized.
        return Object::createWithStructure(state, cached.value()->m_structure, std::move(values));
    }

    JSONStructureCache::Entry::KeyVector keys;
    JSONStructureCache::Entry::IndexVector valueIndexes;
    keys.reserve(memberCount);
    valueIndexes.resize(memberCount);
    std::unordered_map<String*, size_t> uniqueKeys;
    std::basic_string<LChar> latin1Key;
    size_t keyOrdinal = 0;
    size_t uniqueKeyCount = 0;
    for (size_t ordinal = 0; ordinal < memberCount; ++ordinal) {
        AtomicString name = keyAt(ordinal).intern(state, latin1Key);
        size_t slot;
        if (memberCount <= 16) {
            // Small schemas do not need a heap-allocated hash node for
            // every key when the layout cache misses or is full.
            slot = uniqueKeyCount;
            for (size_t i = 0; i < keyOrdinal; ++i) {
                if (keys[i] == name.string()) {
                    slot = valueIndexes[i];
                    break;
                }
            }
            uniqueKeyCount += slot == uniqueKeyCount;
        } else {
            slot = uniqueKeys.emplace(name.string(), uniqueKeys.size()).first->second;
            uniqueKeyCount = uniqueKeys.size();
        }
        keys.push_back(name.string());
        valueIndexes[keyOrdinal++] = slot;
    }
    ObjectStructureItemTightVector properties;
    properties.resizeWithUninitializedValues(uniqueKeyCount);
    bool hasIndexedProperties = false;
    for (size_t i = 0; i < memberCount; ++i) {
        ObjectStructurePropertyName name(AtomicString::fromPayload(keys[i]));
        hasIndexedProperties |= name.isIndexString();
        properties[valueIndexes[i]] = ObjectStructureItem(name, ObjectStructurePropertyDescriptor::createDataDescriptor());
    }

    ObjectStructure* structure = ObjectStructure::create(state.context(), std::move(properties), true);
    ObjectPropertyValueVector values;
    values.resize(0, structure->propertyCount(), Value());
    for (size_t i = 0; hasIndexedProperties && i < memberCount; ++i) {
        // Indexed names are partitioned and sorted by structure creation;
        // map the original member order to the final storage slots.
        auto slot = structure->findProperty(ObjectStructurePropertyName(AtomicString::fromPayload(keys[i])));
        ASSERT(slot.second);
        valueIndexes[i] = slot.first;
    }
    for (size_t ordinal = 0; ordinal < memberCount; ++ordinal) {
        // Duplicate names reuse their first insertion slot and later
        // values overwrite earlier ones, including above transition size.
        values[valueIndexes[ordinal]] = valueAt(ordinal);
    }
    Object* obj = Object::createWithStructure(state, structure, std::move(values));

    const size_t metadataBytes = sizeof(JSONStructureCache::Entry) + 256;
    const size_t propertyBytes = 2 * sizeof(ObjectStructureItem) + 4 * sizeof(size_t);
    size_t charge = JSONStructureCache::MaxEntryBytes;
    if (keyBytes < JSONStructureCache::MaxEntryBytes
        && structure->propertyCount() <= JSONStructureCache::MaxEntryBytes / propertyBytes
        && keys.capacity() <= JSONStructureCache::MaxEntryBytes / sizeof(String*)
        && valueIndexes.capacity() <= JSONStructureCache::MaxEntryBytes / sizeof(size_t)) {
        charge = metadataBytes + keyBytes + structure->propertyCount() * propertyBytes
            + keys.capacity() * sizeof(String*) + valueIndexes.capacity() * sizeof(size_t);
    }
    if (charge < JSONStructureCache::MaxEntryBytes && cache->shouldInsert(structure, charge)) {
        cache->insert(new JSONStructureCache::Entry(structure, std::move(keys), std::move(valueIndexes), signature, charge));
    }
    return obj;
}

template <typename CharType, typename JSONCharType>
static Value parseJSONWorker(ExecutionState& state, const rapidjson::GenericValue<JSONCharType>& value)
{
    CHECK_STACK_OVERFLOW(state);
    if (value.IsBool()) {
        return Value(value.GetBool());
    } else if (value.IsInt()) {
        return Value(value.GetInt());
    } else if (value.IsUint()) {
        return Value(value.GetUint());
    } else if (value.IsInt64()) {
        return Value(value.GetInt64());
    } else if (value.IsUint64()) {
        return Value(value.GetUint64());
    } else if (value.IsDouble()) {
        return Value(Value::DoubleToIntConvertibleTestNeeds, value.GetDouble());
    } else if (value.IsNull()) {
        return Value(Value::Null);
    } else if (value.IsString()) {
        if (std::is_same<CharType, char16_t>::value) {
            const char16_t* chars = (const char16_t*)value.GetString();
            unsigned len = value.GetStringLength();
            if (isAllLatin1(chars, len)) {
                return String::fromLatin1(chars, len, state.context(), String::ShortStringCacheAdmission::MostRecentlyUsed, String::AtomicStringLookup::Skip);
            } else {
                return new UTF16String(chars, len);
            }
        } else {
            return String::fromLatin1(reinterpret_cast<const LChar*>(value.GetString()), value.GetStringLength(), state.context(), String::ShortStringCacheAdmission::MostRecentlyUsed, String::AtomicStringLookup::Skip);
        }
    } else if (value.IsArray()) {
        ArrayObject* arr = new ArrayObject(state, value.Size(), false);
        for (size_t i = 0; i < value.Size(); i++) {
            arr->defineOwnIndexedPropertyWithoutExpanding(state, i, parseJSONWorker<CharType, JSONCharType>(state, value[i]));
        }
        return arr;
    } else if (value.IsObject()) {
        auto keyAt = [&](size_t ordinal) {
            const auto& name = (value.MemberBegin() + ordinal)->name;
            return JSONKey{ name.GetString(), name.GetStringLength(), sizeof(CharType) == 1 };
        };
        auto valueAt = [&](size_t ordinal) { return parseJSONWorker<CharType, JSONCharType>(state, (value.MemberBegin() + ordinal)->value); };
        return createJSONObject(state, value.MemberCount(), keyAt, valueAt);
    } else {
        ASSERT_UNREACHABLE();
    }
}

template <typename CharType, typename JSONCharType>
static bool parseJSONDocument(ExecutionState& state, const CharType* data, size_t length, typename JSONCharType::Ch* output, JSONDocument<JSONCharType>& jsonDocument)
{
    auto strings = &state.context()->staticStrings();

    using Stream = JSONStringStream<CharType, typename JSONCharType::Ch>;
    Stream stringStream(data, length, output);
    jsonDocument.template ParseStream<rapidjson::kParseDefaultFlags | rapidjson::kParseInsituFlag, JSONCharType, Stream, JSONDocument<JSONCharType>>(stringStream);
    if (jsonDocument.HasParseError()) {
#if !defined(ESCARGOT_SMALL_CONFIG)
        // Our DOM handler accepts every event. Only an unrepresentable Unicode
        // escape terminates the Latin1 reader; retry it before materialization.
        if (std::is_same<JSONCharType, JSONLatin1Encoding>::value && jsonDocument.GetParseError() == rapidjson::kParseErrorTermination) {
            return false;
        }
#endif
        ErrorObject::throwBuiltinError(state, ErrorCode::SyntaxError, strings->JSON.string(), true, strings->parse.string(), rapidjson::GetParseError_En(jsonDocument.GetParseError()));
    }
    return true;
}

#if !defined(ESCARGOT_SMALL_CONFIG)
// Borrow unescaped strings from the original input. Reader's in-situ callbacks
// select this sink without modifying the input. On the first escape, decode
// only this token into reusable storage, widening it if a Unicode escape needs
// UTF16. Only our handler consumes these callbacks and the stream's metadata.
template <typename CharType>
struct JSONSAXStream {
    static constexpr size_t OwnedTokenThreshold = 8192;
    using Ch = CharType;

    JSONSAXStream(const Ch* input, size_t length)
        : current(input)
        , begin(input)
        , end(input + length)
        , tokenBegin(input)
    {
    }

    Ch Peek() const { return current < end ? *current : 0; }
    Ch Take() { return current < end ? *current++ : 0; }
    size_t Tell() const { return current - begin; }
    Ch* PutBegin()
    {
        tokenBegin = current + 1; // Reader has not consumed the opening quote.
        decodedUnits = 0;
        decoding = false;
        wide = false;
        hasNonLatin1 = false;
        return const_cast<Ch*>(tokenBegin);
    }
    void Put(Ch c)
    {
        // Reader's terminating NUL is metadata, not part of the string. An
        // escaped NUL ends in a hex digit and therefore takes the decode path.
        if (c == 0 && current[-1] == '"') {
            ++decodedUnits;
            return;
        }
        beginDecoding(c > 0xFF);
        if (wide) {
            ensureSpace(m_utf16, decodedUnits + 1);
            m_utf16[decodedUnits] = c;
        } else {
            ensureSpace(m_latin1, decodedUnits + 1);
            m_latin1[decodedUnits] = c;
        }
        ++decodedUnits;
    }
    void Flush() {}
    size_t PutEnd(Ch*)
    {
        ASSERT(decodedUnits && current > tokenBegin);
        rawLength = current - tokenBegin - 1; // Exclude the closing quote.
        return decodedUnits; // Includes Reader's terminating NUL.
    }
    void putCodePoint(unsigned codepoint)
    {
        beginDecoding(codepoint > 0xFF);
        if (!wide) {
            ensureSpace(m_latin1, decodedUnits + 1);
            m_latin1[decodedUnits++] = static_cast<LChar>(codepoint);
        } else {
            ensureSpace(m_utf16, decodedUnits + 2);
            if (codepoint <= 0xFFFF) {
                m_utf16[decodedUnits++] = static_cast<char16_t>(codepoint);
            } else {
                const unsigned value = codepoint - 0x10000;
                m_utf16[decodedUnits++] = (value >> 10) | 0xD800;
                m_utf16[decodedUnits++] = (value & 0x3FF) | 0xDC00;
            }
        }
    }
    void scanUnescapedString()
    {
        uintptr_t codeUnits = 0;
        const size_t remaining = end - current;
        const size_t count = sizeof(Ch) == 1
            ? findFirstOfOrLessThan(current, remaining, Ch('"'), Ch('\\'), Ch(0x20))
            : findFirstOfOrLessThan(current, remaining, Ch('"'), Ch('\\'), Ch(0x20), codeUnits);
        constexpr uintptr_t highBytes = (std::numeric_limits<uintptr_t>::max() / 0xFFFF) * 0xFF00;
        const bool needsWide = sizeof(Ch) != 1 && (codeUnits & highBytes);
        hasNonLatin1 |= needsWide;
        if (decoding && count) {
            if (needsWide && !wide) {
                beginDecoding(true);
            }
            if (wide) {
                ensureSpace(m_utf16, decodedUnits + count);
                copyJSONCharacters(current, m_utf16.data() + decodedUnits, count);
            } else {
                ensureSpace(m_latin1, decodedUnits + count);
                copyJSONCharacters(current, m_latin1.data() + decodedUnits, count);
            }
        }
        current += count;
        decodedUnits += count;
    }
    size_t length() const { return decodedUnits - 1; }
    bool has8BitContent() const { return decoding ? !wide : sizeof(Ch) == 1; }
    bool contentIsLatin1() const { return !hasNonLatin1 && !wide; }
    const void* characters() const
    {
        if (!decoding) {
            return tokenBegin;
        }
        return wide ? static_cast<const void*>(m_utf16.data()) : static_cast<const void*>(m_latin1.data());
    }
    Optional<String*> takeLatin1String() { return m_latin1.takeString(length()); }
    Optional<String*> takeUTF16String() { return m_utf16.takeString(length()); }
    void finishBuffers()
    {
        m_latin1.trimExcessCapacity();
        m_utf16.trimExcessCapacity();
    }

private:
    template <typename T>
    class TokenBuffer {
    public:
        TokenBuffer()
            : m_data(m_inline)
        {
        }
        TokenBuffer(const TokenBuffer&) = delete;
        TokenBuffer& operator=(const TokenBuffer&) = delete;
        ~TokenBuffer()
        {
            if (m_external && !m_owner) {
                GC_FREE(m_external.value());
            }
        }

        T* data() { return m_data; }
        const T* data() const { return m_data; }
        T& operator[](size_t index) { return data()[index]; }
        Optional<String*> takeString(size_t length)
        {
            ASSERT(length <= m_capacity);
            if (!m_external) {
                return NullOption;
            }
            m_data[length] = 0;
            if (!m_owner) {
                m_owner = sizeof(T) == 1
                    ? static_cast<String*>(new Latin1String(Latin1StringData::adoptBuffer(reinterpret_cast<LChar*>(m_external.value()), m_capacity)))
                    : static_cast<String*>(new UTF16String(UTF16StringData::adoptBuffer(reinterpret_cast<char16_t*>(m_external.value()), m_capacity)));
            }
            const size_t offset = m_data - m_external.value();
            auto* string = new StringView(m_owner.value(), offset, offset + length);
            // Published slices are immutable. Decode the next token only into
            // the unused tail; the owner keeps the allocation base GC-visible.
            if (length < m_capacity) {
                m_data += length + 1;
                m_capacity -= length + 1;
            } else {
                m_data += length;
                m_capacity = 0;
            }
            return string;
        }

        void trimExcessCapacity()
        {
            if (!m_owner) {
                return;
            }
            const size_t used = m_data - m_external.value();
            const size_t capacity = m_owner.value()->length();
            // Ordinary escape overhead does not justify copying the buffer.
            // Trim only a large allocation that is more than seven eighths unused.
            if (capacity <= (64 * 1024) / sizeof(T) || used > capacity / 8) {
                return;
            }
            Optional<T*> buffer(static_cast<T*>(GC_REALLOC(m_external.value(), (used + 1) * sizeof(T))));
            if (!buffer) {
                return; // A failed optional shrink leaves the original buffer valid.
            }
            if (sizeof(T) == 1) {
                auto data = Latin1StringData::adoptBuffer(reinterpret_cast<LChar*>(buffer.value()), used);
                static_cast<Latin1String*>(m_owner.value())->initBufferAccessData(data);
            } else {
                auto data = UTF16StringData::adoptBuffer(reinterpret_cast<char16_t*>(buffer.value()), used);
                static_cast<UTF16String*>(m_owner.value())->initBufferAccessData(data);
            }
            m_external = buffer;
            m_data = buffer.value() + used;
            m_capacity = 0;
        }

        void ensureSpace(size_t size, size_t preserved, size_t limit)
        {
            if (UNLIKELY(size > m_capacity)) {
                grow(size, preserved, limit);
            }
        }

    private:
        NEVER_INLINE void grow(size_t size, size_t preserved, size_t limit)
        {
            if (size <= OwnedTokenThreshold) {
                if (!m_native.size()) {
                    m_native.resize(OwnedTokenThreshold);
                }
                if (preserved) {
                    copyJSONCharacters(m_data, m_native.data(), preserved);
                }
                if (m_external && !m_owner) {
                    GC_FREE(m_external.value());
                }
                m_owner = NullOption;
                m_external = NullOption;
                m_data = m_native.data();
                m_capacity = OwnedTokenThreshold;
                return;
            }
            // The remaining input is an upper bound for all subsequent decoded
            // tokens. Reserve it once and share the unused tail, without a
            // length prepass or geometric GC-buffer growth.
            const size_t capacity = std::max(size, limit);
            const size_t bytes = (capacity + 1) * sizeof(T);
            Optional<T*> buffer(static_cast<T*>(m_external && !m_owner
                                                    ? GC_REALLOC_NO_SHRINK(m_external.value(), bytes)
                                                    : GC_MALLOC_ATOMIC(bytes)));
            if (!buffer) {
                throw std::bad_alloc();
            }
            if ((!m_external || m_owner) && preserved) {
                copyJSONCharacters(m_data, buffer.value(), preserved);
            }
            m_owner = NullOption;
            m_external = buffer;
            m_data = buffer.value();
            m_capacity = capacity;
        }

        static constexpr size_t InlineCapacity = 64;
        T m_inline[InlineCapacity + 1];
        T* m_data;
        size_t m_capacity{ InlineCapacity };
        Optional<T*> m_external;
        Optional<String*> m_owner;
        Vector<T, std::allocator<T>> m_native;
    };

    template <typename Buffer>
    void ensureSpace(Buffer& buffer, size_t size, bool preserve = true)
    {
        // A Unicode escape may already be consumed while its two output units
        // are still pending. Include them in the reservation upper bound.
        buffer.ensureSpace(size, preserve ? decodedUnits : 0, decodedUnits + (end - current) + 2);
    }
    void beginDecoding(bool needsWide = false)
    {
        if (!decoding) {
            wide = hasNonLatin1 || needsWide;
            if (wide) {
                ensureSpace(m_utf16, decodedUnits + 2, false);
                copyJSONCharacters(tokenBegin, m_utf16.data(), decodedUnits);
            } else {
                ensureSpace(m_latin1, decodedUnits + 1, false);
                copyJSONCharacters(tokenBegin, m_latin1.data(), decodedUnits);
            }
            decoding = true;
        } else if (needsWide && !wide) {
            ensureSpace(m_utf16, decodedUnits + 2, false);
            copyJSONCharacters(m_latin1.data(), m_utf16.data(), decodedUnits);
            wide = true;
        }
    }
    // Inline tokens need no allocation; medium tokens reuse native scratch.
    // Large tokens share GC-owned output through StringViews. Reserve from the
    // known input bound and leave room for each terminating NUL.
    TokenBuffer<LChar> m_latin1;
    TokenBuffer<char16_t> m_utf16;

public:
    const Ch* current;
    const Ch* const begin;
    const Ch* const end;
    const Ch* tokenBegin;
    size_t decodedUnits{ 0 };
    size_t rawLength{ 0 };
    bool decoding{ false };
    bool wide{ false };
    bool hasNonLatin1{ false };
};

template <typename CharType>
inline void ScanCopyUnescapedString(JSONSAXStream<CharType>& input, JSONSAXStream<CharType>& output)
{
    ASSERT(&input == &output);
    input.scanUnescapedString();
}

inline bool EncodeJSONCodePoint(JSONLatin1Encoding, JSONSAXStream<LChar>& output, unsigned codepoint)
{
    output.putCodePoint(codepoint);
    return true;
}

inline bool EncodeJSONCodePoint(rapidjson::UTF16<char16_t>, JSONSAXStream<char16_t>& output, unsigned codepoint)
{
    output.putCodePoint(codepoint);
    return true;
}

// Integers need no full-precision digit backup. Borrow their input span and
// materialize only the mantissa digits when RapidJSON converts a double.
template <typename Ch, typename DefaultNumberStream>
class JSONNumberStream {
public:
    using Reader = typename DefaultNumberStream::ReaderType;

    JSONNumberStream(Reader& reader, JSONSAXStream<Ch>& input)
        : m_reader(reader)
        , m_input(input)
        , m_numberBegin(input.current)
        , m_skippedUnits(0)
    {
    }

    Ch Peek() const { return m_input.Peek(); }
    Ch TakePush() { return m_input.Take(); }
    Ch Take()
    {
        ++m_skippedUnits;
        return m_input.Take();
    }
    size_t Tell() { return m_input.Tell(); }
    size_t Length() { return static_cast<size_t>(m_input.current - m_numberBegin) - m_skippedUnits; }

    const char* Pop(bool needsDigits)
    {
        return needsDigits ? materializeDigits() : "";
    }

private:
    NEVER_INLINE const char* materializeDigits()
    {
        DefaultNumberStream backup(m_reader, m_input);
        const Ch* numberEnd = m_input.current;
        m_input.current = m_numberBegin;
        while (m_input.current < numberEnd) {
            const Ch c = m_input.Peek();
            if (c == 'e' || c == 'E') {
                break;
            }
            if (c >= '0' && c <= '9') {
                backup.TakePush();
            } else {
                ASSERT(c == '-' || c == '.');
                m_input.Take();
            }
        }
        m_input.current = numberEnd;
        ASSERT(backup.Length() == Length());
        return backup.Pop(true);
    }

    Reader& m_reader;
    JSONSAXStream<Ch>& m_input;
    const Ch* m_numberBegin;
    size_t m_skippedUnits;
};

} // namespace Escargot

namespace rapidjson {
namespace internal {
template <typename Ch, typename DefaultNumberStream>
struct NumberStreamAdapter<Escargot::JSONSAXStream<Ch>, DefaultNumberStream> {
    typedef Escargot::JSONNumberStream<Ch, DefaultNumberStream> Type;
};
} // namespace internal
} // namespace rapidjson

namespace Escargot {

template <typename T>
static inline void clearJSONSAXSlot(T& slot)
{
    slot = T();
}

static inline void clearJSONSAXSlot(JSONKey& slot)
{
    slot = JSONKey{ "", 0, true };
}

template <size_t InlineCapacity, typename T, typename Allocator>
class JSONSAXStack {
public:
    T* data() { return m_external.size() ? m_external.data() : m_inline; }
    const T* data() const { return m_external.size() ? m_external.data() : m_inline; }
    T& operator[](size_t index) { return data()[index]; }
    const T& operator[](size_t index) const { return data()[index]; }

    void put(size_t index, const T& value)
    {
        if (LIKELY(!m_external.size() && index < InlineCapacity)) {
            m_inline[index] = value;
            return;
        }
        grow(index + 1);
        m_external[index] = value;
    }

private:
    void grow(size_t size)
    {
        if (size <= m_external.size()) {
            return;
        }
        const bool wasInline = !m_external.size();
        m_external.resize(size);
        if (wasInline) {
            // The first spill occurs when every inline slot is active. The
            // external allocator traces Values or pointers as appropriate.
            for (size_t i = 0; i < InlineCapacity; ++i) {
                m_external[i] = m_inline[i];
                clearJSONSAXSlot(m_inline[i]);
            }
        }
    }

    // Only active slots are read. Values initialize themselves; scalar frames
    // and key slices do not need to initialize unused inline slots.
    T m_inline[InlineCapacity];
    Vector<T, Allocator, ComputeReservedCapacityFunctionWithLog2<>> m_external;
};

template <typename CharType>
class JSONSAXHandler {
public:
    MAKE_STACK_ALLOCATED();

    JSONSAXHandler(ExecutionState& state, JSONSAXStream<CharType>& stream)
        : m_state(state)
        , m_stream(stream)
    {
    }

    bool Null() { return append(Value(Value::Null)); }
    bool Bool(bool value) { return append(Value(value)); }
    bool Int(int value) { return append(Value(value)); }
    bool Uint(unsigned value) { return append(Value(value)); }
    bool Int64(int64_t value) { return append(Value(value)); }
    bool Uint64(uint64_t value) { return append(Value(value)); }
    bool Double(double value) { return append(Value(Value::DoubleToIntConvertibleTestNeeds, value)); }

    bool String(const CharType*, rapidjson::SizeType, bool)
    {
        Escargot::String* string;
        const auto length = m_stream.length();
        if (m_stream.has8BitContent()) {
            const auto* chars = static_cast<const LChar*>(m_stream.characters());
            auto owned = m_stream.decoding && length > JSONSAXStream<CharType>::OwnedTokenThreshold ? m_stream.takeLatin1String() : Optional<Escargot::String*>(NullOption);
            string = owned ? owned.value()
                           : Escargot::String::fromLatin1(chars, length, m_state.context(), String::ShortStringCacheAdmission::MostRecentlyUsed, String::AtomicStringLookup::Skip);
        } else {
            const auto* chars = static_cast<const char16_t*>(m_stream.characters());
            auto owned = m_stream.decoding && length > JSONSAXStream<CharType>::OwnedTokenThreshold ? m_stream.takeUTF16String() : Optional<Escargot::String*>(NullOption);
            if (owned) {
                string = owned.value();
            } else {
                string = m_stream.contentIsLatin1() ? Escargot::String::fromLatin1(chars, length, m_state.context(), String::ShortStringCacheAdmission::MostRecentlyUsed, String::AtomicStringLookup::Skip) : new UTF16String(chars, length);
            }
        }
        append(Value(string));
        return true;
    }

    bool Key(const CharType*, rapidjson::SizeType, bool)
    {
        JSONKey key{ m_stream.characters(), m_stream.length(), m_stream.has8BitContent() };
        if (m_stream.decoding) {
            // The stream reuses its decoded storage at the next token. Keep
            // escaped key slices rooted without interning before cache lookup.
            const size_t bytes = key.length * (key.has8BitContent ? sizeof(LChar) : sizeof(char16_t));
            const size_t start = (m_keyStorageUsed + alignof(char16_t) - 1) & ~(alignof(char16_t) - 1);
            void* characters;
            if (bytes <= sizeof(m_keyStorage) - start) {
                characters = m_keyStorage + start;
                m_keyStorageUsed = start + bytes;
            } else {
                characters = GC_MALLOC_ATOMIC(bytes);
            }
            memcpy(characters, key.characters, bytes);
            key.characters = characters;
        }
        m_keys.put(m_keyCount, key);
        ++m_keyCount;
        return true;
    }

    bool StartObject() { return start(true); }
    bool StartArray() { return start(false); }
    bool EndObject(rapidjson::SizeType count)
    {
        const auto frame = m_frames[m_frameCount - 1];
        ASSERT(frame.object && m_keyCount - frame.keyStart == count && m_valueCount - frame.valueStart == count);
        Object* object = createJSONObject(m_state, count, [&](size_t ordinal) { return m_keys[frame.keyStart + ordinal]; }, [&](size_t ordinal) { return m_values[frame.valueStart + ordinal]; });
        finish(frame, object);
        return true;
    }
    bool EndArray(rapidjson::SizeType count)
    {
        const auto frame = m_frames[m_frameCount - 1];
        ASSERT(!frame.object && m_valueCount - frame.valueStart == count);
        // The source Values remain traced during array allocation and copying.
        // Empty arrays use a valid non-null buffer, although it is not read.
        const Value empty;
        ArrayObject* array = Object::createArrayFromList(m_state, count, count ? m_values.data() + frame.valueStart : &empty);
        finish(frame, array);
        return true;
    }
    Value result() const
    {
        ASSERT(m_frameCount == 0 && m_valueCount == 1);
        return m_values[0];
    }

private:
    // Keep small parse stacks inline. Native pointers and Values are traced
    // on the C++ stack; external Values use their custom GC mark procedure.
    // Frames contain only offsets and do not require pointer tracing.
    struct Frame {
        size_t valueStart;
        size_t keyStart;
        size_t keyStorageStart;
        bool object;
    };
    bool append(Value value)
    {
        m_values.put(m_valueCount, value);
        ++m_valueCount;
        return true;
    }
    bool start(bool object)
    {
        CHECK_STACK_OVERFLOW(m_state);
        const Frame frame{ m_valueCount, m_keyCount, m_keyStorageUsed, object };
        m_frames.put(m_frameCount, frame);
        ++m_frameCount;
        return true;
    }
    void finish(const Frame& frame, Object* object)
    {
        if (m_valueCount == frame.valueStart) {
            append(Value(object));
        } else {
            m_values[frame.valueStart] = Value(object);
        }
        // Retain buffer capacity across sibling containers, but release roots
        // immediately so earlier values and decoded keys do not stay alive.
        for (size_t i = frame.valueStart + 1; i < m_valueCount; ++i) {
            m_values[i] = Value();
        }
        m_valueCount = frame.valueStart + 1;
        for (size_t i = frame.keyStart; i < m_keyCount; ++i) {
            m_keys[i] = JSONKey{ "", 0, true };
        }
        m_keyCount = frame.keyStart;
        m_keyStorageUsed = frame.keyStorageStart;
        --m_frameCount;
    }

    ExecutionState& m_state;
    JSONSAXStream<CharType>& m_stream;
    JSONSAXStack<32, Value, CustomAllocator<Value>> m_values;
    JSONSAXStack<16, JSONKey, GCUtil::gc_malloc_allocator<JSONKey>> m_keys;
    JSONSAXStack<16, Frame, GCUtil::gc_malloc_atomic_allocator<Frame>> m_frames;
    size_t m_valueCount{ 0 };
    size_t m_keyCount{ 0 };
    size_t m_frameCount{ 0 };
    size_t m_keyStorageUsed{ 0 };
    alignas(char16_t) LChar m_keyStorage[256];
};

// Full-precision numbers use Reader's scratch stack even for integers.
// Keep ordinary numbers inline, allocating only for unusually long tokens.
class JSONReaderAllocator {
public:
    static constexpr bool kNeedFree = false;

    void* Malloc(size_t size) { return Realloc(nullptr, 0, size); }
    void* Realloc(Optional<void*> original, size_t originalSize, size_t size)
    {
        if (!m_external.size() && size <= sizeof(m_inline)) {
            return m_inline;
        }
        const bool wasInline = !m_external.size();
        if (size > m_external.size()) {
            m_external.resizeWithUninitializedValues(size);
        }
        if (wasInline && original) {
            ASSERT(original.value() == m_inline && originalSize <= sizeof(m_inline));
            memcpy(m_external.data(), m_inline, originalSize);
        }
        return m_external.data();
    }
    static void Free(Optional<void*>) {}

private:
    alignas(char16_t) char m_inline[256];
    Vector<char, std::allocator<char>, ComputeReservedCapacityFunctionWithLog2<>> m_external;
};


template <typename CharType, typename Encoding>
static Value parseJSONSAX(ExecutionState& state, const CharType* input, size_t length)
{
    JSONSAXStream<CharType> stream(input, length);
    JSONSAXHandler<CharType> handler(state, stream);
    JSONReaderAllocator allocator;
    rapidjson::GenericReader<Encoding, Encoding, JSONReaderAllocator> reader(&allocator, 256);
    auto result = reader.template Parse<rapidjson::kParseDefaultFlags | rapidjson::kParseInsituFlag>(stream, handler);
    if (!result) {
        auto& strings = state.context()->staticStrings();
        ErrorObject::throwBuiltinError(state, ErrorCode::SyntaxError, strings.JSON.string(), true, strings.parse.string(), rapidjson::GetParseError_En(result.Code()));
    }
    stream.finishBuffers();
    return handler.result();
}
#endif

template <typename JSONCharType>
class JSONSourceCollector {
    using JSONValue = rapidjson::GenericValue<JSONCharType>;
    // Revivers may run GC before later source strings have been visited.
    using JSONSourceMap = std::unordered_map<const JSONValue*, String*,
                                             std::hash<const JSONValue*>, std::equal_to<const JSONValue*>,
                                             GCUtil::gc_malloc_allocator<std::pair<const JSONValue* const, String*>>>;

public:
    JSONSourceCollector(ExecutionState& state, String* text)
        : m_state(state)
        , m_text(text)
        , m_data(text->toUTF16StringData().data(), text->length())
    {
    }

    JSONSourceMap collect(const JSONValue& value)
    {
        collectValue(value);
        return std::move(m_sources);
    }

private:
    void skipWhitespace()
    {
        while (m_position < m_data.size() && (m_data[m_position] == u' ' || m_data[m_position] == u'\t' || m_data[m_position] == u'\n' || m_data[m_position] == u'\r')) {
            m_position++;
        }
    }

    void skipString()
    {
        ASSERT(m_data[m_position] == u'"');
        m_position++;
        while (m_position < m_data.size()) {
            char16_t c = m_data[m_position++];
            if (c == u'\\') {
                ASSERT(m_position < m_data.size());
                if (m_data[m_position++] == u'u') {
                    m_position += 4;
                }
            } else if (c == u'"') {
                return;
            }
        }
        ASSERT_NOT_REACHED();
    }

    void collectValue(const JSONValue& value)
    {
        skipWhitespace();
        if (value.IsObject()) {
            ASSERT(m_data[m_position] == u'{');
            m_position++;
            for (auto member = value.MemberBegin(); member != value.MemberEnd(); ++member) {
                skipWhitespace();
                skipString();
                skipWhitespace();
                ASSERT(m_data[m_position] == u':');
                m_position++;
                collectValue(member->value);
                skipWhitespace();
                if (member + 1 != value.MemberEnd()) {
                    ASSERT(m_data[m_position] == u',');
                    m_position++;
                }
            }
            skipWhitespace();
            ASSERT(m_data[m_position] == u'}');
            m_position++;
            return;
        }
        if (value.IsArray()) {
            ASSERT(m_data[m_position] == u'[');
            m_position++;
            for (rapidjson::SizeType i = 0; i < value.Size(); ++i) {
                collectValue(value[i]);
                skipWhitespace();
                if (i + 1 != value.Size()) {
                    ASSERT(m_data[m_position] == u',');
                    m_position++;
                }
            }
            skipWhitespace();
            ASSERT(m_data[m_position] == u']');
            m_position++;
            return;
        }

        size_t start = m_position;
        if (m_data[m_position] == u'"') {
            skipString();
        } else {
            while (m_position < m_data.size() && m_data[m_position] != u' ' && m_data[m_position] != u'\t' && m_data[m_position] != u'\n' && m_data[m_position] != u'\r' && m_data[m_position] != u',' && m_data[m_position] != u']' && m_data[m_position] != u'}') {
                m_position++;
            }
        }
        m_sources.emplace(&value, m_text->substring(start, m_position, &m_state));
    }

    ExecutionState& m_state;
    String* m_text;
    std::u16string m_data;
    size_t m_position{ 0 };
    JSONSourceMap m_sources;
};

static void codePointTo4digitString(int codepoint, std::basic_string<char16_t>& ss)
{
    ss.push_back(u'\\');
    ss.push_back(u'u');
    int d = 16 * 16 * 16;
    for (int i = 0; i < 4; ++i) {
        if (codepoint >= d) {
            char16_t c;
            if (codepoint / d < 10) {
                c = (codepoint / d) + '0';
            } else {
                c = (codepoint / d) - 10 + 'a';
            }
            codepoint %= d;
            ss.push_back(c);
        } else {
            ss.push_back(u'0');
        }
        d >>= 4;
    }
}

template <typename JSONCharType>
static Value finishJSONParse(ExecutionState& state, String* JText, Value reviver, Value unfiltered, const rapidjson::GenericValue<JSONCharType>& parseResult)
{
    using JSONValue = rapidjson::GenericValue<JSONCharType>;

    // 4
    if (reviver.isCallable()) {
        Object* root = new Object(state);
        auto sourceMap = JSONSourceCollector<JSONCharType>(state, JText).collect(parseResult);
        root->markThisObjectDontNeedStructureTransitionTable();
        root->defineOwnProperty(state, ObjectPropertyName(state, String::emptyString()), ObjectPropertyDescriptor(unfiltered, ObjectPropertyDescriptor::AllPresent));
        std::function<Value(Value, const ObjectPropertyName&, const JSONValue&)> Walk;
        Walk = [&](Value holder, const ObjectPropertyName& name, const JSONValue& source) -> Value {
            CHECK_STACK_OVERFLOW(state);
            Object* context = new Object(state);
            Value val = holder.asPointerValue()->asObject()->get(state, name).value(state, holder);
            if (val.isObject()) {
                JSONValue undefined;
                if (val.asObject()->isArray(state)) {
                    Object* object = val.asObject();
                    uint64_t i = 0;
                    uint64_t len = object->length(state);
                    while (i < len) {
                        const JSONValue* s = &undefined;
                        if (source.IsArray() && i < source.Size()) {
                            s = &source[i];
                        }
                        Value newElement = Walk(val, ObjectPropertyName(state, Value(i).toString(state)), *s);
                        if (newElement.isUndefined()) {
                            object->deleteOwnProperty(state, ObjectPropertyName(state, Value(i).toString(state)));
                        } else {
                            object->defineOwnProperty(state, ObjectPropertyName(state, Value(i).toString(state)), ObjectPropertyDescriptor(newElement, ObjectPropertyDescriptor::AllPresent));
                        }
                        i++;
                    }
                } else if (val.asObject()->isTypedArrayObject()) {
                    ArrayBufferView* arrObject = val.asObject()->asArrayBufferView();
                    uint64_t i = 0;
                    uint64_t len = arrObject->arrayLength();
                    while (i < len) {
                        const JSONValue* s = &undefined;
                        if (source.IsArray() && i < source.Size()) {
                            s = &source[i];
                        }
                        Value newElement = Walk(val, ObjectPropertyName(state, Value(i).toString(state)), *s);
                        if (newElement.isUndefined()) {
                            arrObject->deleteOwnProperty(state, ObjectPropertyName(state, Value(i).toString(state)));
                        } else {
                            arrObject->defineOwnProperty(state, ObjectPropertyName(state, Value(i).toString(state)), ObjectPropertyDescriptor(newElement, ObjectPropertyDescriptor::AllPresent));
                        }
                        i++;
                    }
                } else {
                    Object* object = val.asObject();

                    ObjectPropertyNameVector keys;
                    auto keyValues = Object::enumerableOwnProperties(state, object, EnumerableOwnPropertiesType::Key);
                    for (size_t i = 0; i < keyValues.size(); ++i) {
                        keys.push_back(ObjectPropertyName(state, keyValues[i]));
                    }

                    for (auto key : keys) {
                        const JSONValue* s = &undefined;
                        if (source.IsObject()) {
                            for (auto iter = source.MemberBegin(); iter != source.MemberEnd(); ++iter) {
                                Value sourceKey = parseJSONWorker<typename JSONCharType::Ch, JSONCharType>(state, iter->name);
                                if (key.toPlainValue().toString(state)->equals(sourceKey.asString())) {
                                    // Duplicate JSON property names are resolved last-wins.
                                    s = &iter->value;
                                }
                            }
                        }
                        Value newElement = Walk(val, key, *s);
                        if (newElement.isUndefined()) {
                            object->deleteOwnProperty(state, key);
                        } else {
                            object->defineOwnProperty(state, key, ObjectPropertyDescriptor(newElement, ObjectPropertyDescriptor::AllPresent));
                        }
                    }
                }
            } else {
                Value sourceValue;
                auto sourceIt = sourceMap.find(&source);
                if (sourceIt != sourceMap.end() && val.equalsToByTheSameValueAlgorithm(state, parseJSONWorker<typename JSONCharType::Ch, JSONCharType>(state, source))) {
                    sourceValue = sourceIt->second;
                }
                context->defineOwnProperty(state, ObjectPropertyName(state, state.context()->staticStrings().source),
                                           ObjectPropertyDescriptor(sourceValue, ObjectPropertyDescriptor::AllPresent));
            }
            // Internal index keys may stay numeric, but the reviver observes
            // an ECMAScript property key and therefore must receive a String.
            Value arguments[] = { name.toPropertyKeyValue(state), val, context };
            return Object::call(state, reviver, holder, 3, arguments);
        };
        return Walk(root, ObjectPropertyName(state, String::emptyString()), parseResult);
    }

    // 5
    return unfiltered;
}

Value JSON::parse(ExecutionState& state, Value text, Value reviver)
{
    // 1, 2, 3
    String* JText = text.toString(state);
    const size_t len = JText->length();
    auto accessData = JText->bufferAccessData();

#if !defined(ESCARGOT_SMALL_CONFIG)
    if (!reviver.isCallable()) {
        if (JText->has8BitContent()) {
            const LChar* src = reinterpret_cast<const LChar*>(len ? accessData.bufferAs8Bit : "");
            return parseJSONSAX<LChar, JSONLatin1Encoding>(state, src, len);
        }
        const char16_t* src = len ? accessData.bufferAs16Bit : u"";
        return parseJSONSAX<char16_t, rapidjson::UTF16<char16_t>>(state, src, len);
    }
    if (JText->has8BitContent()) {
        const LChar* src = reinterpret_cast<const LChar*>(len ? accessData.bufferAs8Bit : "");
        // The document borrows this one-byte buffer through reviver processing.
        std::unique_ptr<LChar[]> buf(new LChar[len]);
        JSONDocument<JSONLatin1Encoding> parseResult(state);
        if (parseJSONDocument<LChar, JSONLatin1Encoding>(state, src, len, buf.get(), parseResult)) {
            Value unfiltered = parseJSONWorker<LChar, JSONLatin1Encoding>(state, parseResult);
            return finishJSONParse<JSONLatin1Encoding>(state, JText, reviver, unfiltered, parseResult);
        }
        // Discard the partial one-byte DOM and retry from the original text.
    }
#endif

    // Keep the existing UTF-16 path for SMALL_CONFIG, two-byte input, and
    // Unicode escapes that could not be represented by the one-byte parser.
    // Own the decoded DOM strings through reviver processing, while preserving
    // the original text for context.source. Destroy the document first.
    std::unique_ptr<char16_t[]> buf(new char16_t[len]);
    JSONDocument<rapidjson::UTF16<char16_t>> parseResult(state);
    if (JText->has8BitContent()) {
        const LChar* src = reinterpret_cast<const LChar*>(len ? accessData.bufferAs8Bit : "");
        parseJSONDocument<LChar, rapidjson::UTF16<char16_t>>(state, src, len, buf.get(), parseResult);
    } else {
        if (len) {
            memcpy(buf.get(), accessData.bufferAs16Bit, len * sizeof(char16_t));
        }
        parseJSONDocument<char16_t, rapidjson::UTF16<char16_t>>(state, buf.get(), len, buf.get(), parseResult);
    }
    Value unfiltered = parseJSONWorker<char16_t, rapidjson::UTF16<char16_t>>(state, parseResult);
    return finishJSONParse<rapidjson::UTF16<char16_t>>(state, JText, reviver, unfiltered, parseResult);
}

static void builtinJSONArrayReplacerHelper(ExecutionState& state, ValueVectorWithInlineStorage& propertyList, Value property)
{
    String* item;

    if (property.isString()) {
        item = property.asString();
    } else if (property.isNumber()) {
        item = property.toString(state);
    } else if (property.isObject() && (property.asPointerValue()->isStringObject() || property.asPointerValue()->isNumberObject())) {
        item = property.toString(state);
    } else {
        return;
    }

    for (size_t i = 0; i < propertyList.size(); i++) {
        if (propertyList[i].abstractEqualsTo(state, item)) {
            return;
        }
    }
    propertyList.push_back(Value(item));
}

static bool builtinJSONStringifyStr(ExecutionState& state, Value key, Object* holder,
                                    StaticStrings* strings, Value replacerFunc, ValueVectorWithInlineStorage& stack, String* indent,
                                    String* gap, bool propertyListTouched, ValueVectorWithInlineStorage& propertyList,
                                    LargeStringBuilder& product);
static void builtinJSONStringifyJA(ExecutionState& state, Object* obj,
                                   StaticStrings* strings, Value replacerFunc, ValueVectorWithInlineStorage& stack, String* indent,
                                   String* gap, bool propertyListTouched, ValueVectorWithInlineStorage& propertyList,
                                   LargeStringBuilder& product);
static void builtinJSONStringifyJO(ExecutionState& state, Object* value,
                                   StaticStrings* strings, Value replacerFunc, ValueVectorWithInlineStorage& stack, String* indent,
                                   String* gap, bool propertyListTouched, ValueVectorWithInlineStorage& propertyList,
                                   LargeStringBuilder& product);
static void builtinJSONStringifyQuote(ExecutionState& state, Value value, LargeStringBuilder& product);

// https://www.ecma-international.org/ecma-262/6.0/#sec-serializejsonproperty
static bool builtinJSONStringifyStr(ExecutionState& state, Value key, Object* holder,
                                    StaticStrings* strings, Value replacerFunc, ValueVectorWithInlineStorage& stack,
                                    String* indent, String* gap, bool propertyListTouched, ValueVectorWithInlineStorage& propertyList,
                                    LargeStringBuilder& product)
{
    CHECK_STACK_OVERFLOW(state);
    Value value = holder->get(state, ObjectPropertyName(state, key)).value(state, holder);
    if (value.isObject() || value.isBigInt()) {
        Value toJson = Object::getV(state, value, ObjectPropertyName(state, strings->toJSON));
        if (toJson.isCallable()) {
            Value arguments[] = { key.toString(state) };
            value = Object::call(state, toJson, value, 1, arguments);
        }
    }

    if (!replacerFunc.isUndefined()) {
        Value arguments[] = { key.toString(state), value };
        value = Object::call(state, replacerFunc, holder, 2, arguments);
    }

    bool isRawString = false;
    if (value.isObject()) {
        if (value.asObject()->isNumberObject()) {
            value = Value(Value::DoubleToIntConvertibleTestNeeds, value.toNumber(state));
        } else if (value.asObject()->isStringObject()) {
            value = Value(value.toString(state));
        } else if (value.asObject()->isBooleanObject()) {
            value = Value(value.asObject()->asBooleanObject()->primitiveValue());
        } else if (value.asObject()->isBigIntObject()) {
            value = Value(value.asObject()->asBigIntObject()->primitiveValue());
        } else if (value.asObject()->isRawJSONObject()) {
            value = value.asObject()->get(state, ObjectPropertyName(state.context()->staticStrings().rawJSON)).value(state, value);
            isRawString = true;
        }
    }
    if (value.isNull()) {
        product.appendString(strings->null.string());
        return true;
    }
    if (value.isBoolean()) {
        product.appendString(value.asBoolean() ? strings->stringTrue.string() : strings->stringFalse.string());
        return true;
    }
    if (value.isString()) {
        if (isRawString) {
            product.appendString(value.asString(), &state);
        } else {
            builtinJSONStringifyQuote(state, value.asString(), product);
        }
        return true;
    }
    if (value.isNumber()) {
        double d = value.toNumber(state);
        if (std::isfinite(d)) {
            product.appendString(value.toString(state));
            return true;
        }
        product.appendString(strings->null.string());
        return true;
    }
    if (value.isBigInt()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "Could not serialize a BigInt");
    }
    if (value.isObject() && !value.isCallable()) {
        if (value.asObject()->isArray(state)) {
            builtinJSONStringifyJA(state, value.asObject(), strings, replacerFunc, stack, indent, gap, propertyListTouched, propertyList, product);
        } else {
            builtinJSONStringifyJO(state, value.asObject(), strings, replacerFunc, stack, indent, gap, propertyListTouched, propertyList, product);
        }
        return true;
    }

    return false;
}

// https://www.ecma-international.org/ecma-262/6.0/#sec-serializejsonarray
static void builtinJSONStringifyJA(ExecutionState& state, Object* obj,
                                   StaticStrings* strings, Value replacerFunc, ValueVectorWithInlineStorage& stack,
                                   String* indent, String* gap, bool propertyListTouched, ValueVectorWithInlineStorage& propertyList,
                                   LargeStringBuilder& product)
{
    // 1
    for (size_t i = 0; i < stack.size(); i++) {
        Value& v = stack[i];
        if (v == Value(obj)) {
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->JSON.string(), false, strings->stringify.string(), ErrorObject::Messages::GlobalObject_JAError);
        }
    }
    // 2
    stack.push_back(Value(obj));
    // 3
    String* stepback = indent;
    // 4
    StringBuilder newIndent;
    newIndent.appendString(indent);
    newIndent.appendString(gap);
    indent = newIndent.finalize(&state);

    // 6, 7
    uint32_t len = obj->length(state);

    // Each array element requires at least 1 character for the value, and 1 character for the separator
    if (len / 2 > STRING_MAXIMUM_LENGTH) {
        ErrorObject::throwBuiltinError(state, ErrorCode::RangeError, strings->JSON.string(), false, strings->stringify.string(), ErrorObject::Messages::GlobalObject_JAError);
    }

    // 8 ~ 9
    uint32_t index = 0;
    bool first = true;
    String* seperator = strings->asciiTable[(size_t)','].string();

    product.appendChar('[');
    while (index < len) {
        if (first) {
            if (gap->length()) {
                product.appendChar('\n');
                product.appendString(indent);
                StringBuilder seperatorBuilder;
                seperatorBuilder.appendChar(',');
                seperatorBuilder.appendChar('\n');
                seperatorBuilder.appendString(indent);
                seperator = seperatorBuilder.finalize(&state);
            }
            first = false;
        } else {
            product.appendString(seperator);
        }

        bool strP = builtinJSONStringifyStr(state, Value(index), obj, strings, replacerFunc, stack, indent, gap, propertyListTouched, propertyList, product);
        if (!strP) {
            product.appendString(strings->null.string());
        }
        index++;
    }

    if (!first && gap->length()) {
        product.appendChar('\n');
        product.appendString(stepback);
    }

    product.appendChar(']');

    // 11
    stack.pop_back();
    // 12
    indent = stepback;
}

// https://www.ecma-international.org/ecma-262/6.0/#sec-serializejsonobject
static void builtinJSONStringifyJO(ExecutionState& state, Object* value,
                                   StaticStrings* strings, Value replacerFunc, ValueVectorWithInlineStorage& stack, String* indent,
                                   String* gap, bool propertyListTouched, ValueVectorWithInlineStorage& propertyList, LargeStringBuilder& product)
{
    // 1
    for (size_t i = 0; i < stack.size(); i++) {
        if (stack[i] == value) {
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, strings->JSON.string(), false, strings->stringify.string(), ErrorObject::Messages::GlobalObject_JOError);
        }
    }
    // 2
    stack.push_back(Value(value));
    // 3
    String* stepback = indent;
    // 4
    StringBuilder newIndent;
    newIndent.appendString(indent, &state);
    newIndent.appendString(gap, &state);
    indent = newIndent.finalize(&state);
    // 5, 6
    ValueVectorWithInlineStorage k;
    if (propertyListTouched) {
        k = propertyList;
    } else {
        k = Object::enumerableOwnProperties(state, value, EnumerableOwnPropertiesType::Key);
    }

    // 7 ~ 9
    bool first = true;
    size_t len = k.size();
    String* seperator = strings->asciiTable[(size_t)','].string();

    product.appendChar('{');
    LargeStringBuilder subProduct;
    for (size_t i = 0; i < len; i++) {
        auto strP = builtinJSONStringifyStr(state, k[i], value, strings, replacerFunc, stack, indent, gap, propertyListTouched, propertyList, subProduct);
        if (strP) {
            if (first) {
                if (gap->length()) {
                    product.appendChar('\n', &state);
                    product.appendString(indent, &state);
                    StringBuilder seperatorBuilder;
                    seperatorBuilder.appendChar(',', &state);
                    seperatorBuilder.appendChar('\n', &state);
                    seperatorBuilder.appendString(indent, &state);
                    seperator = seperatorBuilder.finalize(&state);
                }
                first = false;
            } else {
                product.appendString(seperator, &state);
            }

            builtinJSONStringifyQuote(state, k[i], product);
            product.appendChar(':', &state);
            if (gap->length() != 0) {
                product.appendChar(' ', &state);
            }
            product.appendString(subProduct.finalize(&state), &state);
        }
    }

    if (!first && gap->length()) {
        product.appendChar('\n');
        product.appendString(stepback);
    }

    product.appendChar('}');

    // 11
    stack.pop_back();
    // 12
    indent = stepback;
}

// https://www.ecma-international.org/ecma-262/6.0/#sec-quotejsonstring
static void builtinJSONStringifyQuote(ExecutionState& state, String* value, LargeStringBuilder& product)
{
    bool allNormalChar = true;
    auto bad = value->bufferAccessData();
    for (size_t i = 0; i < bad.length; ++i) {
        char16_t c = bad.charAt(i);
        switch (c) {
        case u'\"':
        case u'\\':
        case u'\b':
        case u'\f':
        case u'\n':
        case u'\r':
        case u'\t':
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 11:
        case 14:
        case 15:
        case 16:
        case 17:
        case 18:
        case 19:
        case 20:
        case 21:
        case 22:
        case 23:
        case 24:
        case 25:
        case 26:
        case 27:
        case 28:
        case 29:
        case 30:
        case 31:
            allNormalChar = false;
            break;
        default:
            if (UNLIKELY(U16_IS_SURROGATE(c))) {
                allNormalChar = false;
            }
            break;
        }
        if (UNLIKELY(!allNormalChar)) {
            break;
        }
    }

    if (allNormalChar) {
        product.appendChar('"');
        product.appendString(value);
        product.appendChar('"');
        return;
    }

    bool allLatin1 = true;
    std::basic_string<char16_t> buffer;
    buffer.reserve(bad.length);
    buffer.push_back(u'"');
    for (size_t i = 0; i < bad.length; ++i) {
        char16_t c = bad.charAt(i);
        switch (c) {
        case u'\"':
        case u'\\':
            buffer.push_back(u'\\');
            buffer.push_back(c);
            break;
        case u'\b':
            buffer.push_back(u'\\');
            buffer.push_back(u'b');
            break;
        case u'\f':
            buffer.push_back(u'\\');
            buffer.push_back(u'f');
            break;
        case u'\n':
            buffer.push_back(u'\\');
            buffer.push_back(u'n');
            break;
        case u'\r':
            buffer.push_back(u'\\');
            buffer.push_back(u'r');
            break;
        case u'\t':
            buffer.push_back(u'\\');
            buffer.push_back(u't');
            break;
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 11:
        case 14:
        case 15:
        case 16:
        case 17:
        case 18:
        case 19:
        case 20:
        case 21:
        case 22:
        case 23:
        case 24:
        case 25:
        case 26:
        case 27:
        case 28:
        case 29:
        case 30:
        case 31:
            codePointTo4digitString(c, buffer);
            break;
        default:
            if (c > 255) {
                allLatin1 = false;
            }
            if (U16_IS_LEAD(c) && i + 1 < bad.length && U16_IS_TRAIL(bad.charAt(i + 1))) {
                buffer.push_back(c);
                buffer.push_back(bad.charAt(i + 1));
                i++;
            } else if (U16_IS_SURROGATE(c)) {
                codePointTo4digitString(c, buffer);
            } else {
                buffer.push_back(c);
            }
        }
    }
    buffer.push_back(u'"');
    if (allLatin1) {
        product.appendString(String::fromLatin1(buffer.data(), buffer.length(), state.context()));
    } else {
        product.appendString(new UTF16String(buffer.data(), buffer.length()));
    }
}

static void builtinJSONStringifyQuote(ExecutionState& state, Value value, LargeStringBuilder& product)
{
    String* str = value.toString(state);
    builtinJSONStringifyQuote(state, str, product);
}

Value JSON::stringify(ExecutionState& state, Value value, Value replacer, Value space)
{
    auto strings = &state.context()->staticStrings();

    // 1, 2, 3
    String* indent = String::emptyString();
    ValueVectorWithInlineStorage stack;
    ValueVectorWithInlineStorage propertyList;
    bool propertyListTouched = false;

    // 4
    Value replacerFunc;
    if (replacer.isObject()) {
        if (replacer.isCallable()) {
            replacerFunc = replacer;
        } else if (replacer.asObject()->isArrayObject()) {
            propertyListTouched = true;
            ArrayObject* arrObject = replacer.asObject()->asArrayObject();

            std::vector<uint32_t> indexes;
            arrObject->enumeration(state, [](ExecutionState& state, Object* self, const ObjectPropertyName& P, const ObjectStructurePropertyDescriptor& desc, void* data) -> bool {
                uint32_t idx = P.toPlainValue().tryToUseAsIndex32(state);
                if (idx != Value::InvalidIndex32Value) {
                    std::vector<uint32_t>* indexes = (std::vector<uint32_t>*)data;
                    indexes->push_back(idx);
                }
                return true; }, &indexes);
            std::sort(indexes.begin(), indexes.end(), std::less<uint32_t>());
            for (uint32_t i = 0; i < indexes.size(); ++i) {
                Value property = arrObject->get(state, ObjectPropertyName(state, Value(indexes[i]))).value(state, arrObject);
                builtinJSONArrayReplacerHelper(state, propertyList, property);
            }
        } else if (replacer.asObject()->isArray(state)) {
            propertyListTouched = true;
            Object* replacerObj = replacer.asObject();
            uint64_t len = replacerObj->length(state);
            uint64_t k = 0;

            while (k < len) {
                Value v = replacerObj->getIndexedProperty(state, Value(k)).value(state, replacerObj);
                builtinJSONArrayReplacerHelper(state, propertyList, v);
                k++;
            }
        }
    }

    // 5
    if (space.isObject()) {
        if (space.isObject() && space.asObject()->isNumberObject()) {
            space = Value(Value::DoubleToIntConvertibleTestNeeds, space.toNumber(state));
        } else if (space.isObject() && space.asObject()->isStringObject()) {
            space = space.toString(state);
        }
    }

    // 6, 7, 8
    String* gap = String::emptyString();
    if (space.isNumber()) {
        int space_cnt = Value::truncateDoubleToInt32Unchecked(std::min(space.toInteger(state), 10.0));
        if (space_cnt >= 1) {
            UTF8StringData gapData;
            gapData.resizeWithUninitializedValues(space_cnt);
            for (int i = 0; i < space_cnt; i++) {
                gapData[i] = ' ';
            }
            gap = new ASCIIString(gapData.data(), gapData.length());
        }
    } else if (space.isString()) {
        if (space.asString()->length() <= 10) {
            gap = space.asString();
        } else {
            gap = space.asString()->substring(0, 10, &state);
        }
    }

    // 9
    Object* wrapper = new Object(state);
    // 10
    wrapper->defineOwnProperty(state, ObjectPropertyName(state, String::emptyString()), ObjectPropertyDescriptor(value, ObjectPropertyDescriptor::AllPresent));
    LargeStringBuilder product;
    auto ret = builtinJSONStringifyStr(state, String::emptyString(), wrapper, strings, replacerFunc, stack, indent, gap, propertyListTouched, propertyList, product);
    if (ret) {
        return product.finalize(&state);
    }
    return Value();
}
} // namespace Escargot
