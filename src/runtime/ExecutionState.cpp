#include "Escargot.h"
#include "ExecutionState.h"
#include "ExecutionContext.h"
#include "Context.h"

namespace Escargot {

void ExecutionState::throwException(const Value& e)
{
    context()->throwException(*this, e);
}
}
