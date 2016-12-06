#include "Escargot.h"
#include "BooleanObject.h"
#include "Context.h"

namespace Escargot {

BooleanObject::BooleanObject(ExecutionState& state, bool value)
    : Object(state)
    , m_primitiveValue(value)
{
    setPrototype(state, state.context()->globalObject()->booleanPrototype());
}

}
