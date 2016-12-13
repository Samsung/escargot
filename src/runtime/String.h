#ifndef __EscargotString__
#define __EscargotString__

#include "runtime/PointerValue.h"
#include "util/BasicString.h"
#include <string>

namespace Escargot {

typedef BasicString<char, gc_malloc_atomic_ignore_off_page_allocator<char>> ASCIIStringData;
typedef BasicString<char, gc_malloc_atomic_ignore_off_page_allocator<char>> UTF8StringData;
typedef BasicString<char16_t, gc_malloc_atomic_ignore_off_page_allocator<char16_t>> UTF16StringData;
typedef BasicString<char32_t, gc_malloc_atomic_ignore_off_page_allocator<char32_t>> UTF32StringData;

typedef std::basic_string<char, std::char_traits<char>> ASCIIStringDataNonGCStd;
typedef std::basic_string<char, std::char_traits<char>> UTF8StringDataNonGCStd;
typedef std::basic_string<char16_t, std::char_traits<char16_t>> UTF16StringDataNonGCStd;
typedef std::basic_string<char32_t, std::char_traits<char32_t>> UTF32StringDataNonGCStd;

class ASCIIString;
class UTF16String;

class String : public PointerValue {
    friend class AtomicString;

public:
    virtual Type type()
    {
        return StringType;
    }

    virtual bool isString()
    {
        return true;
    }

    virtual bool isASCIIString()
    {
        return false;
    }

    virtual bool isRopeString()
    {
        return false;
    }

    virtual bool hasASCIIContent()
    {
        return false;
    }

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

    bool equals(const String* src) const
    {
        size_t srcLen = src->length();
        if (srcLen != length()) {
            return false;
        }

        for (size_t i = 0; i < srcLen; i++) {
            if (src->charAt(i) != charAt(i)) {
                return false;
            }
        }

        return true;
    }

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
    String* subString(size_t pos, size_t len);

    bool operator==(const String& src) const
    {
        return equals(&src);
    }

    bool operator!=(const String& src) const
    {
        return !operator==(src);
    }

    ALWAYS_INLINE friend bool operator<(const String& a, const String& b);
    ALWAYS_INLINE friend bool operator<=(const String& a, const String& b);
    ALWAYS_INLINE friend bool operator>(const String& a, const String& b);
    ALWAYS_INLINE friend bool operator>=(const String& a, const String& b);

    // NOTE these function generates new copy of string data
    virtual UTF16StringData toUTF16StringData() const = 0;
    virtual UTF8StringData toUTF8StringData() const = 0;
    static String* emptyString;

    uint64_t tryToUseAsIndex() const;
    uint32_t tryToUseAsArrayIndex() const;

protected:
    // NOTE this for atomic string
    // don't use this function anywhere
    inline const char* asASCIIStringData();
    inline const char16_t* asUTF16StringData();

    static int stringCompare(size_t l1, size_t l2, const String* c1, const String* c2);
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

class ASCIIString : public String {
    friend class String;

public:
    virtual bool isASCIIString()
    {
        return true;
    }

    virtual bool hasASCIIContent()
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

    virtual char16_t charAt(const size_t& idx) const
    {
        return m_stringData[idx];
    }

    virtual size_t length() const
    {
        return m_stringData.size();
    }

    virtual UTF16StringData toUTF16StringData() const;
    virtual UTF8StringData toUTF8StringData() const;

protected:
    ASCIIStringData m_stringData;
};

class UTF16String : public String {
    friend class String;

public:
    virtual bool hasASCIIContent()
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

    virtual char16_t charAt(const size_t& idx) const
    {
        return m_stringData[idx];
    }

    virtual size_t length() const
    {
        return m_stringData.size();
    }

    virtual UTF16StringData toUTF16StringData() const;
    virtual UTF8StringData toUTF8StringData() const;

protected:
    UTF16StringData m_stringData;
};

const char* String::asASCIIStringData()
{
    ASSERT(isASCIIString());
    return ((ASCIIString*)this)->m_stringData.data();
}

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

#include "runtime/RopeString.h"
#include "runtime/StringBuilder.h"
#include "runtime/StringView.h"
#endif
