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

    virtual bool hasASCIIContent()
    {
        return m_string->hasASCIIContent();
    }

protected:
    String* m_string;
    size_t m_start, m_end;
};
}

#endif
