/*
 * Copyright (c) 2017 Samsung Electronics Co., Ltd
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

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
