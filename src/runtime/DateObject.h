#ifndef __EscargotDateObject__
#define __EscargotDateObject__

#include "runtime/Object.h"

namespace Escargot {

typedef int64_t time64_t;
typedef double time64IncludingNaN;

class DateObject : public Object {
public:
    DateObject(ExecutionState& state, time64IncludingNaN val);

    int64_t primitiveValue()
    {
        return m_primitiveValue;
    }

    void setPrimitiveValue(int64_t primitiveValue)
    {
        m_primitiveValue = primitiveValue;
    }

    void setPrimitiveValueAsCurrentTime();

    virtual bool isDateObject()
    {
        return true;
    }

private:
    int64_t m_primitiveValue;
};
}

#endif
