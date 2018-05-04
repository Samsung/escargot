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

#ifndef __EscargotString__
#define __EscargotString__

#include "runtime/PointerValue.h"
#include "util/BasicString.h"
#include <string>

namespace Escargot {

// A type to hold a single Latin-1 character.
typedef unsigned char LChar;

typedef BasicString<char, GCUtil::gc_malloc_atomic_ignore_off_page_allocator<char>> ASCIIStringData;
typedef BasicString<LChar, GCUtil::gc_malloc_atomic_ignore_off_page_allocator<LChar>> Latin1StringData;
typedef BasicString<char, GCUtil::gc_malloc_atomic_ignore_off_page_allocator<char>> UTF8StringData;
typedef BasicString<char16_t, GCUtil::gc_malloc_atomic_ignore_off_page_allocator<char16_t>> UTF16StringData;
typedef BasicString<char32_t, GCUtil::gc_malloc_atomic_ignore_off_page_allocator<char32_t>> UTF32StringData;

typedef std::basic_string<char, std::char_traits<char>> ASCIIStringDataNonGCStd;
typedef std::basic_string<LChar, std::char_traits<LChar>> Latin1StringDataNonGCStd;
typedef std::basic_string<char, std::char_traits<char>> UTF8StringDataNonGCStd;
typedef std::basic_string<char16_t, std::char_traits<char16_t>> UTF16StringDataNonGCStd;
typedef std::basic_string<char32_t, std::char_traits<char32_t>> UTF32StringDataNonGCStd;

class ASCIIString;
class Latin1String;
class UTF16String;
class RopeString;
class StringView;

struct StringBufferAccessData {
    bool has8BitContent;
    bool hasSpecialImpl;
    size_t length;
    const void* buffer;

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

    ALWAYS_INLINE bool equalsSameLength(const char* str, size_t compareStartAt = 0) const
    {
        ASSERT(strlen(str) == length);
        if (LIKELY(has8BitContent)) {
            for (size_t i = compareStartAt; i < length; i++) {
                if (str[i] != ((char*)buffer)[i]) {
                    return false;
                }
            }
            return true;
        } else {
            for (size_t i = compareStartAt; i < length; i++) {
                if (str[i] != ((char16_t*)buffer)[i]) {
                    return false;
                }
            }
            return true;
        }
    }
};

class String : public PointerValue {
    friend class AtomicString;

public:
    String()
    {
        m_tag = POINTER_VALUE_STRING_SYMBOL_TAG_IN_DATA;
        m_bufferAccessData.hasSpecialImpl = false;
    }

    virtual bool isString() const
    {
        return true;
    }

    virtual bool isRopeString()
    {
        return false;
    }

    bool has8BitContent() const
    {
        return bufferAccessData().has8BitContent;
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

    virtual size_t length() const = 0;
    virtual char16_t charAt(const size_t& idx) const = 0;
    char16_t operator[](const size_t& idx) const
    {
        return charAt(idx);
    }

    virtual void bufferAccessDataSpecialImpl()
    {
        ASSERT(m_bufferAccessData.hasSpecialImpl);
    }
    ALWAYS_INLINE const StringBufferAccessData& bufferAccessData() const
    {
        if (UNLIKELY(m_bufferAccessData.hasSpecialImpl)) {
            const_cast<String*>(this)->bufferAccessDataSpecialImpl();
        }
        return m_bufferAccessData;
    }

    bool equals(const String* src) const;
    bool equals(const char* src) const
    {
        size_t srcLen = strlen(src);
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

    size_t find(String* str, size_t pos = 0);
    size_t rfind(String* str, size_t pos);

    String* subString(size_t from, size_t to);

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
    virtual UTF8StringDataNonGCStd toNonGCUTF8StringData() const = 0;
    static String* emptyString;

    uint64_t tryToUseAsIndex() const;
    uint64_t tryToUseAsArrayIndex() const;

    bool is8Bit() const
    {
        return has8BitContent();
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

protected:
    size_t m_tag;
    StringBufferAccessData m_bufferAccessData;
    static int stringCompare(size_t l1, size_t l2, const String* c1, const String* c2);

    template <typename T>
    static ALWAYS_INLINE bool stringEqual(const T* s, const T* s1, const size_t& len)
    {
        return memcmp(s, s1, sizeof(T) * len) == 0;
    }

    static ALWAYS_INLINE bool stringEqual(const char16_t* s, const LChar* s1, const size_t& len)
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
    friend class String;

public:
    ASCIIString(ASCIIStringData&& src)
        : String()
    {
        ASCIIStringData stringData = std::move(src);
        initBufferAccessData(stringData);
    }

    ASCIIString(const char* str)
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

    virtual char16_t charAt(const size_t& idx) const
    {
        return m_bufferAccessData.uncheckedCharAtFor8Bit(idx);
    }

    virtual size_t length() const
    {
        return m_bufferAccessData.length;
    }

    virtual const LChar* characters8() const
    {
        return (const LChar*)m_bufferAccessData.buffer;
    }

    void initBufferAccessData(ASCIIStringData& stringData)
    {
        m_bufferAccessData.has8BitContent = true;
        m_bufferAccessData.length = stringData.length();
        m_bufferAccessData.buffer = stringData.takeBuffer();
    }

    virtual UTF16StringData toUTF16StringData() const;
    virtual UTF8StringData toUTF8StringData() const;
    virtual UTF8StringDataNonGCStd toNonGCUTF8StringData() const;

    void* operator new(size_t size);
    void* operator new(size_t size, GCPlacement p)
    {
        return gc::operator new(size, p);
    }
    void* operator new(size_t size, void* ptr)
    {
        return ptr;
    }
    void* operator new[](size_t size) = delete;

protected:
};

class Latin1String : public String {
    friend class Latin1;

public:
    Latin1String(Latin1StringData&& src)
        : String()
    {
        Latin1StringData data = std::move(src);
        initBufferAccessData(data);
    }

    Latin1String(const char* str)
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
        m_bufferAccessData.has8BitContent = true;
        m_bufferAccessData.length = stringData.length();
        m_bufferAccessData.buffer = stringData.takeBuffer();
    }

    virtual char16_t charAt(const size_t& idx) const
    {
        return m_bufferAccessData.uncheckedCharAtFor8Bit(idx);
    }

    virtual size_t length() const
    {
        return m_bufferAccessData.length;
    }

    virtual const LChar* characters8() const
    {
        return (const LChar*)m_bufferAccessData.buffer;
    }

    virtual UTF16StringData toUTF16StringData() const;
    virtual UTF8StringData toUTF8StringData() const;
    virtual UTF8StringDataNonGCStd toNonGCUTF8StringData() const;

    void* operator new(size_t size);
    void* operator new(size_t size, GCPlacement p)
    {
        return gc::operator new(size, p);
    }
    void* operator new(size_t size, void* ptr)
    {
        return ptr;
    }
    void* operator new[](size_t size) = delete;

protected:
};

class UTF16String : public String {
    friend class String;

public:
    UTF16String(UTF16StringData&& src)
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
#ifdef ENABLE_ICU
    UTF16String(icu::UnicodeString& src)
        : String()
    {
        UTF16StringData str;
        size_t srclen = src.length();
        for (size_t i = 0; i < srclen; i++) {
            str.push_back(src.charAt(i));
        }
        initBufferAccessData(str);
    }
#endif

    void initBufferAccessData(UTF16StringData& stringData)
    {
        m_bufferAccessData.has8BitContent = false;
        m_bufferAccessData.length = stringData.length();
        m_bufferAccessData.buffer = stringData.takeBuffer();
    }

    virtual char16_t charAt(const size_t& idx) const
    {
        return m_bufferAccessData.uncheckedCharAtFor16Bit(idx);
    }

    virtual size_t length() const
    {
        return m_bufferAccessData.length;
    }

    virtual const char16_t* characters16() const
    {
        return (const char16_t*)m_bufferAccessData.buffer;
    }

    virtual UTF16StringData toUTF16StringData() const;
    virtual UTF8StringData toUTF8StringData() const;
    virtual UTF8StringDataNonGCStd toNonGCUTF8StringData() const;

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

protected:
};

bool isAllASCII(const char* buf, const size_t& len);
bool isAllASCII(const char16_t* buf, const size_t& len);
bool isAllLatin1(const char16_t* buf, const size_t& len);
bool isIndexString(String* str);
char32_t readUTF8Sequence(const char*& sequence, bool& valid, int& charlen);
UTF16StringData utf8StringToUTF16String(const char* buf, const size_t& len);
UTF8StringData utf16StringToUTF8String(const char16_t* buf, const size_t& len);
ASCIIStringData utf16StringToASCIIString(const char16_t* buf, const size_t& len);
ASCIIStringData dtoa(double number);
size_t utf32ToUtf8(char32_t uc, char* UTF8);

inline String* String::fromCharCode(char32_t code)
{
    if (code < 128) {
        char c = (char)code;
        return new ASCIIString(std::move(ASCIIStringData(&c, 1)));
    } else if (code <= 0x10000) {
        char16_t c = (char16_t)code;
        return new UTF16String(std::move(UTF16StringData(&c, 1)));
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
typedef std::unordered_map<String*, String*, std::hash<String*>, std::equal_to<String*>, gc_allocator<std::pair<String*, String*>>> StringMapStd;
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
