#ifndef __EscargotPromiseObject__
#define __EscargotPromiseObject__

#if ESCARGOT_ENABLE_PROMISE

#include "runtime/Object.h"

namespace Escargot {

class PromiseObject : public Object {
public:
    PromiseObject(ExecutionState& state);

    virtual bool isPromiseObject()
    {
        return true;
    }

    // http://www.ecma-international.org/ecma-262/5.1/#sec-8.6.2
    virtual const char* internalClassProperty()
    {
        return "Promise";
    }

protected:
};
}

#endif // ESCARGOT_ENABLE_PROMISE

#endif // __EscargotPromiseObject__
