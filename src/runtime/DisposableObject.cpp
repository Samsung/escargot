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

namespace Escargot {

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

} // namespace Escargot
