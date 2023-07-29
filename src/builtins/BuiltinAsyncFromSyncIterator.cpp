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
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 */

#include "Escargot.h"
#include "runtime/GlobalObject.h"
#include "runtime/Context.h"
#include "runtime/VMInstance.h"
#include "runtime/AsyncFromSyncIteratorObject.h"
#include "runtime/PromiseObject.h"
#include "runtime/ErrorObject.h"
#include "runtime/IteratorObject.h"
#include "runtime/NativeFunctionObject.h"

namespace Escargot {

class ScriptAsyncFromSyncIteratorHelperFunctionObject : public NativeFunctionObject {
public:
    ScriptAsyncFromSyncIteratorHelperFunctionObject(ExecutionState& state, NativeFunctionInfo info, bool done)
        : NativeFunctionObject(state, info)
        , m_done(done)
    {
    }

    bool m_done;
};


// https://www.ecma-international.org/ecma-262/10.0/#sec-async-from-sync-iterator-value-unwrap-functions
static Value asyncFromSyncIteratorValueUnwrapLogic(ExecutionState& state, ScriptAsyncFromSyncIteratorHelperFunctionObject* activeFunctionObject, const Value& value)
{
    // Let F be the active function object.
    ScriptAsyncFromSyncIteratorHelperFunctionObject* F = activeFunctionObject;
    // Return ! CreateIterResultObject(value, F.[[Done]]).
    return IteratorObject::createIterResultObject(state, value, F->m_done);
}

static Value asyncFromSyncIteratorValueUnwrap(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    return asyncFromSyncIteratorValueUnwrapLogic(state, (ScriptAsyncFromSyncIteratorHelperFunctionObject*)state.resolveCallee(), argv[0]);
}

// https://www.ecma-international.org/ecma-262/10.0/#sec-asyncfromsynciteratorcontinuation
static Value asyncFromSyncIteratorContinuation(ExecutionState& state, Object* result, PromiseReaction::Capability promiseCapability)
{
    // Let done be IteratorComplete(result).
    bool done = IteratorObject::iteratorComplete(state, result);
    if (UNLIKELY(state.hasPendingException())) {
        // IfAbruptRejectPromise(done, promiseCapability).
        Value argv = state.detachPendingException();
        Object::call(state, promiseCapability.m_rejectFunction, Value(), 1, &argv);
        RETURN_VALUE_IF_PENDING_EXCEPTION
        return promiseCapability.m_promise;
    }

    // Let value be IteratorValue(result).
    Value value = IteratorObject::iteratorValue(state, result);
    if (UNLIKELY(state.hasPendingException())) {
        // IfAbruptRejectPromise(value, promiseCapability).
        Value argv = state.detachPendingException();
        Object::call(state, promiseCapability.m_rejectFunction, Value(), 1, &argv);
        RETURN_VALUE_IF_PENDING_EXCEPTION
        return promiseCapability.m_promise;
    }

    // Let valueWrapper be ? PromiseResolve(%Promise%, « value »).
    PromiseObject* valueWrapper = PromiseObject::promiseResolve(state, state.context()->globalObject()->promise(), value)->asPromiseObject();
    if (UNLIKELY(state.hasPendingException())) {
        // * added step from 2020 (esid: language/statements/for-await-of/async-from-sync-iterator-continuation-abrupt-completion-get-constructor.js)
        // IfAbruptRejectPromise(valueWrapper, promiseCapability).
        Value argv = state.detachPendingException();
        Object::call(state, promiseCapability.m_rejectFunction, Value(), 1, &argv);
        RETURN_VALUE_IF_PENDING_EXCEPTION
        return promiseCapability.m_promise;
    }

    // Let steps be the algorithm steps defined in Async-from-Sync Iterator Value Unwrap Functions.
    // Let onFulfilled be CreateBuiltinFunction(steps, « [[Done]] »).
    // Set onFulfilled.[[Done]] to done.
    auto onFulfilled = new ScriptAsyncFromSyncIteratorHelperFunctionObject(state, NativeFunctionInfo(AtomicString(), asyncFromSyncIteratorValueUnwrap, 1), done);
    // Perform ! PerformPromiseThen(valueWrapper, onFulfilled, undefined, promiseCapability).
    valueWrapper->then(state, onFulfilled, Value(), promiseCapability);
    // Return promiseCapability.[[Promise]].
    return promiseCapability.m_promise;
}

static Value builtinAsyncFromSyncIteratorNext(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // https://tc39.es/ecma262/#sec-%asyncfromsynciteratorprototype%.next
    // Let O be the this value.
    Value& O = thisValue;
    // Let promiseCapability be ! NewPromiseCapability(%Promise%).
    auto promiseCapability = PromiseObject::newPromiseCapability(state, state.context()->globalObject()->promise());
    ASSERT(!state.hasPendingException());
    // If Type(O) is not Object, or if O does not have a [[SyncIteratorRecord]] internal slot, then
    if (!O.isObject() || !O.asObject()->isAsyncFromSyncIteratorObject()) {
        // Let invalidIteratorError be a newly created TypeError object.
        Value invalidIteratorError = ErrorObject::createError(state, ErrorCode::TypeError, String::fromASCII("given this value is not Async-from-Sync Iterator"));
        // Perform ! Call(promiseCapability.[[Reject]], undefined, « invalidIteratorError »).
        Object::call(state, promiseCapability.m_rejectFunction, Value(), 1, &invalidIteratorError);
        RETURN_VALUE_IF_PENDING_EXCEPTION
        // Return promiseCapability.[[Promise]].
        return promiseCapability.m_promise;
    }
    // Let syncIteratorRecord be O.[[SyncIteratorRecord]].
    auto syncIteratorRecord = O.asObject()->asAsyncFromSyncIteratorObject()->syncIteratorRecord();
    Object* result;

    // Let result be IteratorNext(syncIteratorRecord, value).
    // If value is present, then
    //   a. Let result be IteratorNext(syncIteratorRecord, value).
    // Else,
    //   a. Let result be IteratorNext(syncIteratorRecord).
    result = IteratorObject::iteratorNext(state, syncIteratorRecord, argc ? argv[0] : Value(Value::EmptyValue));
    RETURN_VALUE_IF_PENDING_EXCEPTION
    if (UNLIKELY(state.hasPendingException())) {
        // IfAbruptRejectPromise(result, promiseCapability).
        Value argv = state.detachPendingException();
        Object::call(state, promiseCapability.m_rejectFunction, Value(), 1, &argv);
        RETURN_VALUE_IF_PENDING_EXCEPTION
        return promiseCapability.m_promise;
    }

    // Return ! AsyncFromSyncIteratorContinuation(result, promiseCapability).
    return asyncFromSyncIteratorContinuation(state, result, promiseCapability);
}

static Value builtinAsyncFromSyncIteratorReturn(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // https://tc39.es/ecma262/#sec-%asyncfromsynciteratorprototype%.return
    Value value = argv[0];
    // Let O be the this value.
    Value& O = thisValue;
    // Let promiseCapability be ! NewPromiseCapability(%Promise%).
    auto promiseCapability = PromiseObject::newPromiseCapability(state, state.context()->globalObject()->promise());
    ASSERT(!state.hasPendingException());
    // If Type(O) is not Object, or if O does not have a [[SyncIteratorRecord]] internal slot, then
    if (!O.isObject() || !O.asObject()->isAsyncFromSyncIteratorObject()) {
        // Let invalidIteratorError be a newly created TypeError object.
        Value invalidIteratorError = ErrorObject::createError(state, ErrorCode::TypeError, String::fromASCII("given this value is not Async-from-Sync Iterator"));
        // Perform ! Call(promiseCapability.[[Reject]], undefined, « invalidIteratorError »).
        Object::call(state, promiseCapability.m_rejectFunction, Value(), 1, &invalidIteratorError);
        RETURN_VALUE_IF_PENDING_EXCEPTION
        // Return promiseCapability.[[Promise]].
        return promiseCapability.m_promise;
    }
    // Let syncIterator be O.[[SyncIteratorRecord]].[[Iterator]].
    Object* syncIterator = O.asObject()->asAsyncFromSyncIteratorObject()->syncIteratorRecord()->m_iterator;
    // Let return be GetMethod(syncIterator, "return").
    Value returnVariable = Object::getMethod(state, syncIterator, state.context()->staticStrings().stringReturn);
    if (UNLIKELY(state.hasPendingException())) {
        // IfAbruptRejectPromise(return, promiseCapability).
        Value argv = state.detachPendingException();
        Object::call(state, promiseCapability.m_rejectFunction, Value(), 1, &argv);
        RETURN_VALUE_IF_PENDING_EXCEPTION
        return promiseCapability.m_promise;
    }

    // If return is undefined, then
    if (returnVariable.isUndefined()) {
        // Let iterResult be ! CreateIterResultObject(value, true).
        Value iterResult = IteratorObject::createIterResultObject(state, value, true);
        // Perform ! Call(promiseCapability.[[Resolve]], undefined, « iterResult »).
        Object::call(state, promiseCapability.m_resolveFunction, Value(), 1, &iterResult);
        RETURN_VALUE_IF_PENDING_EXCEPTION
        // Return promiseCapability.[[Promise]].
        return promiseCapability.m_promise;
    }

    Value result;
    // If value is present, then
    //   a. Let result be Call(return, syncIterator, « value »).
    // Else,
    //   a. Let result be Call(return, syncIterator).
    if (argc) {
        result = Object::call(state, returnVariable, syncIterator, 1, &value);
    } else {
        result = Object::call(state, returnVariable, syncIterator, 0, nullptr);
    }
    if (UNLIKELY(state.hasPendingException())) {
        // IfAbruptRejectPromise(return, promiseCapability).
        Value argv = state.detachPendingException();
        Object::call(state, promiseCapability.m_rejectFunction, Value(), 1, &argv);
        RETURN_VALUE_IF_PENDING_EXCEPTION
        return promiseCapability.m_promise;
    }

    // If Type(result) is not Object, then
    if (!result.isObject()) {
        // Perform ! Call(promiseCapability.[[Reject]], undefined, « a newly created TypeError object »).
        Value typeError = ErrorObject::createError(state, ErrorCode::TypeError, String::fromASCII("result of iterator is not Object"));
        Object::call(state, promiseCapability.m_rejectFunction, Value(), 1, &typeError);
        RETURN_VALUE_IF_PENDING_EXCEPTION
        // Return promiseCapability.[[Promise]].
        return promiseCapability.m_promise;
    }
    // Return ! AsyncFromSyncIteratorContinuation(result, promiseCapability).
    return asyncFromSyncIteratorContinuation(state, result.asObject(), promiseCapability);
}

static Value builtinAsyncFromSyncIteratorThrow(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // https://www.ecma-international.org/ecma-262/10.0/#sec-%asyncfromsynciteratorprototype%.throw
    Value value = argv[0];
    // Let O be the this value.
    Value& O = thisValue;
    // Let promiseCapability be ! NewPromiseCapability(%Promise%).
    auto promiseCapability = PromiseObject::newPromiseCapability(state, state.context()->globalObject()->promise());
    ASSERT(!state.hasPendingException());
    // If Type(O) is not Object, or if O does not have a [[SyncIteratorRecord]] internal slot, then
    if (!O.isObject() || !O.asObject()->isAsyncFromSyncIteratorObject()) {
        // Let invalidIteratorError be a newly created TypeError object.
        Value invalidIteratorError = ErrorObject::createError(state, ErrorCode::TypeError, String::fromASCII("given this value is not Async-from-Sync Iterator"));
        // Perform ! Call(promiseCapability.[[Reject]], undefined, « invalidIteratorError »).
        Object::call(state, promiseCapability.m_rejectFunction, Value(), 1, &invalidIteratorError);
        RETURN_VALUE_IF_PENDING_EXCEPTION
        // Return promiseCapability.[[Promise]].
        return promiseCapability.m_promise;
    }

    // Let syncIterator be O.[[SyncIteratorRecord]].[[Iterator]].
    Object* syncIterator = O.asObject()->asAsyncFromSyncIteratorObject()->syncIteratorRecord()->m_iterator;
    // Let throw be GetMethod(syncIterator, "throw").
    Value throwVariable = Object::getMethod(state, syncIterator, state.context()->staticStrings().stringThrow);
    if (state.hasPendingException()) {
        // IfAbruptRejectPromise(return, promiseCapability).
        Value argv = state.detachPendingException();
        Object::call(state, promiseCapability.m_rejectFunction, Value(), 1, &argv);
        RETURN_VALUE_IF_PENDING_EXCEPTION
        return promiseCapability.m_promise;
    }

    // If throw is undefined, then
    if (throwVariable.isUndefined()) {
        // Perform ! Call(promiseCapability.[[Reject]], undefined, « value »).
        Object::call(state, promiseCapability.m_rejectFunction, Value(), 1, &value);
        RETURN_VALUE_IF_PENDING_EXCEPTION
        // Return promiseCapability.[[Promise]].
        return promiseCapability.m_promise;
    }

    // Let result be Call(throw, syncIterator, « value »).
    Value result = Object::call(state, throwVariable, syncIterator, 1, &value);
    if (UNLIKELY(state.hasPendingException())) {
        // IfAbruptRejectPromise(result, promiseCapability).
        Value argv = state.detachPendingException();
        Object::call(state, promiseCapability.m_rejectFunction, Value(), 1, &argv);
        RETURN_VALUE_IF_PENDING_EXCEPTION
        return promiseCapability.m_promise;
    }

    // If Type(result) is not Object, then
    if (!result.isObject()) {
        // Perform ! Call(promiseCapability.[[Reject]], undefined, « a newly created TypeError object »).
        Value typeError = ErrorObject::createError(state, ErrorCode::TypeError, String::fromASCII("result of iterator is not Object"));
        Object::call(state, promiseCapability.m_rejectFunction, Value(), 1, &typeError);
        RETURN_VALUE_IF_PENDING_EXCEPTION
        // Return promiseCapability.[[Promise]].
        return promiseCapability.m_promise;
    }
    // Return ! AsyncFromSyncIteratorContinuation(result, promiseCapability).
    return asyncFromSyncIteratorContinuation(state, result.asObject(), promiseCapability);
}

void GlobalObject::initializeAsyncFromSyncIterator(ExecutionState& state)
{
    // do nothing
}

void GlobalObject::installAsyncFromSyncIterator(ExecutionState& state)
{
    ASSERT(!!m_asyncIteratorPrototype);

    // https://www.ecma-international.org/ecma-262/10.0/#sec-%asyncfromsynciteratorprototype%-object
    m_asyncFromSyncIteratorPrototype = new PrototypeObject(state, m_asyncIteratorPrototype);
    m_asyncFromSyncIteratorPrototype->setGlobalIntrinsicObject(state, true);

    m_asyncFromSyncIteratorPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().toStringTag),
                                                              ObjectPropertyDescriptor(String::fromASCII("Async-from-Sync Iterator"), ObjectPropertyDescriptor::ConfigurablePresent));

    m_asyncFromSyncIteratorPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().next),
                                                              ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().next, builtinAsyncFromSyncIteratorNext, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_asyncFromSyncIteratorPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().stringReturn),
                                                              ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().stringReturn, builtinAsyncFromSyncIteratorReturn, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_asyncFromSyncIteratorPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().stringThrow),
                                                              ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().stringThrow, builtinAsyncFromSyncIteratorThrow, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}

} // namespace Escargot
