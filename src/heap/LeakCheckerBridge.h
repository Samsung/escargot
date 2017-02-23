#ifndef __EscargotLeakCheckerBridge__
#define __EscargotLeakCheckerBridge__

#ifdef PROFILE_BDWGC

#include "runtime/Value.h"

namespace Escargot {

class ExecutionState;
Value builtinRegisterLeakCheck(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression);
Value builtinDumpBackTrace(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression);
Value builtinSetGCPhaseName(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression);
}

#endif // PROFILE_BDWGC

#endif // __EscargotLeakCheckerBridge__
