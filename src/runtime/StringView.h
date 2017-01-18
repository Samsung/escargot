#ifndef __EscargotStringView__
#define __EscargotStringView__

#include "runtime/String.h"

namespace Escargot {

class StringView : public String {
public:
    StringView(String* str, const size_t& s, const size_t& e)
        : m_string(str)
        , m_start(s)
        , m_end(e)
    {
        ASSERT(s <= e);
        ASSERT(e <= m_string->length());
    }

    StringView(const StringView& str, const size_t& s, const size_t& e)
        : StringView(str.string(), s + str.start(), e + str.start())
    {
    }

    StringView()
    {
        m_string = String::emptyString;
        m_start = 0;
        m_end = 0;
    }

    virtual bool isStringView()
    {
        return true;
    }

    virtual char16_t charAt(const size_t& idx) const
    {
        return m_string->charAt(idx + m_start);
    }

    virtual size_t length() const
    {
        return m_end - m_start;
    }

    bool operator==(const char* src) const
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

    bool operator!=(const char* src) const
    {
        return !operator==(src);
    }

    virtual UTF16StringData toUTF16StringData() const
    {
        UTF16StringData ret;
        size_t len = length();
        ret.resizeWithUninitializedValues(len);

        for (size_t i = 0; i < len; i++) {
            ret[i] = charAt(i);
        }

        return ret;
    }

    virtual UTF8StringData toUTF8StringData() const
    {
        // FIXME optimze this function
        UTF16StringData s = toUTF16StringData();
        return utf16StringToUTF8String(s.data(), s.length());
    }

    String* string() const
    {
        return m_string;
    }

    size_t start() const
    {
        return m_start;
    }

    size_t end() const
    {
        return m_end;
    }

    virtual bool hasASCIIContent() const
    {
        return m_string->hasASCIIContent();
    }

    virtual const char* characters8() const
    {
        return m_string->characters8() + m_start;
    }

    virtual const char16_t* characters16() const
    {
        return m_string->characters16() + m_start;
    }

    virtual StringBufferAccessData bufferAccessData() const
    {
        StringBufferAccessData data;
        data.hasASCIIContent = m_string->hasASCIIContent();
        data.length = m_end - m_start;
        if (data.hasASCIIContent) {
            data.buffer = m_string->characters8() + m_start;
        } else {
            data.buffer = m_string->characters16() + m_start;
        }
        return data;
    }

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

protected:
    String* m_string;
    size_t m_start, m_end;
};

class BufferedStringView : public String {
    MAKE_STACK_ALLOCATED();

public:
    BufferedStringView()
        : BufferedStringView(StringView())
    {
    }

    BufferedStringView(StringView src)
        : m_src(src)
    {
        m_hasASCIIContent = src.hasASCIIContent();
        if (m_hasASCIIContent) {
            m_buffer = src.characters8();
        } else {
            m_buffer = src.characters16();
        }
    }

    char16_t bufferedCharAt(const size_t& idx) const
    {
        if (LIKELY(m_hasASCIIContent)) {
            return ((const char*)m_buffer)[idx];
        } else {
            return ((const char16_t*)m_buffer)[idx];
        }
    }

    virtual char16_t charAt(const size_t& idx) const
    {
        return m_src.charAt(idx);
    }

    virtual size_t length() const
    {
        return m_src.length();
    }

    virtual UTF16StringData toUTF16StringData() const
    {
        UTF16StringData ret;
        size_t len = length();
        ret.resizeWithUninitializedValues(len);

        for (size_t i = 0; i < len; i++) {
            ret[i] = charAt(i);
        }

        return ret;
    }

    virtual UTF8StringData toUTF8StringData() const
    {
        // FIXME optimze this function
        UTF16StringData s = toUTF16StringData();
        return utf16StringToUTF8String(s.data(), s.length());
    }

    virtual bool hasASCIIContent() const
    {
        return m_hasASCIIContent;
    }

    virtual const char* characters8() const
    {
        return m_src.characters8();
    }

    virtual const char16_t* characters16() const
    {
        return m_src.characters16();
    }

    virtual StringBufferAccessData bufferAccessData() const
    {
        return m_src.bufferAccessData();
    }

    StringView src() const
    {
        return m_src;
    }

protected:
    bool m_hasASCIIContent;
    const void* m_buffer;
    StringView m_src;
};
}

#endif
