#include "Escargot.h"
#include "ExecutionContext.h"
#include "Environment.h"
#include "FunctionObject.h"

namespace Escargot {

#if ESCARGOT_ENABLE_PROMISE
FunctionObject* ExecutionContext::resolveCallee()
{
    ASSERT(m_lexicalEnvironment->record()->isDeclarativeEnvironmentRecord());
    ASSERT(m_lexicalEnvironment->record()->asDeclarativeEnvironmentRecord()->isFunctionEnvironmentRecord());
    return m_lexicalEnvironment->record()->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord()->functionObject();
}
#endif
}
