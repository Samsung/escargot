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

#ifndef __EscargotString__
#define __EscargotString__

#include "runtime/PointerValue.h"
#include "util/BasicString.h"
#include <string>
#include "util/Vector.h"

namespace Escargot {

// A type to hold a single Latin-1 character.
typedef unsigned char LChar;

typedef BasicString<char, GCUtil::gc_malloc_atomic_allocator<char>> ASCIIStringData;
typedef BasicString<LChar, GCUtil::gc_malloc_atomic_allocator<LChar>> Latin1StringData;
typedef BasicString<char, GCUtil::gc_malloc_atomic_allocator<char>> UTF8StringData;
typedef BasicString<char16_t, GCUtil::gc_malloc_atomic_allocator<char16_t>> UTF16StringData;
typedef BasicString<char32_t, GCUtil::gc_malloc_atomic_allocator<char32_t>> UTF32StringData;
typedef Vector<String*, GCUtil::gc_malloc_allocator<String*>> StringVector;

typedef std::basic_string<char, std::char_traits<char>> ASCIIStringDataNonGCStd;
typedef std::basic_string<LChar, std::char_traits<LChar>> Latin1StringDataNonGCStd;
typedef std::basic_string<char, std::char_traits<char>> UTF8StringDataNonGCStd;
typedef std::basic_string<char16_t, std::char_traits<char16_t>> UTF16StringDataNonGCStd;
typedef std::basic_string<char32_t, std::char_traits<char32_t>> UTF32StringDataNonGCStd;

std::vector<std::string> split(const std::string& s, char seperator);

bool isASCIIAlpha(char ch);
bool isASCIIDigit(char ch);
bool isASCIIAlphanumeric(char ch);
bool isAllSpecialCharacters(const std::string& s, bool (*fn)(char));

bool isAllASCII(const char* buf, const size_t len);
bool isAllASCII(const char16_t* buf, const size_t len);
bool isAllLatin1(const char16_t* buf, const size_t len);
bool isIndexString(String* str);
char32_t readUTF8Sequence(const char*& sequence, bool& valid, int& charlen);
UTF16StringData utf8StringToUTF16String(const char* buf, const size_t len);
UTF8StringData utf16StringToUTF8String(const char16_t* buf, const size_t len);
ASCIIStringData utf16StringToASCIIString(const char16_t* buf, const size_t len);
ASCIIStringData dtoa(double number);
size_t utf32ToUtf8(char32_t uc, char* UTF8);
size_t utf32ToUtf16(char32_t i, char16_t* u);

// these functions only care ascii range(0~127)
bool islower(char32_t ch);
bool isupper(char32_t ch);
char32_t tolower(char32_t ch);
char32_t toupper(char32_t ch);
bool isspace(char32_t ch);
bool isdigit(char32_t ch);

class ASCIIString;
class Latin1String;
class UTF16String;
class RopeString;
class StringView;
class Context;

class StringWriteOption {
public:
    enum {
        NoOptions = 0,
        ReplaceInvalidUtf8 = 1 << 3
    };
};

struct StringBufferAccessData {
    bool has8BitContent;
    size_t length;
    union {
        const void* buffer;
        const char* bufferAs8Bit;
        const char16_t* bufferAs16Bit;
    };
    void* extraData;

    StringBufferAccessData(bool has8BitContent, size_t length, void* buffer, void* extraDataKeepInStack = nullptr)
        : has8BitContent(has8BitContent)
        , length(length)
        , buffer(buffer)
        , extraData(extraDataKeepInStack)
    {
    }

    char16_t uncheckedCharAtFor8Bit(size_t idx) const
    {
        ASSERT(has8BitContent);
        return ((LChar*)buffer)[idx];
    }

    char16_t uncheckedCharAtFor16Bit(size_t idx) const
    {
        ASSERT(!has8BitContent);
        return ((char16_t*)buffer)[idx];
    }

    char16_t charAt(size_t idx) const
    {
        if (has8BitContent) {
            return ((LChar*)buffer)[idx];
        } else {
            return ((char16_t*)buffer)[idx];
        }
    }

    static bool equals16Bit(const char16_t* c1, const char* c2, size_t len);

    ALWAYS_INLINE bool equalsSameLength(const char* str, size_t compareStartAt = 0) const
    {
        ASSERT(strlen(str) == length);

        if (LIKELY(has8BitContent)) {
            return memcmp((const char*)buffer + compareStartAt, str + compareStartAt, length - compareStartAt) == 0;
        } else {
            return equals16Bit((const char16_t*)buffer + compareStartAt, str + compareStartAt, length - compareStartAt);
        }
    }

    template <typename OutputType, typename ComputingType>
    OutputType toUTF8String() const
    {
        ComputingType s = toUTF8String<ComputingType>();
        return OutputType(s.data(), s.length());
    }

    template <typename OutputType>
    OutputType toUTF8String(int options = StringWriteOption::NoOptions) const
    {
        OutputType ret;
        const bool replaceInvalidUtf8 = options == StringWriteOption::ReplaceInvalidUtf8;
        const auto& accessData = *this;
        for (size_t i = 0; i < accessData.length; i++) {
            char32_t ch = (uint16_t)accessData.charAt(i);
            if (ch < 0x80) {
                ret.append((char*)&ch, 1);
            } else {
                char32_t finalCh;
                if (U16_IS_LEAD(ch)) {
                    if (i + 1 == accessData.length) {
                        if (replaceInvalidUtf8) {
                            finalCh = 0xFFFD;
                        } else {
                            finalCh = ch;
                        }
                    } else {
                        char16_t c2 = accessData.charAt(i + 1);
                        finalCh = ch;
                        if (U16_IS_TRAIL(c2)) {
                            finalCh = U16_GET_SUPPLEMENTARY(ch, c2);
                            i++;
                        } else if (replaceInvalidUtf8) {
                            finalCh = 0xFFFD;
                        }
                    }
                } else if (replaceInvalidUtf8 && U16_IS_TRAIL(ch)) {
                    finalCh = 0xFFFD;
                } else {
                    finalCh = ch;
                }

                char buf[8];
                auto len = utf32ToUtf8(finalCh, buf);
                ret.append(buf, len);
            }
        }
        return ret;
    }
};

class String : public PointerValue {
    friend class AtomicString;

protected:
    String()
    {
        m_tag = POINTER_VALUE_STRING_TAG_IN_DATA;
        m_bufferData.hasSpecialImpl = false;
    }

    struct StringBufferData {
        bool has8BitContent : 1;
        bool hasSpecialImpl : 1;
#if defined(ESCARGOT_32)
        size_t length : 30;
#else
        size_t length : 62;
#endif
        union {
            const void* buffer;
            const char* bufferAs8Bit;
            const char16_t* bufferAs16Bit;
            String* bufferAsString;
        };

        COMPILE_ASSERT(STRING_MAXIMUM_LENGTH < (std::numeric_limits<size_t>::max() >> 2), "");

        operator StringBufferAccessData() const
        {
            ASSERT(!hasSpecialImpl);
            return StringBufferAccessData(has8BitContent, length, const_cast<void*>(buffer));
        }

        char16_t uncheckedCharAtFor8Bit(size_t idx) const
        {
            ASSERT(has8BitContent);
            return ((LChar*)buffer)[idx];
        }

        char16_t uncheckedCharAtFor16Bit(size_t idx) const
        {
            ASSERT(!has8BitContent);
            return ((char16_t*)buffer)[idx];
        }

        char16_t charAt(size_t idx) const
        {
            if (has8BitContent) {
                return ((LChar*)buffer)[idx];
            } else {
                return ((char16_t*)buffer)[idx];
            }
        }
    };

public:
    enum FromExternalMemoryTag {
        FromExternalMemory
    };

    virtual bool isStringByVTable() const override
    {
        return true;
    }

    virtual bool isStringView()
    {
        return false;
    }

    virtual bool isCompressibleString()
    {
        return false;
    }

    virtual bool isRopeString()
    {
        return false;
    }

    bool has8BitContent() const
    {
        return m_bufferData.has8BitContent;
    }

    static String* fromASCII(const char* s);
    static String* fromCharCode(char32_t code);
    static String* fromDouble(double v);
    static String* fromInt32(int32_t v)
    {
        // TODO
        return fromDouble(v);
    }
    static String* fromUTF8(const char* src, size_t len);
#if defined(ENABLE_COMPRESSIBLE_STRING)
    static String* fromUTF8ToCompressibleString(Context* context, const char* src, size_t len);
#endif

    size_t length() const
    {
        return m_bufferData.length;
    }
    virtual char16_t charAt(const size_t idx) const
    {
        return bufferAccessData().charAt(idx);
    }
    char16_t operator[](const size_t idx) const
    {
        return charAt(idx);
    }

    ALWAYS_INLINE StringBufferAccessData bufferAccessData() const
    {
        if (UNLIKELY(m_bufferData.hasSpecialImpl)) {
            return const_cast<String*>(this)->bufferAccessDataSpecialImpl();
        }
        return m_bufferData;
    }

    bool equals(const String* src) const;

    template <const size_t srcLen>
    bool equals(const char (&src)[srcLen]) const
    {
        return equals(src, srcLen - 1);
    }

    bool equals(const char* src, size_t srcLen) const
    {
        if (srcLen != length()) {
            return false;
        }

        for (size_t i = 0; i < srcLen; i++) {
            if (src[i] != charAt(i)) {
                return false;
            }
        }

        return true;
    }
    String* getSubstitution(ExecutionState& state, String* matched, String* str, size_t position, StringVector& captures, String* replacement);
    size_t find(String* str, size_t pos = 0);

    template <size_t N>
    size_t find(const char (&str)[N], size_t pos = 0) const
    {
        const size_t srcStrLen = N - 1;
        const size_t size = length();

        if (srcStrLen == 0)
            return pos <= size ? pos : SIZE_MAX;

        if (srcStrLen <= size) {
            char32_t src0 = str[0];
            const auto& data = bufferAccessData();
            if (data.has8BitContent) {
                for (; pos <= size - srcStrLen; ++pos) {
                    if (((const LChar*)data.buffer)[pos] == src0) {
                        bool same = true;
                        for (size_t k = 1; k < srcStrLen; k++) {
                            if (((const LChar*)data.buffer)[pos + k] != str[k]) {
                                same = false;
                                break;
                            }
                        }
                        if (same)
                            return pos;
                    }
                }
            } else {
                for (; pos <= size - srcStrLen; ++pos) {
                    if (((const char16_t*)data.buffer)[pos] == src0) {
                        bool same = true;
                        for (size_t k = 1; k < srcStrLen; k++) {
                            if (((const char16_t*)data.buffer)[pos + k] != str[k]) {
                                same = false;
                                break;
                            }
                        }
                        if (same)
                            return pos;
                    }
                }
            }
        }
        return SIZE_MAX;
    }

    template <size_t N>
    bool contains(const char (&str)[N]) const
    {
        return find(str) != SIZE_MAX;
    }

    size_t rfind(String* str, size_t pos);

    String* substring(size_t from, size_t to);

    template <typename T>
    static inline size_t stringHash(T* src, size_t length)
    {
        size_t hash = static_cast<size_t>(0xc70f6907UL);
        for (; length; --length)
            hash = (hash * 131) + *src++;
        return hash;
    }

    size_t hashValue() const
    {
        const auto& data = bufferAccessData();
        size_t len = data.length;
        size_t hash;
        if (LIKELY(data.has8BitContent)) {
            auto ptr = (const LChar*)data.buffer;
            hash = stringHash(ptr, len);
        } else {
            auto ptr = (const char16_t*)data.buffer;
            hash = stringHash(ptr, len);
        }

        if (UNLIKELY((hash % sizeof(size_t)) == 0)) {
            hash++;
        }

        return hash;
    }

    bool operator==(const String& src) const
    {
        return equals(&src);
    }

    bool operator!=(const String& src) const
    {
        return !operator==(src);
    }

    ALWAYS_INLINE
    friend bool operator<(const String& a, const String& b);
    ALWAYS_INLINE
    friend bool operator<=(const String& a, const String& b);
    ALWAYS_INLINE
    friend bool operator>(const String& a, const String& b);
    ALWAYS_INLINE
    friend bool operator>=(const String& a, const String& b);
    ALWAYS_INLINE
    friend int stringCompare(const String& a, const String& b);

    // NOTE these function generates new copy of string data
    virtual UTF16StringData toUTF16StringData() const = 0;
    virtual UTF8StringData toUTF8StringData() const = 0;
    virtual UTF8StringDataNonGCStd toNonGCUTF8StringData(int options = StringWriteOption::NoOptions) const = 0;
    static String* emptyString;

    uint64_t tryToUseAsIndex() const;
    uint64_t tryToUseAsArrayIndex() const;

    bool is8Bit() const
    {
        return has8BitContent();
    }

    template <typename Any>
    const Any* characters() const
    {
        if (is8Bit()) {
            return (Any*)characters8();
        } else {
            return (Any*)characters16();
        }
    }

    virtual const LChar* characters8() const
    {
        RELEASE_ASSERT_NOT_REACHED();
        return nullptr;
    }
    virtual const char16_t* characters16() const
    {
        RELEASE_ASSERT_NOT_REACHED();
        return nullptr;
    }

    size_t advanceStringIndex(size_t index, bool unicode);

private:
    size_t m_tag;

protected:
    StringBufferData m_bufferData;
    virtual StringBufferAccessData bufferAccessDataSpecialImpl()
    {
        RELEASE_ASSERT_NOT_REACHED();
        return m_bufferData;
    }

    static int stringCompare(size_t l1, size_t l2, const String* c1, const String* c2);

    template <typename T>
    static ALWAYS_INLINE bool stringEqual(const T* s, const T* s1, const size_t len)
    {
        return memcmp(s, s1, sizeof(T) * len) == 0;
    }

    static ALWAYS_INLINE bool stringEqual(const char16_t* s, const LChar* s1, const size_t len)
    {
        for (size_t i = 0; i < len; i++) {
            if (s[i] != s1[i]) {
                return false;
            }
        }
        return true;
    }
};

inline bool operator<(const String& a, const String& b)
{
    return String::stringCompare(a.length(), b.length(), &a, &b) < 0;
}

inline bool operator>(const String& a, const String& b)
{
    return String::stringCompare(a.length(), b.length(), &a, &b) > 0;
}

inline bool operator<=(const String& a, const String& b)
{
    return String::stringCompare(a.length(), b.length(), &a, &b) <= 0;
}

inline bool operator>=(const String& a, const String& b)
{
    return String::stringCompare(a.length(), b.length(), &a, &b) >= 0;
}

inline int stringCompare(const String& a, const String& b)
{
    return String::stringCompare(a.length(), b.length(), &a, &b);
}

class ASCIIString : public String {
public:
    explicit ASCIIString(ASCIIStringData&& src)
        : String()
    {
        ASCIIStringData stringData = std::move(src);
        initBufferAccessData(stringData);
    }

    explicit ASCIIString(const char* str)
        : String()
    {
        ASCIIStringData stringData;
        stringData.append(str, strlen(str));
        initBufferAccessData(stringData);
    }

    ASCIIString(const char* str, size_t len)
        : String()
    {
        ASCIIStringData stringData;
        stringData.append(str, len);
        initBufferAccessData(stringData);
    }

    ASCIIString(const char16_t* str, size_t len)
        : String()
    {
        ASCIIStringData stringData;
        stringData.resizeWithUninitializedValues(len);
        for (size_t i = 0; i < len; i++) {
            ASSERT(str[i] < 128);
            stringData[i] = str[i];
        }
        initBufferAccessData(stringData);
    }

    ASCIIString(const char* str, size_t len, FromExternalMemoryTag)
        : String()
    {
        m_bufferData.bufferAs8Bit = str;
        m_bufferData.length = len;
        m_bufferData.hasSpecialImpl = false;
        m_bufferData.has8BitContent = true;
    }

    virtual char16_t charAt(const size_t idx) const
    {
        return m_bufferData.uncheckedCharAtFor8Bit(idx);
    }

    virtual const LChar* characters8() const
    {
        return (const LChar*)m_bufferData.buffer;
    }

    void initBufferAccessData(ASCIIStringData& stringData)
    {
        m_bufferData.has8BitContent = true;
        m_bufferData.length = stringData.length();
        m_bufferData.buffer = stringData.takeBuffer();
    }

    virtual UTF16StringData toUTF16StringData() const;
    virtual UTF8StringData toUTF8StringData() const;
    virtual UTF8StringDataNonGCStd toNonGCUTF8StringData(int options = StringWriteOption::NoOptions) const;

    void* operator new(size_t size);
    void* operator new(size_t size, GCPlacement p)
    {
        return gc::operator new(size, p);
    }
    void* operator new(size_t, void* ptr)
    {
        return ptr;
    }
    void* operator new[](size_t size) = delete;
};

class Latin1String : public String {
public:
    explicit Latin1String(Latin1StringData&& src)
        : String()
    {
        Latin1StringData data = std::move(src);
        initBufferAccessData(data);
    }

    explicit Latin1String(const char* str)
        : String()
    {
        Latin1StringData data;
        data.append((const LChar*)str, strlen(str));
        initBufferAccessData(data);
    }

    Latin1String(const char* str, size_t len)
        : String()
    {
        Latin1StringData data;
        data.append((const LChar*)str, len);
        initBufferAccessData(data);
    }

    Latin1String(const LChar* str, size_t len, FromExternalMemoryTag)
        : String()
    {
        m_bufferData.buffer = str;
        m_bufferData.length = len;
        m_bufferData.hasSpecialImpl = false;
        m_bufferData.has8BitContent = true;
    }

    Latin1String(const LChar* str, size_t len)
        : String()
    {
        Latin1StringData data;
        data.append(str, len);
        initBufferAccessData(data);
    }

    Latin1String(const char16_t* str, size_t len)
        : String()
    {
        Latin1StringData data;

        data.resizeWithUninitializedValues(len);
        for (size_t i = 0; i < len; i++) {
            ASSERT(str[i] < 256);
            data[i] = str[i];
        }
        initBufferAccessData(data);
    }

    void initBufferAccessData(Latin1StringData& stringData)
    {
        m_bufferData.has8BitContent = true;
        m_bufferData.length = stringData.length();
        m_bufferData.buffer = stringData.takeBuffer();
    }

    virtual char16_t charAt(const size_t idx) const
    {
        return m_bufferData.uncheckedCharAtFor8Bit(idx);
    }

    virtual const LChar* characters8() const
    {
        return (const LChar*)m_bufferData.buffer;
    }

    virtual UTF16StringData toUTF16StringData() const;
    virtual UTF8StringData toUTF8StringData() const;
    virtual UTF8StringDataNonGCStd toNonGCUTF8StringData(int options = StringWriteOption::NoOptions) const;

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;
};

class UTF16String : public String {
public:
    explicit UTF16String(UTF16StringData&& src)
        : String()
    {
        UTF16StringData data = std::move(src);
        initBufferAccessData(data);
    }

    UTF16String(const char16_t* str, size_t len)
        : String()
    {
        UTF16StringData data;
        data.append(str, len);
        initBufferAccessData(data);
    }

    UTF16String(const char16_t* str, size_t len, FromExternalMemoryTag)
        : String()
    {
        m_bufferData.bufferAs16Bit = str;
        m_bufferData.length = len;
        m_bufferData.hasSpecialImpl = false;
        m_bufferData.has8BitContent = false;
    }

    void initBufferAccessData(UTF16StringData& stringData)
    {
        m_bufferData.has8BitContent = false;
        m_bufferData.length = stringData.length();
        m_bufferData.buffer = stringData.takeBuffer();
    }

    virtual char16_t charAt(const size_t idx) const
    {
        return m_bufferData.uncheckedCharAtFor16Bit(idx);
    }

    virtual const char16_t* characters16() const
    {
        return (const char16_t*)m_bufferData.buffer;
    }

    virtual UTF16StringData toUTF16StringData() const;
    virtual UTF8StringData toUTF8StringData() const;
    virtual UTF8StringDataNonGCStd toNonGCUTF8StringData(int options = StringWriteOption::NoOptions) const;

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;
};

inline String* String::fromCharCode(char32_t code)
{
    if (code < 128) {
        char c = (char)code;
        ASCIIStringData s(&c, 1);
        return new ASCIIString(std::move(s));
    } else if (code <= 0x10000) {
        char16_t c = (char16_t)code;
        UTF16StringData s(&c, 1);
        return new UTF16String(std::move(s));
    } else {
        char16_t buf[3];
        buf[0] = (char16_t)(0xD800 + ((code - 0x10000) >> 10));
        buf[1] = (char16_t)(0xDC00 + ((code - 0x10000) & 1023));
        buf[2] = 0;
        return new UTF16String(buf, 2);
    }
}
}

namespace std {
template <>
struct hash<Escargot::String*> {
    size_t operator()(Escargot::String* const& x) const
    {
        return x->hashValue();
    }
};

template <>
struct equal_to<Escargot::String*> {
    bool operator()(Escargot::String* const& a, Escargot::String* const& b) const
    {
        return a->equals(b);
    }
};
}

#include "runtime/RopeString.h"
#include "runtime/StringBuilder.h"
#include "runtime/StringView.h"

namespace Escargot {
typedef std::unordered_map<String*, String*, std::hash<String*>, std::equal_to<String*>, GCUtil::gc_malloc_allocator<std::pair<String* const, String*>>> StringMapStd;
class StringMap : public StringMapStd, public gc {
public:
    String* at(String* s) const
    {
        auto iter = find(s);
        if (iter != end()) {
            return iter->second;
        }
        return String::emptyString;
    }
};
}

#endif
