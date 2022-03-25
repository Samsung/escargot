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
#include "util/Vector.h"
#include <string>

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
std::vector<std::string> split(const std::string& s, const std::string& seperator);

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
ASCIIStringDataNonGCStd dtoa(double number);
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
class ReloadableString;
class RopeString;
class StringView;
class VMInstance;

class StringWriteOption {
public:
    enum {
        NoOptions = 0,
        ReplaceInvalidUtf8 = 1 << 3
    };
};

struct StringBufferAccessData {
    // should be allocated on the stack
    MAKE_STACK_ALLOCATED();

    bool has8BitContent;
    size_t length;
    union {
        const void* buffer;
        const char* bufferAs8Bit;
        const char16_t* bufferAs16Bit;
    };
    void* extraData;

    StringBufferAccessData(bool has8Bit, size_t len, void* buffer, void* extraDataToKeep = nullptr)
        : has8BitContent(has8Bit)
        , length(len)
        , buffer(buffer)
        , extraData(extraDataToKeep)
    {
#if defined(ENABLE_COMPRESSIBLE_STRING) || defined(ENABLE_RELOADABLE_STRING)
        if (extraData) {
            // increase refCount in CompressibleString or ReloadableString
            (*reinterpret_cast<size_t*>(extraData))++;
        }
#endif
    }

    ~StringBufferAccessData()
    {
#if defined(ENABLE_COMPRESSIBLE_STRING) || defined(ENABLE_RELOADABLE_STRING)
        if (extraData) {
            // decrease refCount in CompressibleString or ReloadableString
            size_t& count = *reinterpret_cast<size_t*>(extraData);
            ASSERT(count > 0);
            count--;
        }
#endif
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
    }

    struct StringBufferData {
        StringBufferData()
            : has8BitContent(true)
            , hasSpecialImpl(false)
            , length(0)
            , buffer(nullptr)
        {
        }

        union {
            struct {
                bool has8BitContent : 1;
                bool hasSpecialImpl : 1;
#if defined(ESCARGOT_32)
                size_t length : 30;
#else
                size_t length : 62;
#endif
            };
            size_t valueShouldBeOddForFewTypes;
        };

        static constexpr size_t bufferPointerAsArraySize = sizeof(size_t);
        union {
            const void* buffer;
            const char* bufferAs8Bit;
            const char16_t* bufferAs16Bit;
            String* bufferAsString;
            LChar bufferPointerAsArray[bufferPointerAsArraySize];
            char16_t bufferPointerAs16BitArray[bufferPointerAsArraySize / 2];
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
    virtual bool isStringView()
    {
        return false;
    }

    virtual bool isCompressibleString()
    {
        return false;
    }

    virtual bool isReloadableString()
    {
        return false;
    }

    ReloadableString* asReloadableString()
    {
        ASSERT(isReloadableString());
        return (ReloadableString*)this;
    }

    virtual bool isRopeString()
    {
        return false;
    }

    RopeString* asRopeString()
    {
        ASSERT(isRopeString());
        return (RopeString*)this;
    }

    bool has8BitContent() const
    {
        return m_bufferData.has8BitContent;
    }

    virtual bool hasExternalMemory()
    {
        return false;
    }

    // initialize String::emptyString value
    // its called only once by VMInstance constructor
    static void initEmptyString();

    template <const size_t srcLen>
    static String* fromASCII(const char (&src)[srcLen])
    {
        ASSERT(srcLen - 1 == strlen(src));
        return fromASCII(src, srcLen - 1);
    }

    static String* fromASCII(const char* s, size_t len);

    // if you want to change this value, you  should change LATIN1_LARGE_INLINE_BUFFER macro in String.cpp
#define LATIN1_LARGE_INLINE_BUFFER_MAX_SIZE 24
    static String* fromLatin1(const LChar* s, size_t len);
    static String* fromLatin1(const char16_t* s, size_t len);

    static String* fromCharCode(char32_t code);
    static String* fromDouble(double v);
    static String* fromInt32(int32_t v)
    {
        // TODO
        return fromDouble(v);
    }
    static String* fromUTF8(const char* src, size_t len, bool maybeASCII = true);
#if defined(ENABLE_COMPRESSIBLE_STRING)
    static String* fromUTF8ToCompressibleString(VMInstance* instance, const char* src, size_t len, bool maybeASCII = true);
#endif

    static String* getSubstitution(ExecutionState& state, String* matched, String* str, size_t position, StringVector& captures, Value namedCapture, String* replacement);

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

    bool isAtomicStringSource() const
    {
        return (m_tag > POINTER_VALUE_STRING_TAG_IN_DATA);
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

    size_t find(String* str, size_t pos = 0);
    size_t find(const char* str, size_t len, size_t pos = 0) const;

    template <size_t N>
    size_t find(const char (&src)[N], size_t pos = 0) const
    {
        ASSERT(N - 1 == strlen(src));
        return find(src, N - 1, 0);
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
    virtual UTF16StringData toUTF16StringData() const
    {
        UTF16StringData ret;
        size_t len = length();
        ret.resizeWithUninitializedValues(len);

        auto bad = bufferAccessData();
        for (size_t i = 0; i < len; i++) {
            ret[i] = bad.charAt(i);
        }

        return ret;
    }

    virtual UTF8StringData toUTF8StringData() const
    {
        return bufferAccessData().toUTF8String<UTF8StringData, UTF8StringDataNonGCStd>();
    }

    virtual UTF8StringDataNonGCStd toNonGCUTF8StringData(int options = StringWriteOption::NoOptions) const
    {
        return bufferAccessData().toUTF8String<UTF8StringDataNonGCStd>();
    }

    static MAY_THREAD_LOCAL String* emptyString;

    uint64_t tryToUseAsIndex() const;
    uint32_t tryToUseAsIndex32() const;
    uint32_t tryToUseAsIndexProperty() const;

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

    uint64_t advanceStringIndex(uint64_t index, bool unicode);

    bool isAllSpecialCharacters(bool (*fn)(char));

    enum StringTrimWhere : unsigned {
        TrimStart,
        TrimEnd,
        TrimBoth
    };
    String* trim(StringTrimWhere where = StringTrimWhere::TrimBoth);

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

#if defined(NDEBUG) && defined(ESCARGOT_32) && !defined(COMPILER_MSVC)
COMPILE_ASSERT(sizeof(String) == sizeof(size_t) * 4, "");
#endif

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
    explicit ASCIIString()
        : String()
    {
        ASCIIStringData stringData;
        initBufferAccessData(stringData);
    }

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

    enum FromGCBuffer {
        FromGCBufferTag
    };
    ASCIIString(char* str, size_t len, FromGCBuffer)
        : String()
    {
        m_bufferData.has8BitContent = true;
        m_bufferData.length = len;
        m_bufferData.buffer = str;
    }

    virtual char16_t charAt(const size_t idx) const override
    {
        return m_bufferData.uncheckedCharAtFor8Bit(idx);
    }

    virtual const LChar* characters8() const override
    {
        return (const LChar*)m_bufferData.buffer;
    }

    void initBufferAccessData(ASCIIStringData& stringData)
    {
        m_bufferData.has8BitContent = true;
        m_bufferData.length = stringData.length();
        m_bufferData.buffer = stringData.takeBuffer();

        ASSERT(m_bufferData.valueShouldBeOddForFewTypes & 1);
    }

    virtual UTF16StringData toUTF16StringData() const override;
    virtual UTF8StringData toUTF8StringData() const override;
    virtual UTF8StringDataNonGCStd toNonGCUTF8StringData(int options = StringWriteOption::NoOptions) const override;

    void* operator new(size_t size)
    {
        return GC_MALLOC(size);
    }
    void* operator new(size_t size, GCPlacement p)
    {
        return gc::operator new(size, p);
    }
    void* operator new[](size_t size) = delete;
};

class ASCIIStringFromExternalMemory : public ASCIIString {
public:
    ASCIIStringFromExternalMemory(const char* str, size_t len)
        : ASCIIString()
    {
        m_bufferData.buffer = str;
        m_bufferData.length = len;
        m_bufferData.hasSpecialImpl = false;
        m_bufferData.has8BitContent = true;
    }

    virtual bool hasExternalMemory() override
    {
        return true;
    }
};

class ASCIIStringWithInlineBuffer : public String {
public:
    ASCIIStringWithInlineBuffer(const char* str, size_t len)
        : String()
    {
        ASSERT(len <= m_bufferData.bufferPointerAsArraySize);
        memcpy(m_bufferData.bufferPointerAsArray, str, len);
        m_bufferData.length = len;
        m_bufferData.hasSpecialImpl = true;
        m_bufferData.has8BitContent = true;
    }

    void* operator new(size_t size)
    {
        return GC_MALLOC_ATOMIC(size);
    }

    virtual StringBufferAccessData bufferAccessDataSpecialImpl() override
    {
        return StringBufferAccessData(true, m_bufferData.length, &m_bufferData.bufferPointerAsArray);
    }

    virtual const LChar* characters8() const override
    {
        return (LChar*)&m_bufferData.bufferPointerAsArray;
    }
};

class Latin1String : public String {
public:
    explicit Latin1String()
        : String()
    {
        Latin1StringData data;
        initBufferAccessData(data);
    }

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

        ASSERT(m_bufferData.valueShouldBeOddForFewTypes & 1);
    }

    virtual char16_t charAt(const size_t idx) const override
    {
        return m_bufferData.uncheckedCharAtFor8Bit(idx);
    }

    virtual const LChar* characters8() const override
    {
        return (const LChar*)m_bufferData.buffer;
    }

    virtual UTF16StringData toUTF16StringData() const override;
    virtual UTF8StringData toUTF8StringData() const override;
    virtual UTF8StringDataNonGCStd toNonGCUTF8StringData(int options = StringWriteOption::NoOptions) const override;

    void* operator new(size_t size)
    {
        return GC_MALLOC(size);
    }
};

class Latin1StringFromExternalMemory : public Latin1String {
public:
    Latin1StringFromExternalMemory(const unsigned char* str, size_t len)
        : Latin1String()
    {
        m_bufferData.buffer = str;
        m_bufferData.length = len;
        m_bufferData.hasSpecialImpl = false;
        m_bufferData.has8BitContent = true;
    }

    virtual bool hasExternalMemory() override
    {
        return true;
    }
};

class Latin1StringWithInlineBuffer : public String {
public:
    Latin1StringWithInlineBuffer(const LChar* str, size_t len)
        : String()
    {
        ASSERT(len <= m_bufferData.bufferPointerAsArraySize);
        memcpy(m_bufferData.bufferPointerAsArray, str, len);
        m_bufferData.length = len;
        m_bufferData.hasSpecialImpl = true;
        m_bufferData.has8BitContent = true;
    }

    void* operator new(size_t size)
    {
        return GC_MALLOC_ATOMIC(size);
    }

    virtual StringBufferAccessData bufferAccessDataSpecialImpl() override
    {
        return StringBufferAccessData(true, m_bufferData.length, &m_bufferData.bufferPointerAsArray);
    }

    virtual const LChar* characters8() const override
    {
        return (LChar*)&m_bufferData.bufferPointerAsArray;
    }
};

template <const int bufferSize>
class Latin1StringWithLargeInlineBuffer : public String {
public:
    Latin1StringWithLargeInlineBuffer(const LChar* str, size_t len)
        : String()
    {
        ASSERT(len <= bufferSize);
        m_bufferData.buffer = m_buffer;
        m_bufferData.length = len;
        m_bufferData.hasSpecialImpl = false;
        m_bufferData.has8BitContent = true;
        memcpy(m_buffer, str, len);
    }

    void* operator new(size_t size)
    {
        return GC_MALLOC_ATOMIC(size);
    }

    virtual const LChar* characters8() const override
    {
        return m_buffer;
    }

private:
    LChar m_buffer[bufferSize];
};

class UTF16String : public String {
public:
    explicit UTF16String()
        : String()
    {
        UTF16StringData data;
        initBufferAccessData(data);
    }

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

    void initBufferAccessData(UTF16StringData& stringData)
    {
        m_bufferData.has8BitContent = false;
        m_bufferData.length = stringData.length();
        m_bufferData.buffer = stringData.takeBuffer();
    }

    virtual char16_t charAt(const size_t idx) const override
    {
        return m_bufferData.uncheckedCharAtFor16Bit(idx);
    }

    virtual const char16_t* characters16() const override
    {
        return (const char16_t*)m_bufferData.buffer;
    }

    virtual UTF16StringData toUTF16StringData() const override;
    virtual UTF8StringData toUTF8StringData() const override;
    virtual UTF8StringDataNonGCStd toNonGCUTF8StringData(int options = StringWriteOption::NoOptions) const override;

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;
};

class UTF16StringFromExternalMemory : public UTF16String {
public:
    UTF16StringFromExternalMemory(const char16_t* str, size_t len)
        : UTF16String()
    {
        m_bufferData.buffer = str;
        m_bufferData.length = len;
        m_bufferData.hasSpecialImpl = false;
        m_bufferData.has8BitContent = false;
    }

    virtual bool hasExternalMemory() override
    {
        return true;
    }
};

class UTF16StringWithInlineBuffer : public String {
public:
    UTF16StringWithInlineBuffer(const char16_t* str, size_t len)
        : String()
    {
        ASSERT(len <= m_bufferData.bufferPointerAsArraySize / 2);
        memcpy(m_bufferData.bufferPointerAs16BitArray, str, len * 2);
        m_bufferData.length = len;
        m_bufferData.hasSpecialImpl = true;
        m_bufferData.has8BitContent = false;
    }

    void* operator new(size_t size)
    {
        return GC_MALLOC_ATOMIC(size);
    }

    virtual StringBufferAccessData bufferAccessDataSpecialImpl() override
    {
        return StringBufferAccessData(false, m_bufferData.length, &m_bufferData.bufferPointerAs16BitArray);
    }

    virtual UTF16StringData toUTF16StringData() const override
    {
        UTF16StringData ret;
        size_t len = length();
        ret.resizeWithUninitializedValues(len);

        auto bad = bufferAccessData();
        for (size_t i = 0; i < len; i++) {
            ret[i] = bad.charAt(i);
        }

        return ret;
    }

    virtual UTF8StringData toUTF8StringData() const override
    {
        return bufferAccessData().toUTF8String<UTF8StringData, UTF8StringDataNonGCStd>();
    }

    virtual UTF8StringDataNonGCStd toNonGCUTF8StringData(int options = StringWriteOption::NoOptions) const override
    {
        return bufferAccessData().toUTF8String<UTF8StringDataNonGCStd>();
    }

    virtual const char16_t* characters16() const override
    {
        return (char16_t*)&m_bufferData.bufferPointerAs16BitArray;
    }
};

} // namespace Escargot

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
} // namespace std

#include "runtime/RopeString.h"
#include "runtime/StringBuilder.h"
#include "runtime/StringView.h"

namespace Escargot {
typedef std::unordered_map<std::string, String*, std::hash<std::string>, std::equal_to<std::string>, GCUtil::gc_malloc_allocator<std::pair<std::string const, String*>>> StringMapStd;
class StringMap : public StringMapStd, public gc {
public:
    String* at(const std::string& s) const
    {
        auto iter = find(s);
        if (iter != end()) {
            return iter->second;
        }
        return String::emptyString;
    }

    void* operator new(size_t size) = delete;
    void* operator new[](size_t size) = delete;
};
} // namespace Escargot

#endif
