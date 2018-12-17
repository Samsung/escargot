/*
 * Copyright (c) 2017-present Samsung Electronics Co., Ltd
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 */

#include "Escargot.h"
#include "runtime/Context.h"
#include "runtime/GlobalObject.h"
#include "runtime/JobQueue.h"

#ifdef ESCARGOT_ENABLE_VENDORTEST

namespace Escargot {

#ifdef ESCARGOT_ENABLE_PROMISE
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
#endif // ESCARGOT_ENABLE_PROMISE

static Value builtinCreateNewGlobalObject(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    Context* ctx = new Context(state.context()->vmInstance());
    return ctx->globalObject();
}

void installTestFunctions(ExecutionState& state)
{
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
}

}

#endif // ESCARGOT_ENABLE_VENDORTEST
