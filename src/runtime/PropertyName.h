#ifndef __EscargotPropertyName__
#define __EscargotPropertyName__

#include "runtime/ExecutionState.h"
#include "runtime/String.h"
#include "runtime/AtomicString.h"

namespace Escargot {

class PropertyName {
public:
    PropertyName()
    {
        m_data = ((size_t)AtomicString().string()) | 1;
    }

    PropertyName(ExecutionState& state, const AtomicString& atomicString)
    {
        m_data = ((size_t)atomicString.string() | 1);
    }

    PropertyName(ExecutionState& state, String* string);

    String* string() const
    {
        if (hasAtomicString()) {
            return (String*)(m_data - 1);
        } else {
            return (String*)m_data;
        }
    }

    inline friend bool operator == (const PropertyName& a, const PropertyName& b);
    inline friend bool operator != (const PropertyName& a, const PropertyName& b);
protected:

    bool hasAtomicString() const
    {
        return m_data & 1;
    }

    size_t m_data;
    // AtomicString <- saves its (String* | 1)
    // String* <- saves pointer
};

inline bool operator == (const PropertyName& a, const PropertyName& b)
{
    if (LIKELY(a.hasAtomicString()) == LIKELY(b.hasAtomicString())) {
        return a.m_data == b.m_data;
    } else {
        return a.string()->equals(b.string());
    }
}

inline bool operator != (const PropertyName& a, const PropertyName& b)
{
    return !operator==(a, b);
}

}

#endif
