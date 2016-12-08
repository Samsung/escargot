#include "Escargot.h"
#include "NumberObject.h"
#include "Context.h"

namespace Escargot {

NumberObject::NumberObject(ExecutionState& state, double value)
    : Object(state)
    , m_primitiveValue(value)
{
    setPrototype(state, state.context()->globalObject()->numberPrototype());
}
}
