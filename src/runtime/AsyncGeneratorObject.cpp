/*
 * Copyright (c) 2020-present Samsung Electronics Co., Ltd
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more detaials.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 */

#include "Escargot.h"
#include "AsyncGeneratorObject.h"
#include "interpreter/ByteCodeInterpreter.h"
#include "runtime/Context.h"
#include "runtime/PromiseObject.h"
#include "runtime/ErrorObject.h"
#include "runtime/IteratorObject.h"
#include "runtime/ExtendedNativeFunctionObject.h"
#include "runtime/ScriptAsyncGeneratorFunctionObject.h"

namespace Escargot {

AsyncGeneratorObject::AsyncGeneratorObject(ExecutionState& state, Object* proto, ExecutionState* executionState, Value* registerFile, ByteCodeBlock* blk)
    : Object(state, proto)
    , m_asyncGeneratorState(SuspendedStart)
    , m_executionPauser(state, this, executionState, registerFile, blk)
{
}

void* AsyncGeneratorObject::operator new(size_t size)
{
    ASSERT(size == sizeof(AsyncGeneratorObject));

    static MAY_THREAD_LOCAL bool typeInited = false;
    static MAY_THREAD_LOCAL GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(AsyncGeneratorObject)] = { 0 };
        fillGCDescriptor(obj_bitmap);
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(AsyncGeneratorObject));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

// https://www.ecma-international.org/ecma-262/10.0/index.html#async-generator-resume-next-return-processor-fulfilled
Value asyncGeneratorResumeNextReturnProcessorFulfilledFunction(ExecutionState& state, ExtendedNativeFunctionObject* F, const Value& value)
{
    // Let F be the active function object.
    // Set F.[[Generator]].[[AsyncGeneratorState]] to "completed".
    AsyncGeneratorObject* generator = Value(F->internalSlot(AsyncGeneratorObject::BuiltinFunctionSlot::Generator)).asPointerValue()->asAsyncGeneratorObject();
    generator->m_asyncGeneratorState = AsyncGeneratorObject::AsyncGeneratorState::Completed;
    // Return ! AsyncGeneratorResolve(F.[[Generator]], value, true).
    return AsyncGeneratorObject::asyncGeneratorResolve(state, generator, value, true);
}

static Value asyncGeneratorResumeNextReturnProcessorFulfilledFunction(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    ExtendedNativeFunctionObject* self = state.resolveCallee()->asExtendedNativeFunctionObject();
    return asyncGeneratorResumeNextReturnProcessorFulfilledFunction(state, self, argv[0]);
}

// https://www.ecma-international.org/ecma-262/10.0/index.html#async-generator-resume-next-return-processor-rejected
Value asyncGeneratorResumeNextReturnProcessorRejectedFunction(ExecutionState& state, ExtendedNativeFunctionObject* F, const Value& reason)
{
    // Let F be the active function object.
    // Set F.[[Generator]].[[AsyncGeneratorState]] to "completed".
    AsyncGeneratorObject* generator = Value(F->internalSlot(AsyncGeneratorObject::BuiltinFunctionSlot::Generator)).asPointerValue()->asAsyncGeneratorObject();
    generator->m_asyncGeneratorState = AsyncGeneratorObject::AsyncGeneratorState::Completed;
    // Return ! AsyncGeneratorReject(F.[[Generator]], reason).
    return AsyncGeneratorObject::asyncGeneratorReject(state, generator, reason);
}

static Value asyncGeneratorResumeNextReturnProcessorRejectedFunction(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    ExtendedNativeFunctionObject* self = state.resolveCallee()->asExtendedNativeFunctionObject();
    return asyncGeneratorResumeNextReturnProcessorRejectedFunction(state, self, argv[0]);
}

// https://www.ecma-international.org/ecma-262/10.0/index.html#sec-asyncgeneratorenqueue
Value AsyncGeneratorObject::asyncGeneratorEnqueue(ExecutionState& state, const Value& generator, AsyncGeneratorObject::AsyncGeneratorEnqueueType type, const Value& value)
{
    // Let promiseCapability be ! NewPromiseCapability(%Promise%).
    PromiseReaction::Capability promiseCapability = PromiseObject::newPromiseCapability(state, state.context()->globalObject()->promise());
    // If Type(generator) is not Object, or if generator does not have an [[AsyncGeneratorState]] internal slot, then
    if (!generator.isObject() || !generator.asObject()->isAsyncGeneratorObject()) {
        // Let badGeneratorError be a newly created TypeError object.
        ErrorObject* badGeneratorError = ErrorObject::createError(state, ErrorObject::TypeError, String::fromASCII("This value is not Async Generator Object."));
        // Perform ! Call(promiseCapability.[[Reject]], undefined, « badGeneratorError »).
        Value argv(badGeneratorError);
        Object::call(state, promiseCapability.m_rejectFunction, Value(), 1, &argv);
        // Return promiseCapability.[[Promise]].
        return promiseCapability.m_promise;
    }

    AsyncGeneratorObject* generatorObject = generator.asObject()->asAsyncGeneratorObject();
    // Let queue be generator.[[AsyncGeneratorQueue]].
    auto& queue = generatorObject->m_asyncGeneratorQueue;

    // Let request be AsyncGeneratorRequest { [[Completion]]: completion, [[Capability]]: promiseCapability }.
    // Append request to the end of queue.
    queue.pushBack(AsyncGeneratorObject::AsyncGeneratorQueueData(promiseCapability, type, value));

    // Let state be generator.[[AsyncGeneratorState]].
    // If state is not "executing", then
    if (generatorObject->asyncGeneratorState() != AsyncGeneratorObject::Executing) {
        // Perform ! AsyncGeneratorResumeNext(generator).
        AsyncGeneratorObject::asyncGeneratorResumeNext(state, generatorObject);
    }
    // Return promiseCapability.[[Promise]].
    return promiseCapability.m_promise;
}

// https://www.ecma-international.org/ecma-262/10.0/index.html#sec-asyncgeneratorresumenext
Value AsyncGeneratorObject::asyncGeneratorResumeNext(ExecutionState& state, AsyncGeneratorObject* generator)
{
    // Assert: generator is an AsyncGenerator instance.
    // Let state be generator.[[AsyncGeneratorState]].
    auto generatorState = generator->asyncGeneratorState();
    // Assert: state is not "executing".
    ASSERT(generatorState != AsyncGeneratorObject::Executing);
    // If state is "awaiting-return", return undefined.
    if (generatorState == AsyncGeneratorObject::AwaitingReturn) {
        return Value();
    }
    // Let queue be generator.[[AsyncGeneratorQueue]].
    auto& queue = generator->m_asyncGeneratorQueue;
    // If queue is an empty List, return undefined.
    if (queue.size() == 0) {
        return Value();
    }
    // Let next be the value of the first element of queue.
    const auto& next = generator->m_asyncGeneratorQueue[0];
    // Assert: next is an AsyncGeneratorRequest record.
    // Let completion be next.[[Completion]].

    // If completion is an abrupt completion, then
    if (next.m_operationType != AsyncGeneratorObject::AsyncGeneratorEnqueueType::Next) {
        // If state is "suspendedStart", then
        if (generatorState == AsyncGeneratorObject::SuspendedStart) {
            // Set generator.[[AsyncGeneratorState]] to "completed".
            generator->m_asyncGeneratorState = AsyncGeneratorObject::Completed;
            // Set state to "completed".
            generatorState = AsyncGeneratorObject::Completed;
        }
        // If state is "completed", then
        if (generatorState == AsyncGeneratorObject::Completed) {
            // If completion.[[Type]] is return, then
            if (next.m_operationType == AsyncGeneratorObject::AsyncGeneratorEnqueueType::Return) {
                // Set generator.[[AsyncGeneratorState]] to "awaiting-return".
                generator->m_asyncGeneratorState = AsyncGeneratorObject::AwaitingReturn;
                // Let promise be ? PromiseResolve(%Promise%, « completion.[[Value]] »).
                auto promise = PromiseObject::promiseResolve(state, state.context()->globalObject()->promise(), next.m_value)->asPromiseObject();
                // Let stepsFulfilled be the algorithm steps defined in AsyncGeneratorResumeNext Return Processor Fulfilled Functions.
                // Let onFulfilled be CreateBuiltinFunction(stepsFulfilled, « [[Generator]] »).
                // Set onFulfilled.[[Generator]] to generator.
                ExtendedNativeFunctionObject* onFulfilled = new ExtendedNativeFunctionObjectImpl<1>(state, NativeFunctionInfo(AtomicString(), asyncGeneratorResumeNextReturnProcessorFulfilledFunction, 1));
                onFulfilled->setInternalSlot(AsyncGeneratorObject::BuiltinFunctionSlot::Generator, generator);
                // Let stepsRejected be the algorithm steps defined in AsyncGeneratorResumeNext Return Processor Rejected Functions.
                // Let onRejected be CreateBuiltinFunction(stepsRejected, « [[Generator]] »).
                // Set onRejected.[[Generator]] to generator.
                ExtendedNativeFunctionObject* onRejected = new ExtendedNativeFunctionObjectImpl<1>(state, NativeFunctionInfo(AtomicString(), asyncGeneratorResumeNextReturnProcessorRejectedFunction, 1));
                onRejected->setInternalSlot(AsyncGeneratorObject::BuiltinFunctionSlot::Generator, generator);
                // Perform ! PerformPromiseThen(promise, onFulfilled, onRejected).
                promise->then(state, onFulfilled, onRejected);
                // Return undefined.
                return Value();
            } else {
                // Assert: completion.[[Type]] is throw.
                // Perform ! AsyncGeneratorReject(generator, completion.[[Value]]).
                AsyncGeneratorObject::asyncGeneratorReject(state, generator, next.m_value);
                // Return undefined.
                return Value();
            }
        }
    } else if (generatorState == AsyncGeneratorObject::Completed) {
        // Else if state is "completed", return ! AsyncGeneratorResolve(generator, undefined, true).
        return AsyncGeneratorObject::asyncGeneratorResolve(state, generator, Value(), true);
    }

    // Assert: state is either "suspendedStart" or "suspendedYield".
    ASSERT(generatorState == AsyncGeneratorObject::SuspendedStart || generatorState == AsyncGeneratorObject::SuspendedYield);
    // Let genContext be generator.[[AsyncGeneratorContext]].
    // Let callerContext be the running execution context.
    // Suspend callerContext.
    // Set generator.[[AsyncGeneratorState]] to "executing".
    generator->m_asyncGeneratorState = AsyncGeneratorObject::Executing;
    // Push genContext onto the execution context stack; genContext is now the running execution context.
    // Resume the suspended evaluation of genContext using completion as the result of the operation that suspended it. Let result be the completion record returned by the resumed computation.
    bool isAbruptReturn = next.m_operationType == AsyncGeneratorObject::AsyncGeneratorEnqueueType::Return;
    bool isAbruptThrow = next.m_operationType == AsyncGeneratorObject::AsyncGeneratorEnqueueType::Throw;
    ExecutionPauser::start(state, &generator->m_executionPauser, generator, next.m_value, isAbruptReturn, isAbruptThrow, ExecutionPauser::StartFrom::AsyncGenerator);
    // Assert: result is never an abrupt completion.
    // Assert: When we return here, genContext has already been removed from the execution context stack and callerContext is the currently running execution context.
    // Return undefined.
    return Value();
}

// https://www.ecma-international.org/ecma-262/10.0/index.html#sec-asyncgeneratorresolve
Value AsyncGeneratorObject::asyncGeneratorResolve(ExecutionState& state, AsyncGeneratorObject* generator, const Value& value, bool done)
{
    // Assert: generator is an AsyncGenerator instance.
    // Let queue be generator.[[AsyncGeneratorQueue]].
    auto& queue = generator->m_asyncGeneratorQueue;
    // Assert: queue is not an empty List.
    ASSERT(queue.size());
    // Remove the first element from queue and let next be the value of that element.
    auto next = queue[0];
    queue.erase(0);
    // Let promiseCapability be next.[[Capability]].
    auto promiseCapability = next.m_capability;
    // Let iteratorResult be ! CreateIterResultObject(value, done).
    Value iteratorResult = IteratorObject::createIterResultObject(state, value, done);
    // Perform ! Call(promiseCapability.[[Resolve]], undefined, « iteratorResult »).
    Object::call(state, promiseCapability.m_resolveFunction, Value(), 1, &iteratorResult);
    // Perform ! AsyncGeneratorResumeNext(generator).
    AsyncGeneratorObject::asyncGeneratorResumeNext(state, generator);
    // Return undefined.
    return Value();
}

// https://www.ecma-international.org/ecma-262/10.0/index.html#sec-asyncgeneratorreject
Value AsyncGeneratorObject::asyncGeneratorReject(ExecutionState& state, AsyncGeneratorObject* generator, Value exception)
{
    // Assert: generator is an AsyncGenerator instance.
    // Let queue be generator.[[AsyncGeneratorQueue]].
    auto& queue = generator->m_asyncGeneratorQueue;
    // Assert: queue is not an empty List.
    ASSERT(queue.size());
    // Remove the first element from queue and let next be the value of that element.
    auto next = queue[0];
    queue.erase(0);
    // Let promiseCapability be next.[[Capability]].
    auto promiseCapability = next.m_capability;
    // Perform ! Call(promiseCapability.[[Reject]], undefined, « exception »).
    Object::call(state, promiseCapability.m_rejectFunction, Value(), 1, &exception);
    // Perform ! AsyncGeneratorResumeNext(generator).
    AsyncGeneratorObject::asyncGeneratorResumeNext(state, generator);
    // Return undefined.
    return Value();
}
} // namespace Escargot
