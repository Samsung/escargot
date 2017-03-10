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

#include "Escargot.h"
#include "LeakChecker.h"

#include "LeakCheckerBridge.h"
#include "CustomAllocator.h"
#include "runtime/ErrorObject.h"

#include <sys/time.h>
#include <sys/resource.h>

#ifdef PROFILE_BDWGC

namespace Escargot {

/* Usage in JS: registerLeakCheck( object: PointerValue, description: String ); */
Value builtinRegisterLeakCheck(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    if (!argv[0].isPointerValue())
        state.throwException(new ErrorObject(state, new ASCIIString("builtinRegisterLeakCheck should get pointer-type argument")));

    PointerValue* ptr = argv[0].asPointerValue();
    std::string description = argv[1].toString(state)->toUTF8StringData().data();
    RELEASE_ASSERT(ptr);

    GCUtil::GCLeakChecker::registerAddress(ptr, description);

    return Value();
}

/* Usage in JS: dumpBackTrace( phase: String ); */
Value builtinDumpBackTrace(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    GCUtil::GCLeakChecker::dumpBackTrace(argv[0].toString(state)->toUTF8StringData().data());
    return Value();
}

/* Usage in JS: setPhaseName( phase: String ); */
Value builtinSetGCPhaseName(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    GCUtil::GCLeakChecker::setGCPhaseName(argv[0].toString(state)->toUTF8StringData().data());
    return Value();
}
}

#endif // PROFILE_BDWGC
