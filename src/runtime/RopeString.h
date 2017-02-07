#ifndef __EscargotRopeString__
#define __EscargotRopeString__

#include "runtime/String.h"

namespace Escargot {

class RopeString : public String {
public:
    RopeString(String* left, String* right)
        : String()
    {
        m_left = left;
        m_right = right;
    }

    virtual size_t length() const
    {
        if (m_right) {
            return m_left->length() + m_right->length();
        }
        return m_left->length();
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
};
}

#endif
