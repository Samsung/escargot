#ifndef __EscargotRopeString__
#define __EscargotRopeString__

#include "runtime/String.h"

namespace Escargot {

#define ESCARGOT_ROPE_STRING_MIN_LENGTH 24

class RopeString : public String {
protected:
    RopeString()
        : String()
    {
        m_contentLength = 0;
        m_hasASCIIContent = false;
        m_left = nullptr;
        m_right = nullptr;
    }

public:
    // this function not always create RopeString.
    // if (l+r).length() < ESCARGOT_ROPE_STRING_MIN_LENGTH
    // then create just normalString
    static String* createRopeString(String* lstr, String* rstr);
    virtual size_t length() const
    {
        return m_contentLength;
    }
    virtual char16_t charAt(const size_t& idx) const
    {
        return normalString()->charAt(idx);
    }
    virtual UTF16StringData toUTF16StringData() const
    {
        return normalString()->toUTF16StringData();
    }
    virtual UTF8StringData toUTF8StringData() const
    {
        return normalString()->toUTF8StringData();
    }
    virtual bool hasASCIIContent()
    {
        return m_hasASCIIContent;
    }
    virtual bool isRopeString()
    {
        return true;
    }

protected:
    String* normalString() const
    {
        const_cast<RopeString*>(this)->flattenRopeString();
        return m_left;
    }
    template <typename A, typename B>
    void flattenRopeStringWorker();
    void flattenRopeString();

    bool m_hasASCIIContent;
    String* m_left;
    String* m_right;
    size_t m_contentLength;
};
}

#endif
