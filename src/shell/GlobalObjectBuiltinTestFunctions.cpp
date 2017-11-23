/*
 * Copyright (c) 2017-present Samsung Electronics Co., Ltd
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
#include "runtime/Context.h"
#include "runtime/GlobalObject.h"
#include "runtime/JobQueue.h"

namespace Escargot {

#ifdef ESCARGOT_ENABLE_PROMISE
#ifdef ESCARGOT_ENABLE_VENDORTEST

static Value builtinDrainJobQueue(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    DefaultJobQueue* jobQueue = DefaultJobQueue::get(state.context()->jobQueue());
    while (jobQueue->hasNextJob()) {
        auto jobResult = jobQueue->nextJob()->run();
        if (!jobResult.error.isEmpty())
            return Value(false);
    }
    return Value(true);
}

static Value builtinAddPromiseReactions(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    PromiseObject* promise = argv[0].toObject(state)->asPromiseObject();
    PromiseReaction::Capability capability = PromiseObject::newPromiseCapability(state, promise->get(state, state.context()->staticStrings().constructor).value(state, promise).toObject(state));
    promise->appendReaction(argv[1].toObject(state)->asFunctionObject(), argv[2].toObject(state)->asFunctionObject(), capability);
    return Value();
}
#endif
#endif

#ifdef ESCARGOT_ENABLE_VENDORTEST
static Value builtinCreateNewGlobalObject(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    Context* ctx = new Context(state.context()->vmInstance());
    return ctx->globalObject();
}
#endif


void installTestFunctions(ExecutionState& state)
{
#ifdef ESCARGOT_ENABLE_VENDORTEST
    GlobalObject* globalObject = state.context()->globalObject();

#ifdef ESCARGOT_ENABLE_PROMISE
    AtomicString drainJobQueue(state, "drainJobQueue");
    globalObject->defineOwnProperty(state, ObjectPropertyName(drainJobQueue),
                                    ObjectPropertyDescriptor(new FunctionObject(state,
                                                                                NativeFunctionInfo(drainJobQueue, builtinDrainJobQueue, 0, nullptr, NativeFunctionInfo::Strict)),
                                                             (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::AllPresent)));
    AtomicString addPromiseReactions(state, "addPromiseReactions");
    globalObject->defineOwnProperty(state, ObjectPropertyName(addPromiseReactions),
                                    ObjectPropertyDescriptor(new FunctionObject(state,
                                                                                NativeFunctionInfo(addPromiseReactions, builtinAddPromiseReactions, 3, nullptr, NativeFunctionInfo::Strict)),
                                                             (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::AllPresent)));
#endif // ESCARGOT_ENABLE_PROMISE

    AtomicString createNewGlobalObject(state, "createNewGlobalObject");
    globalObject->defineOwnProperty(state, ObjectPropertyName(createNewGlobalObject),
                                    ObjectPropertyDescriptor(new FunctionObject(state,
                                                                                NativeFunctionInfo(createNewGlobalObject, builtinCreateNewGlobalObject, 0, nullptr, NativeFunctionInfo::Strict)),
                                                             (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::AllPresent)));
#else // ESCARGOT_ENABLE_VENDORTEST
/* Do nothong */
#endif // ESCARGOT_ENABLE_VENDORTEST
}
}
