#ifndef __EscargotAtomicString__
#define __EscargotAtomicString__

#include "runtime/String.h"
#include "runtime/ExecutionState.h"
#include "util/Vector.h"

namespace Escargot {

typedef std::unordered_map<std::pair<const char *, size_t>, String*,
    std::hash<std::pair<const char *, size_t> >, std::equal_to<std::pair<const char *, size_t> >,
    gc_allocator<std::pair<const std::pair<const char *, size_t>, String*> > > AtomicStringMap;

class AtomicString {
    friend class StaticStrings;
    friend class PropertyName;
    inline AtomicString(String* str)
    {
        m_string = str;
    }
public:
    inline AtomicString()
    {
        m_string = String::emptyString;
    }

    inline AtomicString(const AtomicString& src)
    {
        m_string = src.m_string;
    }

    AtomicString(ExecutionState& ec, String* name);
    AtomicString(ExecutionState& ec, const char16_t* src, size_t len);
    AtomicString(ExecutionState& ec, const char* src, size_t len);
    AtomicString(Context* c, const char16_t* src, size_t len);
    AtomicString(Context* c, const char* src, size_t len);
    AtomicString(Context* c, const StringView& sv);

    inline String* string() const
    {
        return m_string;
    }

    operator String*() const
    {
        return string();
    }

    inline friend bool operator == (const AtomicString& a, const AtomicString& b);
    inline friend bool operator != (const AtomicString& a, const AtomicString& b);

    bool operator== (const char* src) const
    {
        size_t srcLen = strlen(src);
        if (srcLen != m_string->length()) {
            return false;
        }

        for (size_t i = 0; i < srcLen; i ++) {
            if (src[i] != m_string->charAt(i)) {
                return false;
            }
        }

        return true;
    }


protected:
    void init(AtomicStringMap* ec, const char* src, size_t len);
    void init(AtomicStringMap* ec, const char16_t* src, size_t len);
    String* m_string;
};

COMPILE_ASSERT(sizeof (AtomicString) == sizeof (size_t), "");

inline bool operator == (const AtomicString& a, const AtomicString& b)
{
    return a.string() == b.string();
}

inline bool operator != (const AtomicString& a, const AtomicString& b)
{
    return !operator==(a, b);
}

inline size_t stringHash(const char* src, size_t length)
{
    size_t hash = static_cast<size_t>(0xc70f6907UL);
    for (; length; --length)
        hash = (hash * 131) + *src++;
    return hash;
}

typedef Vector<AtomicString, gc_allocator_ignore_off_page<AtomicString> > AtomicStringVector;

}

namespace std {
template<> struct hash<std::pair<const char *, size_t> > {
    size_t operator()(std::pair<const char *, size_t> const &x) const
    {
        return Escargot::stringHash(x.first, x.second);
    }
};

template<> struct equal_to<std::pair<const char *, size_t> > {
    bool operator()(std::pair<const char *, size_t> const &a, std::pair<const char *, size_t> const &b) const
    {
        if (a.second == b.second) {
            return memcmp(a.first, b.first, sizeof(char) * a.second) == 0;
        }
        return false;
    }
};

}

#endif
