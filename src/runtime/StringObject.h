#ifndef __EscargotStringObject__
#define __EscargotStringObject__

#include "runtime/Object.h"

namespace Escargot {

class StringObject : public Object {
public:
    StringObject(ExecutionState& state, String* value);

    String* primitiveValue()
    {
        return m_primitiveValue;
    }

    virtual bool isStringObject()
    {
        return true;
    }
protected:
    String* m_primitiveValue;
};

}

#endif
