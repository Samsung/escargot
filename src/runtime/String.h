/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
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
    size_t length;
    const void* buffer;
};

class String : public PointerValue {
    friend class AtomicString;

public:
    String()
    {
        m_tag = POINTER_VALUE_STRING_TAG_IN_DATA;
    }
    virtual Type type()
    {
        return StringType;
    }

    virtual bool isString() const
    {
        return true;
    }

    virtual bool isRopeString()
    {
        return false;
    }

    virtual bool isStringView()
    {
        return false;
    }

    virtual bool has8BitContent() const
    {
        return false;
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

    virtual StringBufferAccessData bufferAccessData() const = 0;

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
        auto data = bufferAccessData();
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

COMPILE_ASSERT(sizeof(String) == sizeof(size_t) * 2, "");

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
    virtual bool has8BitContent() const
    {
        return true;
    }

    ASCIIString(ASCIIStringData&& src)
        : String()
        , m_stringData(std::move(src))
    {
    }

    ASCIIString(const char* str)
        : String()
    {
        m_stringData.append(str, strlen(str));
    }

    ASCIIString(const char* str, size_t len)
        : String()
    {
        m_stringData.append(str, len);
    }

    ASCIIString(const char16_t* str, size_t len)
        : String()
    {
        m_stringData.resizeWithUninitializedValues(len);
        for (size_t i = 0; i < len; i++) {
            ASSERT(str[i] < 128);
            m_stringData[i] = str[i];
        }
    }

    virtual char16_t charAt(const size_t& idx) const
    {
        return m_stringData[idx];
    }

    virtual size_t length() const
    {
        return m_stringData.size();
    }

    virtual const LChar* characters8() const
    {
        return (const LChar*)m_stringData.data();
    }

    virtual StringBufferAccessData bufferAccessData() const
    {
        StringBufferAccessData data;
        data.has8BitContent = true;
        data.length = m_stringData.length();
        data.buffer = m_stringData.data();
        return data;
    }

    virtual UTF16StringData toUTF16StringData() const;
    virtual UTF8StringData toUTF8StringData() const;

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
    ASCIIStringData m_stringData;
};

class Latin1String : public String {
    friend class Latin1;

public:
    virtual bool has8BitContent() const
    {
        return true;
    }

    Latin1String(Latin1StringData&& src)
        : String()
        , m_stringData(std::move(src))
    {
    }

    Latin1String(const char* str)
        : String()
    {
        m_stringData.append((const LChar*)str, strlen(str));
    }

    Latin1String(const char* str, size_t len)
        : String()
    {
        m_stringData.append((const LChar*)str, len);
    }

    Latin1String(const LChar* str, size_t len)
        : String()
    {
        m_stringData.append(str, len);
    }

    virtual char16_t charAt(const size_t& idx) const
    {
        return m_stringData[idx];
    }

    virtual size_t length() const
    {
        return m_stringData.size();
    }

    virtual const LChar* characters8() const
    {
        return m_stringData.data();
    }

    virtual StringBufferAccessData bufferAccessData() const
    {
        StringBufferAccessData data;
        data.has8BitContent = true;
        data.length = m_stringData.length();
        data.buffer = m_stringData.data();
        return data;
    }

    virtual UTF16StringData toUTF16StringData() const;
    virtual UTF8StringData toUTF8StringData() const;

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
    Latin1StringData m_stringData;
};

class UTF16String : public String {
    friend class String;

public:
    virtual bool has8BitContent() const
    {
        return false;
    }

    UTF16String(UTF16StringData&& src)
        : String()
        , m_stringData(std::move(src))
    {
    }

    UTF16String(const char16_t* str, size_t len)
        : String()
    {
        m_stringData.append(str, len);
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
        m_stringData = std::move(str);
    }
#endif
    virtual char16_t charAt(const size_t& idx) const
    {
        return m_stringData[idx];
    }

    virtual size_t length() const
    {
        return m_stringData.size();
    }

    virtual const char16_t* characters16() const
    {
        return m_stringData.data();
    }

    virtual StringBufferAccessData bufferAccessData() const
    {
        StringBufferAccessData data;
        data.has8BitContent = false;
        data.length = m_stringData.length();
        data.buffer = m_stringData.data();
        return data;
    }

    virtual UTF16StringData toUTF16StringData() const;
    virtual UTF8StringData toUTF8StringData() const;

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

protected:
    UTF16StringData m_stringData;
};

bool isAllASCII(const char* buf, const size_t& len);
bool isAllASCII(const char16_t* buf, const size_t& len);
bool isIndexString(String* str);
char32_t readUTF8Sequence(const char*& sequence, bool& valid, int& charlen);
UTF16StringData utf8StringToUTF16String(const char* buf, const size_t& len);
UTF8StringData utf16StringToUTF8String(const char16_t* buf, const size_t& len);
ASCIIStringData utf16StringToASCIIString(const char16_t* buf, const size_t& len);
ASCIIStringData dtoa(double number);

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
#endif
