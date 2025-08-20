/*
 * Copyright (c) 2025-present Samsung Electronics Co., Ltd
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
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
#include "DisposableObject.h"
#include "runtime/Context.h"
#include "runtime/ErrorObject.h"
#include "runtime/ExtendedNativeFunctionObject.h"
#include "runtime/PromiseObject.h"
#include "runtime/VMInstance.h"
#include "runtime/ExecutionPauser.h"

namespace Escargot {

static constexpr const char* alreadyDisposedErrorMessage = "DisposableStack is already disposed";
static constexpr const char* alreadyAsyncDisposedErrorMessage = "AsyncDisposableStack is already disposed";

static Value disposableWrapper(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Let O be the this value.
    const Value& O = thisValue;
    // Let promiseCapability be ! NewPromiseCapability(%Promise%).
    auto promiseCapability = PromiseObject::newPromiseCapability(state, state.context()->globalObject()->promise());
    // Let result be Completion(Call(method, O)).
    try {
        Object::call(state, state.resolveCallee()->asExtendedNativeFunctionObject()->internalSlot(0), O, 0, nullptr);
    } catch (const Value& v) {
        // IfAbruptRejectPromise(result, promiseCapability).
        Value rejectArgv = v;
        Object::call(state, promiseCapability.m_rejectFunction, Value(), 1, &rejectArgv);
        return promiseCapability.m_promise;
    }
    // Perform ? Call(promiseCapability.[[Resolve]], undefined, « undefined »).
    Object::call(state, promiseCapability.m_resolveFunction, Value(), 0, nullptr);
    // Return promiseCapability.[[Promise]].
    return promiseCapability.m_promise;
}

static Value getDisposableMethod(ExecutionState& state, const Value& V, bool isAsync)
{
    Value method;
    // If hint is async-dispose, then
    if (isAsync) {
        // Let method be ? GetMethod(V, @@asyncDispose).
        method = Object::getMethod(state, V, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().asyncDispose));
        // If method is undefined, then
        if (method.isUndefined()) {
            // Set method to ? GetMethod(V, @@dispose).
            method = Object::getMethod(state, V, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().dispose));
            // If method is not undefined, then
            if (!method.isUndefined()) {
                // Let closure be a new Abstract Closure with no parameters that captures method and performs the following steps when called:
                // NOTE: This function is not observable to user code. It is used to ensure that a Promise returned from a synchronous @@dispose method will not be awaited and that any exception thrown will not be thrown synchronously.
                // Return CreateBuiltinFunction(closure, 0, "", « »).
                ExtendedNativeFunctionObject* fn = new ExtendedNativeFunctionObjectImpl<1>(state, NativeFunctionInfo(AtomicString(), disposableWrapper, 0, NativeFunctionInfo::Strict));
                fn->setInternalSlot(0, method);
                return fn;
            }
        }
    } else {
        // Else
        // Let method be ? GetMethod(V, @@dispose).
        method = Object::getMethod(state, V, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().dispose));
    }
    return method;
}

DisposableResourceRecord::Record createDisposableResource(ExecutionState& state, Value V, bool isAsync, Optional<Value> method)
{
    // If method is not present, then
    if (!method) {
        // If V is either null or undefined, then
        if (V.isUndefinedOrNull()) {
            // Set V to undefined.
            V = Value();
            // Set method to undefined.
            method = Value();
        } else {
            // Else,
            // If V is not an Object, throw a TypeError exception.
            if (!V.isObject()) {
                ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "disposable value should be Object, undefined or null");
            }
            // Set method to ? GetDisposeMethod(V, hint).
            method = getDisposableMethod(state, V, isAsync);
            // If method is undefined, throw a TypeError exception.
            if (method.value().isUndefined()) {
                ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "disposable method should be callable");
            }
        }
    } else {
        // Else,
        // If IsCallable(method) is false, throw a TypeError exception.
        if (!method->isCallable()) {
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "disposable method should be callable");
        }
    }
    // Return the DisposableResource Record { [[ResourceValue]]: V, [[Hint]]: hint, [[DisposeMethod]]: method }.
    return DisposableResourceRecord::Record(V, method.value(), isAsync);
}

Value DisposableStackObject::use(ExecutionState& state, const Value& value)
{
    // Let disposableStack be the this value.
    // Perform ? RequireInternalSlot(disposableStack, [[DisposableState]]).
    // If disposableStack.[[DisposableState]] is disposed, throw a ReferenceError exception.
    if (m_isDisposed) {
        ErrorObject::throwBuiltinError(state, ErrorCode::ReferenceError, alreadyDisposedErrorMessage);
    }
    // Perform ? AddDisposableResource(disposableStack.[[DisposeCapability]], value, sync-dispose).
    m_record->m_records.pushBack(createDisposableResource(state, value, false, NullOption));
    // Return value.
    return value;
}

void DisposableStackObject::dispose(ExecutionState& state)
{
    // Let disposableStack be the this value.
    // Perform ? RequireInternalSlot(disposableStack, [[DisposableState]]).
    // If disposableStack.[[DisposableState]] is disposed, return undefined.
    if (m_isDisposed) {
        return;
    }
    // Set disposableStack.[[DisposableState]] to disposed.
    m_isDisposed = true;
    // Return ? DisposeResources(disposableStack.[[DisposeCapability]], NormalCompletion(undefined)).
    Value resultError = Value(Value::EmptyValue);
    // Let needsAwait be false.
    // Let hasAwaited be false.
    // For each element resource of disposeCapability.[[DisposableResourceStack]], in reverse list order, do
    while (m_record->m_records.size()) {
        DisposableResourceRecord::Record record = m_record->m_records.back();
        m_record->m_records.pop_back();

        // Let value be resource.[[ResourceValue]].
        Value value = record.m_resourceValue;
        // Let hint be resource.[[Hint]].
        // Let method be resource.[[DisposeMethod]].
        Value method = record.m_disposbleMethod;
        // If hint is sync-dispose and needsAwait is true and hasAwaited is false, then
        if (!record.m_isAsyncDisposableResource && m_record->m_needsAwait) {
            ASSERT_NOT_REACHED();
            // Perform ! Await(undefined).
            // Set needsAwait to false.
        }
        // If method is not undefined, then
        if (!method.isUndefined()) {
            // Let result be Completion(Call(method, value)).
            try {
                Object::call(state, method, value, 0, nullptr);
                // If result is a normal completion and hint is async-dispose, then
                if (record.m_isAsyncDisposableResource) {
                    ASSERT_NOT_REACHED();
                    // Set result to Completion(Await(result.[[Value]])).
                    // Set hasAwaited to true.
                }
            } catch (const Value& error) {
                // If result is a throw completion, then
                // If completion is a throw completion, then
                if (!resultError.isEmpty()) {
                    // Set result to result.[[Value]].
                    // Let suppressed be completion.[[Value]].
                    // Let error be a newly created SuppressedError object.
                    // Perform CreateNonEnumerableDataPropertyOrThrow(error, "error", result).
                    // Perform CreateNonEnumerableDataPropertyOrThrow(error, "suppressed", suppressed).
                    // Set completion to ThrowCompletion(error).
                    auto supressedError = new SuppressedErrorObject(state, state.context()->globalObject()->suppressedErrorPrototype(),
                                                                    new ASCIIStringFromExternalMemory("An error was suppressed during disposal"),
                                                                    true, true, error, resultError);
                    resultError = supressedError;
                } else {
                    // Else,
                    // Set completion to result.
                    resultError = error;
                }
            }
        } else if (record.m_isAsyncDisposableResource) {
            ASSERT_NOT_REACHED();
            // Else,
            // NOTE below assert could be wrong since method can be null
            // so I added condition to this statement "record.m_isAsyncDisposableResource"
            // Assert: hint is async-dispose.
            ASSERT(record.m_isAsyncDisposableResource);
            // Set needsAwait to true.
            // NOTE: This can only indicate a case where either null or undefined was the initialized value of an await using declaration.
        }
    }
    // If needsAwait is true and hasAwaited is false, then
    //   Perform ! Await(undefined).
    // NOTE: After disposeCapability has been disposed, it will never be used again. The contents of disposeCapability.[[DisposableResourceStack]] can be discarded in implementations, such as by garbage collection, at this point.
    // Set disposeCapability.[[DisposableResourceStack]] to a new empty List.
    // Return ? completion.
    if (!resultError.isEmpty()) {
        state.context()->throwException(state, resultError);
    }
}

static Value disposableStackObjectAdoptWrapper(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Return ? Call(onDispose, undefined, « value »).
    Value value = state.resolveCallee()->asExtendedNativeFunctionObject()->internalSlot(1);
    return Object::call(state, state.resolveCallee()->asExtendedNativeFunctionObject()->internalSlot(0), Value(), 1, &value);
}

Value DisposableStackObject::adopt(ExecutionState& state, const Value& value, const Value& onDispose)
{
    // Let disposableStack be the this value.
    // Perform ? RequireInternalSlot(disposableStack, [[DisposableState]]).
    // If disposableStack.[[DisposableState]] is disposed, throw a ReferenceError exception.
    if (m_isDisposed) {
        ErrorObject::throwBuiltinError(state, ErrorCode::ReferenceError, alreadyDisposedErrorMessage);
    }
    // If IsCallable(onDispose) is false, throw a TypeError exception.
    if (!onDispose.isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError,
                                       state.context()->staticStrings().DisposableStack.string(),
                                       true, state.context()->staticStrings().adopt.string(),
                                       ErrorObject::Messages::GlobalObject_FirstArgumentNotCallable);
    }
    // Let closure be a new Abstract Closure with no parameters that captures value and onDispose and performs the following steps when called:
    // Let F be CreateBuiltinFunction(closure, 0, "", « »).
    auto F = new ExtendedNativeFunctionObjectImpl<2>(state, NativeFunctionInfo(AtomicString(), disposableStackObjectAdoptWrapper, 0));
    F->setInternalSlot(0, onDispose);
    F->setInternalSlot(1, value);
    // Perform ? AddDisposableResource(disposableStack.[[DisposeCapability]], undefined, sync-dispose, F).
    m_record->m_records.pushBack(createDisposableResource(state, Value(), false, Value(F)));
    // Return value.
    return value;
}

void DisposableStackObject::defer(ExecutionState& state, const Value& onDispose)
{
    // Let disposableStack be the this value.
    // Perform ? RequireInternalSlot(disposableStack, [[DisposableState]]).
    // If disposableStack.[[DisposableState]] is disposed, throw a ReferenceError exception.
    if (m_isDisposed) {
        ErrorObject::throwBuiltinError(state, ErrorCode::ReferenceError, alreadyDisposedErrorMessage);
    }
    // If IsCallable(onDispose) is false, throw a TypeError exception.
    if (!onDispose.isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError,
                                       state.context()->staticStrings().DisposableStack.string(),
                                       true, state.context()->staticStrings().adopt.string(),
                                       ErrorObject::Messages::GlobalObject_FirstArgumentNotCallable);
    }
    // Perform ? AddDisposableResource(disposableStack.[[DisposeCapability]], undefined, sync-dispose, onDispose).
    m_record->m_records.pushBack(createDisposableResource(state, Value(), false, onDispose));
    // Return undefined.
}

DisposableStackObject* DisposableStackObject::move(ExecutionState& state)
{
    // Let disposableStack be the this value.
    // Perform ? RequireInternalSlot(disposableStack, [[DisposableState]]).
    // If disposableStack.[[DisposableState]] is disposed, throw a ReferenceError exception.
    if (m_isDisposed) {
        ErrorObject::throwBuiltinError(state, ErrorCode::ReferenceError, alreadyDisposedErrorMessage);
    }
    // Let newDisposableStack be ? OrdinaryCreateFromConstructor(%DisposableStack%, "%DisposableStack.prototype%", « [[DisposableState]], [[DisposeCapability]] »).
    // Set newDisposableStack.[[DisposableState]] to pending.
    // Set newDisposableStack.[[DisposeCapability]] to disposableStack.[[DisposeCapability]].
    // Set disposableStack.[[DisposeCapability]] to NewDisposeCapability().
    // Set disposableStack.[[DisposableState]] to disposed.
    DisposableStackObject* newDisposableStack = new DisposableStackObject(state, state.context()->globalObject()->disposableStackPrototype());
    auto s = newDisposableStack->m_record;
    newDisposableStack->m_record = m_record;
    m_record = s;
    m_isDisposed = true;
    // Return newDisposableStack.
    return newDisposableStack;
}

Value AsyncDisposableStackObject::use(ExecutionState& state, const Value& value)
{
    // Let asyncDisposableStack be the this value.
    // Perform ? RequireInternalSlot(asyncDisposableStack, [[AsyncDisposableState]]).
    // If asyncDisposableStack.[[AsyncDisposableState]] is disposed, throw a ReferenceError exception.
    if (m_isDisposed) {
        ErrorObject::throwBuiltinError(state, ErrorCode::ReferenceError, alreadyAsyncDisposedErrorMessage);
    }
    // Perform ? AddDisposableResource(asyncDisposableStack.[[DisposeCapability]], value, async-dispose).
    m_record->m_records.pushBack(createDisposableResource(state, value, true, NullOption));
    // Return value.
    return value;
}

static void asyncDisposeResources(ExecutionState& state, DisposableResourceRecord* data);

static Value awaitFulfilledFunction(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    auto s = state.resolveCallee()->asExtendedNativeFunctionObject();
    auto data = s->internalSlot(0).asPointerValue()->asDisposableResourceRecord();
    asyncDisposeResources(state, data);
    return Value();
}

static Value awaitRejectedFunction(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    auto s = state.resolveCallee()->asExtendedNativeFunctionObject();
    auto data = s->internalSlot(0).asPointerValue()->asDisposableResourceRecord();
    asyncDisposeResources(state, data);
    return Value();
}

static PromiseObject* asyncDisposeAwaitOperation(ExecutionState& state, const Value& awaitValue, DisposableResourceRecord* source, size_t stage)
{
    source->m_awaitResumeStage = stage;
    PromiseObject* promise = PromiseObject::promiseResolve(state, state.context()->globalObject()->promise(), awaitValue)->asPromiseObject();
    ExtendedNativeFunctionObject* onFulfilled = new ExtendedNativeFunctionObjectImpl<1>(state, NativeFunctionInfo(AtomicString(), awaitFulfilledFunction, 1));
    onFulfilled->setInternalSlot(0, source);
    ExtendedNativeFunctionObject* onRejected = new ExtendedNativeFunctionObjectImpl<1>(state, NativeFunctionInfo(AtomicString(), awaitRejectedFunction, 1));
    onRejected->setInternalSlot(0, source);
    promise->then(state, onFulfilled, onRejected, Optional<PromiseReaction::Capability>());
    return promise;
}

static void asyncDisposeResources(ExecutionState& state, DisposableResourceRecord* data)
{
    Value result;
    bool resultIsError = false;
    while (data->m_records.size()) {
        DisposableResourceRecord::Record record = data->m_records.back();
        data->m_records.pop_back();
        // Let value be resource.[[ResourceValue]].
        Value value = record.m_resourceValue;
        // Let hint be resource.[[Hint]].
        // Let method be resource.[[DisposeMethod]].
        Value method = record.m_disposbleMethod;

        if (data->m_awaitResumeStage == 1) {
            data->m_awaitResumeStage = 0;
            goto AsyncDisposableAwaitResumeStage1;
        } else if (data->m_awaitResumeStage == 2) {
            data->m_awaitResumeStage = 0;
            goto AsyncDisposableAwaitResumeStage2;
        } else if (data->m_awaitResumeStage == 3) {
            data->m_awaitResumeStage = 0;
            goto AsyncDisposableAwaitResumeStage3;
        }

        // If hint is sync-dispose and needsAwait is true and hasAwaited is false, then
        if (!record.m_isAsyncDisposableResource && data->m_needsAwait) {
            // Perform ! Await(undefined).
            data->m_records.pushBack(record);
            asyncDisposeAwaitOperation(state, Value(), data, 1);
            return;
        AsyncDisposableAwaitResumeStage1:
            // Set needsAwait to false.
            data->m_needsAwait = false;
        }
        // If method is not undefined, thenS
        if (!method.isUndefined()) {
            // Let result be Completion(Call(method, value)).
            try {
                resultIsError = false;
                result = Object::call(state, method, value, 0, nullptr);
            } catch (const Value& error) {
                result = error;
                resultIsError = true;
            }

            // If result is a normal completion and hint is async-dispose, then
            if (!resultIsError && record.m_isAsyncDisposableResource) {
                // Set result to Completion(Await(result.[[Value]])).
                data->m_records.pushBack(record);
                asyncDisposeAwaitOperation(state, data->m_result, data, 2);
                return;
                // Set hasAwaited to true.
            AsyncDisposableAwaitResumeStage2:
                result = data->m_awaitResumeValueSlot;
                resultIsError = data->m_awaitResumeStateSlot == Value(ExecutionPauser::ResumeState::Throw);
                data->m_hasAwaited = true;
            }

            // If result is a throw completion, then
            if (resultIsError) {
                // If completion is a throw completion, then
                if (data->m_resultIsError) {
                    // Set result to result.[[Value]].
                    // Let suppressed be completion.[[Value]].
                    // Let error be a newly created SuppressedError object.
                    // Perform CreateNonEnumerableDataPropertyOrThrow(error, "error", result).
                    // Perform CreateNonEnumerableDataPropertyOrThrow(error, "suppressed", suppressed).
                    auto supressedError = new SuppressedErrorObject(state, state.context()->globalObject()->suppressedErrorPrototype(),
                                                                    new ASCIIStringFromExternalMemory("An error was suppressed during disposal"),
                                                                    true, true, result, data->m_result);
                    // Set completion to ThrowCompletion(error).
                    data->m_result = supressedError;
                } else {
                    // Else,
                    // Set completion to result.
                    data->m_resultIsError = true;
                    data->m_result = result;
                }
            }
        } else if (record.m_isAsyncDisposableResource) {
            // Else,
            // NOTE below assert could be wrong since method can be null
            // so I added condition to this statement "record.m_isAsyncDisposableResource"
            // Assert: hint is async-dispose.
            ASSERT(record.m_isAsyncDisposableResource);
            // Set needsAwait to true.
            data->m_needsAwait = true;
            // NOTE: This can only indicate a case where either null or undefined was the initialized value of an await using declaration.
        }
    }
    // If needsAwait is true and hasAwaited is false, then
    if (data->m_needsAwait && !data->m_hasAwaited) {
        data->m_needsAwait = false;
        // Perform ! Await(undefined).
        asyncDisposeAwaitOperation(state, Value(), data, 3);
        return;
    }

    // NOTE: After disposeCapability has been disposed, it will never be used again. The contents of disposeCapability.[[DisposableResourceStack]] can be discarded in implementations, such as by garbage collection, at this point.
    // Set disposeCapability.[[DisposableResourceStack]] to a new empty List.
    // Return ? completion.
AsyncDisposableAwaitResumeStage3:
    if (data->m_resultIsError) {
        Object::call(state, data->m_promiseCapability->m_rejectFunction, Value(), 1, &data->m_result);
    } else {
        Object::call(state, data->m_promiseCapability->m_resolveFunction, Value(), 1, &data->m_result);
    }
}

PromiseObject* AsyncDisposableStackObject::asyncDispose(ExecutionState& state)
{
    // Let asyncDisposableStack be the this value.
    // Let promiseCapability be ! NewPromiseCapability(%Promise%).
    // If asyncDisposableStack does not have an [[AsyncDisposableState]] internal slot, then
    //   Perform ! Call(promiseCapability.[[Reject]], undefined, « a newly created TypeError object »).
    //   Return promiseCapability.[[Promise]].
    auto promiseCapability = PromiseObject::newPromiseCapability(state, state.context()->globalObject()->promise());
    // If asyncDisposableStack.[[AsyncDisposableState]] is disposed, then
    if (m_isDisposed) {
        // Perform ! Call(promiseCapability.[[Resolve]], undefined, « undefined »).
        Value argv;
        Object::call(state, promiseCapability.m_resolveFunction, Value(), 1, &argv);
        // Return promiseCapability.[[Promise]].
        return promiseCapability.m_promise->asPromiseObject();
    }
    // Set asyncDisposableStack.[[AsyncDisposableState]] to disposed.
    m_isDisposed = true;
    // Let result be Completion(DisposeResources(asyncDisposableStack.[[DisposeCapability]], NormalCompletion(undefined))).
    // IfAbruptRejectPromise(result, promiseCapability).
    // Perform ! Call(promiseCapability.[[Resolve]], undefined, « result »).
    m_record->m_promiseCapability = promiseCapability;
    asyncDisposeResources(state, m_record);
    // Return promiseCapability.[[Promise]].
    return promiseCapability.m_promise->asPromiseObject();
}

Value AsyncDisposableStackObject::adopt(ExecutionState& state, const Value& value, const Value& onDisposeAsync)
{
    // Let asyncDisposableStack be the this value.
    // Perform ? RequireInternalSlot(asyncDisposableStack, [[AsyncDisposableState]]).
    // If asyncDisposableStack.[[AsyncDisposableState]] is disposed, throw a ReferenceError exception.
    if (m_isDisposed) {
        ErrorObject::throwBuiltinError(state, ErrorCode::ReferenceError, alreadyAsyncDisposedErrorMessage);
    }
    // If IsCallable(onDisposeAsync) is false, throw a TypeError exception.
    if (!onDisposeAsync.isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError,
                                       state.context()->staticStrings().AsyncDisposableStack.string(),
                                       true, state.context()->staticStrings().adopt.string(),
                                       ErrorObject::Messages::GlobalObject_FirstArgumentNotCallable);
    }

    // Let closure be a new Abstract Closure with no parameters that captures value and onDisposeAsync and performs the following steps when called:
    // Let F be CreateBuiltinFunction(closure, 0, "", « »).
    // NOTE this closure is same with sync version
    auto F = new ExtendedNativeFunctionObjectImpl<2>(state, NativeFunctionInfo(AtomicString(), disposableStackObjectAdoptWrapper, 0));
    F->setInternalSlot(0, onDisposeAsync);
    F->setInternalSlot(1, value);
    // Perform ? AddDisposableResource(asyncDisposableStack.[[DisposeCapability]], undefined, async-dispose, F).
    m_record->m_records.pushBack(createDisposableResource(state, Value(), true, Value(F)));
    // Return value.
    return value;
}

void AsyncDisposableStackObject::defer(ExecutionState& state, const Value& onDisposeAsync)
{
    // Let asyncDisposableStack be the this value.
    // Perform ? RequireInternalSlot(asyncDisposableStack, [[AsyncDisposableState]]).
    // If asyncDisposableStack.[[AsyncDisposableState]] is disposed, throw a ReferenceError exception.
    if (m_isDisposed) {
        ErrorObject::throwBuiltinError(state, ErrorCode::ReferenceError, alreadyAsyncDisposedErrorMessage);
    }
    // If IsCallable(onDisposeAsync) is false, throw a TypeError exception.
    if (!onDisposeAsync.isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError,
                                       state.context()->staticStrings().AsyncDisposableStack.string(),
                                       true, state.context()->staticStrings().adopt.string(),
                                       ErrorObject::Messages::GlobalObject_FirstArgumentNotCallable);
    }
    // Perform ? AddDisposableResource(asyncDisposableStack.[[DisposeCapability]], undefined, async-dispose, onDisposeAsync).
    m_record->m_records.pushBack(createDisposableResource(state, Value(), true, onDisposeAsync));
    // Return undefined.
}

AsyncDisposableStackObject* AsyncDisposableStackObject::move(ExecutionState& state)
{
    // Let asyncDisposableStack be the this value.
    // Perform ? RequireInternalSlot(asyncDisposableStack, [[AsyncDisposableState]]).
    // If asyncDisposableStack.[[AsyncDisposableState]] is disposed, throw a ReferenceError exception.
    if (m_isDisposed) {
        ErrorObject::throwBuiltinError(state, ErrorCode::ReferenceError, alreadyAsyncDisposedErrorMessage);
    }
    // Let newAsyncDisposableStack be ? OrdinaryCreateFromConstructor(%AsyncDisposableStack%, "%AsyncDisposableStack.prototype%", « [[AsyncDisposableState]], [[DisposeCapability]] »).
    AsyncDisposableStackObject* newAsyncDisposableStack = new AsyncDisposableStackObject(state, state.context()->globalObject()->asyncDisposableStackPrototype());
    // Set newAsyncDisposableStack.[[AsyncDisposableState]] to pending.
    // Set newAsyncDisposableStack.[[DisposeCapability]] to asyncDisposableStack.[[DisposeCapability]].
    // Set asyncDisposableStack.[[DisposeCapability]] to NewDisposeCapability().
    // Set asyncDisposableStack.[[AsyncDisposableState]] to disposed.
    // Return newAsyncDisposableStack.
    auto s = newAsyncDisposableStack->m_record;
    newAsyncDisposableStack->m_record = m_record;
    m_record = s;
    m_isDisposed = true;
    return newAsyncDisposableStack;
}
} // namespace Escargot
