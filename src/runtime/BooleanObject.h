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

    virtual bool isBooleanObject() const
    {
        return true;
    }

    // http://www.ecma-international.org/ecma-262/5.1/#sec-8.6.2
    virtual const char* internalClassProperty()
    {
        return "Boolean";
    }

    void* operator new(size_t size);
    void* operator new[](size_t size) = delete;

protected:
    bool m_primitiveValue;
};
}

#endif
