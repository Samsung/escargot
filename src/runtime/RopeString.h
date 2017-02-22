#ifndef __EscargotRopeString__
#define __EscargotRopeString__

#include "runtime/String.h"

namespace Escargot {

class ExecutionState;

class RopeString : public String {
public:
    RopeString()
        : String()
    {
        m_left = String::emptyString;
        m_right = String::emptyString;
        m_contentLength = 0;
    }

    // this function not always create RopeString.
    // if (l+r).length() < ROPE_STRING_MIN_LENGTH
    // then create just normalString
    // provide ExecutionState if you need limit of string length(exception can be thrown only in ExecutionState area)
    static String* createRopeString(String* lstr, String* rstr, ExecutionState* state = nullptr);

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

    virtual bool has8BitContent() const
    {
        if (m_right) {
            bool a = m_left->has8BitContent();
            if (!a)
                return false;
            bool b = m_right->has8BitContent();
            if (!b) {
                return false;
            }
            return true;
        }
        return m_left->has8BitContent();
    }

    virtual bool isRopeString()
    {
        return true;
    }

    virtual const LChar* characters8() const
    {
        return normalString()->characters8();
    }

    virtual const char16_t* characters16() const
    {
        return normalString()->characters16();
    }

    virtual StringBufferAccessData bufferAccessData() const
    {
        return normalString()->bufferAccessData();
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

    String* m_left;
    String* m_right;
    size_t m_contentLength;
};
}

#endif
