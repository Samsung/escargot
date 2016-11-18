#ifndef __EscargotObjectGlobalObject__
#define __EscargotObjectGlobalObject__

#include "runtime/Object.h"

namespace Escargot {

class GlobalObject : public Object {
public:
    GlobalObject(ExecutionState& state)
        : Object(state)
    {

    }
};

}

#endif
