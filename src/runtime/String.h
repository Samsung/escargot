#ifndef __EscargotString__
#define __EscargotString__

#include <string>
#include "runtime/PointerValue.h"

namespace Escargot {

class ASCIIString;
class UTF16String;

class String : public PointerValue {
public:
    virtual Type type()
    {
        return StringType;
    }

    virtual bool isString()
    {
        return true;
    }

    virtual bool isASCIIString() = 0;
    ASCIIString* asASCIIString()
    {
        ASSERT(isASCIIString());
#ifndef NDEBUG
        size_t ptr = (size_t)this;
        ptr += sizeof(String);
        return (ASCIIString*)ptr;
#else
        return (ASCIIString*)this;
#endif
    }


    UTF16String* asUTF16String()
    {
        ASSERT(!isASCIIString());
#ifndef NDEBUG
        size_t ptr = (size_t)this;
        ptr += sizeof(String);
        return (UTF16String*)ptr;
#else
        return (UTF16String*)this;
#endif
    }

protected:
#ifndef NDEBUG
    ASCIIString* m_asciiString;
    UTF16String* m_utf16String;
#endif
};

typedef std::basic_string<char16_t, std::char_traits<char16_t>, gc_malloc_atomic_ignore_off_page_allocator<char16_t> > UTF16StringData;
typedef std::basic_string<char, std::char_traits<char>, gc_malloc_atomic_ignore_off_page_allocator<char> > ASCIIStringData;
typedef std::basic_string<char, std::char_traits<char>, gc_malloc_atomic_ignore_off_page_allocator<char> > UTF8StringData;

class ASCIIString : public String, public ASCIIStringData {
    void init()
    {
        m_bufferRoot = ASCIIStringData::data();
#ifndef NDEBUG
        m_asciiString = asASCIIString();
        m_utf16String = nullptr;
#endif
    }
public:
    virtual bool isASCIIString()
    {
        return false;
    }

    ASCIIString(ASCIIStringData&& src)
        : String()
        , ASCIIStringData(std::move(src))
    {
        init();
    }

    ASCIIString(char* str)
        : String()
        , ASCIIStringData(str)
    {
        init();
    }

protected:
    // FIXME
    // for protect string buffer
    // gcc stores buffer of basic_string with special way
    // so we should root buffer manually
    const void* m_bufferRoot;
};

class UTF16String : public String, public UTF16StringData {
    void init()
    {
        m_bufferRoot = UTF16StringData::data();
#ifndef NDEBUG
        m_asciiString = nullptr;
        m_utf16String = asUTF16String();
#endif
    }
public:
    virtual bool isASCIIString()
    {
        return false;
    }

    UTF16String(UTF16StringData&& src)
        : String()
        , UTF16StringData(std::move(src))
    {
        init();
    }

protected:
    // FIXME
    // for protect string buffer
    // gcc stores buffer of basic_string with special way
    // so we should root buffer manually
    const void* m_bufferRoot;
};


bool isAllASCII(const char* buf, const size_t& len);
bool isAllASCII(const char16_t* buf, const size_t& len);
char32_t readUTF8Sequence(const char*& sequence, bool& valid, int& charlen);
UTF16StringData utf8StringToUTF16String(const char* buf, const size_t& len);
UTF8StringData utf16StringToUTF8String(const char16_t* buf, const size_t& len);
ASCIIStringData utf16StringToASCIIString(const char16_t* buf, const size_t& len);
ASCIIStringData dtoa(double number);

}

#endif

