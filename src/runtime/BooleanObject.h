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

protected:
    bool m_primitiveValue;
};
}

#endif
