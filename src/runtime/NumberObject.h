#ifndef __EscargotNumberObject__
#define __EscargotNumberObject__

#include "runtime/Object.h"

namespace Escargot {

class NumberObject : public Object {
public:
    NumberObject(ExecutionState& state, double value);

    double primitiveValue()
    {
        return m_primitiveValue;
    }
protected:
    double m_primitiveValue;
};

}

#endif
