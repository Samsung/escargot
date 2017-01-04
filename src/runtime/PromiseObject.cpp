#if ESCARGOT_ENABLE_PROMISE
#include "Escargot.h"
#include "PromiseObject.h"
#include "Context.h"

namespace Escargot {

PromiseObject::PromiseObject(ExecutionState& state)
    : Object(state)
{
    setPrototype(state, state.context()->globalObject()->promisePrototype());
}
}

#endif // ESCARGOT_ENABLE_PROMISE
