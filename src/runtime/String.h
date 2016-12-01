#ifndef __EscargotString__
#define __EscargotString__

#include <string>
#include "runtime/PointerValue.h"
#include "util/BasicString.h"

namespace Escargot {

typedef BasicString<char, gc_malloc_atomic_ignore_off_page_allocator<char> > ASCIIStringData;
typedef BasicString<char, gc_malloc_atomic_ignore_off_page_allocator<char> > UTF8StringData;
typedef BasicString<char16_t, gc_malloc_atomic_ignore_off_page_allocator<char16_t> > UTF16StringData;
typedef BasicString<char32_t, gc_malloc_atomic_ignore_off_page_allocator<char32_t> > UTF32StringData;

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

    virtual bool isUTF16String()
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

    virtual size_t length() const = 0;

    virtual char16_t charAt(const size_t& idx) const = 0;
    char16_t operator[](const size_t& idx) const
    {
        return charAt(idx);
    }

    bool equals(String* src)
    {
        size_t srcLen = src->length();
        if (srcLen != length()) {
            return false;
        }

        for (size_t i = 0; i < srcLen; i ++) {
            if (src->charAt(i) != charAt(i)) {
                return false;
            }
        }

        return true;
    }

    bool equals(const char* src)
    {
        size_t srcLen = strlen(src);
        if (srcLen != length()) {
            return false;
        }

        for (size_t i = 0; i < srcLen; i ++) {
            if (src[i] != charAt(i)) {
                return false;
            }
        }

        return true;
    }

    // NOTE these function generates new copy of string data
    virtual UTF16StringData toUTF16StringData() const = 0;
    virtual UTF8StringData toUTF8StringData() const = 0;
    static String* emptyString;


protected:
    // NOTE this for atomic string
    // don't use this function anywhere
    inline const char* asASCIIStringData();
    inline const char16_t* asUTF16StringData();
};

class ASCIIString : public String {
    friend class String;
public:
    virtual bool isASCIIString()
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
    virtual bool isUTF16String()
    {
        return true;
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

const char16_t* String::asUTF16StringData()
{
    ASSERT(isUTF16String());
    return ((UTF16String*)this)->m_stringData.data();
}


bool isAllASCII(const char* buf, const size_t& len);
bool isAllASCII(const char16_t* buf, const size_t& len);
char32_t readUTF8Sequence(const char*& sequence, bool& valid, int& charlen);
UTF16StringData utf8StringToUTF16String(const char* buf, const size_t& len);
UTF8StringData utf16StringToUTF8String(const char16_t* buf, const size_t& len);
ASCIIStringData utf16StringToASCIIString(const char16_t* buf, const size_t& len);
ASCIIStringData dtoa(double number);

inline String* fromCharCode(char32_t code)
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

#include "runtime/StringView.h"

#endif

