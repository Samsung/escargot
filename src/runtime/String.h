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
        return (ASCIIString*)ptr;
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
        return (UTF16String*)ptr;
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

class ASCIIString : public String, public ASCIIStringData {
public:
    virtual bool isASCIIString()
    {
        return false;
    }

protected:
    ASCIIString(ASCIIStringData&& src)
        : String()
        , ASCIIStringData(std::move(src))
    {
        m_bufferRoot = ASCIIString::data();
#ifndef NDEBUG
        m_asciiString = asASCIIString();
        m_utf16String = nullptr;
#endif
    }

    // FIXME
    // for protect string buffer
    // gcc stores buffer of basic_string with special way
    // so we should root buffer manually
    const void* m_bufferRoot;
};

class UTF16String : public String, public UTF16StringData {
public:
    virtual bool isASCIIString()
    {
        return false;
    }

protected:
    UTF16String(UTF16StringData&& src)
        : String()
        , UTF16StringData(std::move(src))
    {
        m_bufferRoot = UTF16String::data();
#ifndef NDEBUG
        m_asciiString = nullptr;
        m_utf16String = asUTF16String();
#endif
    }

    // FIXME
    // for protect string buffer
    // gcc stores buffer of basic_string with special way
    // so we should root buffer manually
    const void* m_bufferRoot;
};


}

#endif

