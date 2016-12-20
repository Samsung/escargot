#ifndef __EscargotBooleanObject__
#define __EscargotBooleanObject__

#include "runtime/Object.h"

namespace Escargot {

class BooleanObject : public Object {
public:
    BooleanObject(ExecutionState& state, bool value = false);

    bool primitiveValue()
    {
        return m_primitiveValue;
    }

    void setPrimitiveValue(ExecutionState& state, bool val)
    {
        m_primitiveValue = val;
    }

    virtual bool isBooleanObject()
    {
        return true;
    }

protected:
    bool m_primitiveValue;
};
}

#endif
