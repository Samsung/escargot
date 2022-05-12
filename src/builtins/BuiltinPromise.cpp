/*
 * Copyright (c) 2017-present Samsung Electronics Co., Ltd
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
#include "runtime/PromiseObject.h"
#include "runtime/ArrayObject.h"
#include "runtime/JobQueue.h"
#include "runtime/SandBox.h"
#include "runtime/NativeFunctionObject.h"
#include "runtime/IteratorObject.h"
#include "runtime/ExtendedNativeFunctionObject.h"

namespace Escargot {

// $25.4.3 Promise(executor)
static Value builtinPromiseConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    auto strings = &state.context()->staticStrings();
    if (!newTarget.hasValue()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->Promise.string(), false, String::emptyString, "%s: Promise constructor should be called with new Promise()");
    }

    Value executor = argv[0];
    if (!executor.isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->Promise.string(), false, String::emptyString, "%s: Promise executor is not a function object");
    }

    // Let promise be ? OrdinaryCreateFromConstructor(NewTarget, "%PromisePrototype%", « [[PromiseState]], [[PromiseResult]], [[PromiseFulfillReactions]], [[PromiseRejectReactions]], [[PromiseIsHandled]] »).
    Object* proto = Object::getPrototypeFromConstructor(state, newTarget.value(), [](ExecutionState& state, Context* constructorRealm) -> Object* {
        return constructorRealm->globalObject()->promisePrototype();
    });
    PromiseObject* promise = new PromiseObject(state, proto);

    PromiseReaction::Capability capability = promise->createResolvingFunctions(state);

    // PromiseHook for Promise initialization case
    if (UNLIKELY(state.context()->vmInstance()->isPromiseHookRegistered())) {
        // Note) To pass parent promise, we allocate the second argument (argv[1]) for the parent promise
        // otherwise, the second argument would be ignored
        state.context()->vmInstance()->triggerPromiseHook(state, VMInstance::PromiseHookType::Init, promise, (argc > 1) ? argv[1] : Value());
    }

    SandBox sb(state.context());
    auto res = sb.run([&]() -> Value {
        Value arguments[] = { capability.m_resolveFunction, capability.m_rejectFunction };
        Object::call(state, executor, Value(), 2, arguments);
        return Value();
    });
    if (!res.error.isEmpty()) {
        Value arguments[] = { res.error };
        Object::call(state, capability.m_rejectFunction, Value(), 1, arguments);
    }

    return promise;
}

// https://tc39.es/ecma262/#sec-getpromiseresolve
static Value getPromiseResolve(ExecutionState& state, Object* promiseConstructor)
{
    // Assert: IsConstructor(promiseConstructor) is true.
    // Let promiseResolve be ? Get(promiseConstructor, "resolve").
    auto promiseResolve = promiseConstructor->get(state, state.context()->staticStrings().resolve).value(state, promiseConstructor);
    // If IsCallable(promiseResolve) is false, throw a TypeError exception.
    if (!promiseResolve.isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Promise resolve is not callable");
    }
    // Return promiseResolve.
    return promiseResolve;
}

static Value builtinPromiseAll(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // https://tc39.es/ecma262/#sec-performpromiseall
    auto strings = &state.context()->staticStrings();

    // Let C be the this value.
    // If Type(C) is not Object, throw a TypeError exception.
    if (!thisValue.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->Promise.string(), false, strings->all.string(), ErrorObject::Messages::GlobalObject_ThisNotObject);
    }
    Object* C = thisValue.asObject();
    // Let promiseCapability be NewPromiseCapability(C).
    PromiseReaction::Capability promiseCapability = PromiseObject::newPromiseCapability(state, C);
    // Let promiseResolve be GetPromiseResolve(C).
    Value promiseResolve;
    try {
        promiseResolve = getPromiseResolve(state, C);
    } catch (const Value& v) {
        Value thrownValue = v;
        // If value is an abrupt completion,
        // Perform ? Call(capability.[[Reject]], undefined, « value.[[Value]] »).
        Object::call(state, promiseCapability.m_rejectFunction, Value(), 1, &thrownValue);
        // Return capability.[[Promise]].
        return promiseCapability.m_promise;
    }

    // Let iteratorRecord be GetIterator(iterable).
    // IfAbruptRejectPromise(iteratorRecord, promiseCapability).
    IteratorRecord* iteratorRecord;
    try {
        iteratorRecord = IteratorObject::getIterator(state, argv[0]);
    } catch (const Value& v) {
        Value thrownValue = v;
        // If value is an abrupt completion,
        // Perform ? Call(capability.[[Reject]], undefined, « value.[[Value]] »).
        Object::call(state, promiseCapability.m_rejectFunction, Value(), 1, &thrownValue);
        // Return capability.[[Promise]].
        return promiseCapability.m_promise;
    }

    // Let result be PerformPromiseAll(iteratorRecord, C, promiseCapability).
    Value result;
    try {
        // Let values be a new empty List.
        ValueVector* values = new ValueVector();
        // Let remainingElementsCount be a new Record { [[value]]: 1 }.
        size_t* remainingElementsCount = new (PointerFreeGC) size_t(1);

        // Let index be 0.
        int64_t index = 0;

        // Repeat
        while (true) {
            // Let next be IteratorStep(iteratorRecord).
            Optional<Object*> next;
            try {
                next = IteratorObject::iteratorStep(state, iteratorRecord);
            } catch (const Value& e) {
                // If next is an abrupt completion, set iteratorRecord.[[Done]] to true.
                iteratorRecord->m_done = true;
                // ReturnIfAbrupt(next).
                state.throwException(e);
            }
            // If next is false,
            if (!next.hasValue()) {
                // set iteratorRecord.[[done]] to true.
                iteratorRecord->m_done = true;
                // Set remainingElementsCount.[[value]] to remainingElementsCount.[[value]] − 1.
                *remainingElementsCount = *remainingElementsCount - 1;
                // If remainingElementsCount.[[value]] is 0,
                if (*remainingElementsCount == 0) {
                    // Let valuesArray be CreateArrayFromList(values).
                    // Perform ? Call(resultCapability.[[Resolve]], undefined, « valuesArray »).
                    Value argv = Object::createArrayFromList(state, *values);
                    Value resolveResult = Object::call(state, promiseCapability.m_resolveFunction, Value(), 1, &argv);
                }
                // Return resultCapability.[[Promise]].
                result = promiseCapability.m_promise;
                break;
            }
            // Let nextValue be IteratorValue(next).
            Value nextValue;
            try {
                nextValue = IteratorObject::iteratorValue(state, next.value());
            } catch (const Value& e) {
                // If next is an abrupt completion, set iteratorRecord.[[done]] to true.
                iteratorRecord->m_done = true;
                // ReturnIfAbrupt(nextValue).
                state.throwException(e);
            }
            // Append undefined to values.
            values->pushBack(Value());
            // Let nextPromise be Invoke(constructor, "resolve", « nextValue »).
            Value nextPromise = Object::call(state, promiseResolve, C, 1, &nextValue);

            // Let resolveElement be a new built-in function object as defined in Promise.all Resolve Element Functions.
            ExtendedNativeFunctionObject* resolveElement = new ExtendedNativeFunctionObjectImpl<6>(state, NativeFunctionInfo(AtomicString(), PromiseObject::promiseAllResolveElementFunction, 1, NativeFunctionInfo::Strict));

            // Set the [[AlreadyCalled]] internal slot of resolveElement to a new Record {[[value]]: false }.
            // Set the [[Index]] internal slot of resolveElement to index.
            // Set the [[Values]] internal slot of resolveElement to values.
            // Set the [[Capabilities]] internal slot of resolveElement to resultCapability.
            // Set the [[RemainingElements]] internal slot of resolveElement to remainingElementsCount.
            bool* alreadyCalled = new (PointerFreeGC) bool(false);

            resolveElement->setInternalSlotAsPointer(PromiseObject::BuiltinFunctionSlot::AlreadyCalled, alreadyCalled);
            resolveElement->setInternalSlot(PromiseObject::BuiltinFunctionSlot::Index, Value(index));
            resolveElement->setInternalSlotAsPointer(PromiseObject::BuiltinFunctionSlot::Values, values);
            resolveElement->setInternalSlot(PromiseObject::BuiltinFunctionSlot::Resolve, promiseCapability.m_resolveFunction);
            resolveElement->setInternalSlot(PromiseObject::BuiltinFunctionSlot::Reject, promiseCapability.m_rejectFunction);
            resolveElement->setInternalSlotAsPointer(PromiseObject::BuiltinFunctionSlot::RemainingElements, remainingElementsCount);

            // Set remainingElementsCount.[[value]] to remainingElementsCount.[[value]] + 1.
            *remainingElementsCount = *remainingElementsCount + 1;
            // Perform ? Invoke(nextPromise, "then", « resolveElement, resultCapability.[[Reject]] »).
            Object* nextPromiseObject = nextPromise.toObject(state);
            Value argv[] = { Value(resolveElement), Value(promiseCapability.m_rejectFunction) };
            Object::call(state, nextPromiseObject->get(state, strings->then).value(state, nextPromiseObject), nextPromiseObject, 2, argv);
            // Increase index by 1.
            index++;
        }
    } catch (const Value& v) {
        Value exceptionValue = v;
        // If result is an abrupt completion,
        // If iteratorRecord.[[Done]] is false, set result to IteratorClose(iteratorRecord, result).
        // IfAbruptRejectPromise(result, promiseCapability).
        try {
            if (!iteratorRecord->m_done) {
                result = IteratorObject::iteratorClose(state, iteratorRecord, exceptionValue, true);
            }
        } catch (const Value& v) {
            exceptionValue = v;
        }

        // If value is an abrupt completion,
        // Perform ? Call(capability.[[Reject]], undefined, « value.[[Value]] »).
        Object::call(state, promiseCapability.m_rejectFunction, Value(), 1, &exceptionValue);
        // Return capability.[[Promise]].
        return promiseCapability.m_promise;
    }

    // Return Completion(result).
    return result;
}

static Value builtinPromiseRace(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // https://tc39.es/ecma262/#sec-performpromiserace
    auto strings = &state.context()->staticStrings();
    // Let C be the this value.
    // If Type(C) is not Object, throw a TypeError exception.
    if (!thisValue.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->Promise.string(), false, strings->race.string(), ErrorObject::Messages::GlobalObject_ThisNotObject);
    }
    Object* C = thisValue.asObject();

    // Let promiseCapability be NewPromiseCapability(C).
    // ReturnIfAbrupt(promiseCapability).
    PromiseReaction::Capability promiseCapability = PromiseObject::newPromiseCapability(state, C);
    // Let promiseResolve be GetPromiseResolve(C).
    Value promiseResolve;
    try {
        promiseResolve = getPromiseResolve(state, C);
    } catch (const Value& v) {
        Value thrownValue = v;
        // If value is an abrupt completion,
        // Perform ? Call(capability.[[Reject]], undefined, « value.[[Value]] »).
        Object::call(state, promiseCapability.m_rejectFunction, Value(), 1, &thrownValue);
        // Return capability.[[Promise]].
        return promiseCapability.m_promise;
    }

    // Let iteratorRecord be GetIterator(iterable).
    // IfAbruptRejectPromise(iteratorRecord, promiseCapability).
    IteratorRecord* iteratorRecord;
    try {
        iteratorRecord = IteratorObject::getIterator(state, argv[0]);
    } catch (const Value& v) {
        Value thrownValue = v;
        // If value is an abrupt completion,
        // Let rejectResult be Call(capability.[[Reject]], undefined, «value.[[value]]»).
        Object::call(state, promiseCapability.m_rejectFunction, Value(), 1, &thrownValue);
        // Return capability.[[Promise]].
        return promiseCapability.m_promise;
    }

    // Let result be PerformPromiseRace(iteratorRecord, C, promiseCapability).
    Value result;
    try {
        // Repeat
        while (true) {
            // Let next be IteratorStep(iteratorRecord).
            Optional<Object*> next;
            try {
                next = IteratorObject::iteratorStep(state, iteratorRecord);
            } catch (const Value& e) {
                // If next is an abrupt completion, set iteratorRecord.[[done]] to true.
                // ReturnIfAbrupt(next).
                iteratorRecord->m_done = true;
                state.throwException(e);
            }
            // If next is false, then
            if (!next.hasValue()) {
                // Set iteratorRecord.[[done]] to true.
                iteratorRecord->m_done = true;
                // Return resultCapability.[[Promise]].
                result = promiseCapability.m_promise;
                break;
            }

            // Let nextValue be IteratorValue(next).
            Value nextValue;
            try {
                nextValue = IteratorObject::iteratorValue(state, next.value());
            } catch (const Value& e) {
                // If next is an abrupt completion, set iteratorRecord.[[done]] to true.
                iteratorRecord->m_done = true;
                // ReturnIfAbrupt(next).
                state.throwException(e);
            }

            // Let nextPromise be Invoke(C, "resolve", «nextValue»).
            Value nextPromise = Object::call(state, promiseResolve, C, 1, &nextValue);
            // Perform ? Invoke(nextPromise, "then", « resultCapability.[[Resolve]], resultCapability.[[Reject]] »).
            Object* nextPromiseObject = nextPromise.toObject(state);
            Value argv[] = { Value(promiseCapability.m_resolveFunction), Value(promiseCapability.m_rejectFunction) };
            Object::call(state, nextPromiseObject->get(state, strings->then).value(state, nextPromiseObject), nextPromiseObject, 2, argv);
        }
    } catch (const Value& e) {
        Value exceptionValue = e;
        // If result is an abrupt completion, then
        // If iteratorRecord.[[Done]] is false, set result to IteratorClose(iteratorRecord, result).
        // IfAbruptRejectPromise(result, promiseCapability).
        try {
            if (!iteratorRecord->m_done) {
                result = IteratorObject::iteratorClose(state, iteratorRecord, exceptionValue, true);
            }
        } catch (const Value& v) {
            exceptionValue = v;
        }
        // If value is an abrupt completion,
        // Perform ? Call(capability.[[Reject]], undefined, « value.[[Value]] »).
        Object::call(state, promiseCapability.m_rejectFunction, Value(), 1, &exceptionValue);
        // Return capability.[[Promise]].
        return promiseCapability.m_promise;
    }

    // Return Completion(result).
    return result;
}

static Value builtinPromiseReject(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    auto strings = &state.context()->staticStrings();
    Object* thisObject = thisValue.toObject(state);
    PromiseReaction::Capability capability = PromiseObject::newPromiseCapability(state, thisObject);

    Value arguments[] = { argv[0] };
    Object::call(state, capability.m_rejectFunction, Value(), 1, arguments);
    return capability.m_promise;
}

// http://www.ecma-international.org/ecma-262/10.0/#sec-promise.resolve
static Value builtinPromiseResolve(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Let C be the this value.
    const Value& C = thisValue;
    // If Type(C) is not Object, throw a TypeError exception.
    if (!C.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Promise.string(), false, state.context()->staticStrings().resolve.string(), "%s: PromiseResolve called on non-object");
    }
    // Return ? PromiseResolve(C, x).
    return PromiseObject::promiseResolve(state, C.asObject(), argv[0]);
}

static Value builtinPromiseCatch(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    auto strings = &state.context()->staticStrings();
    Object* thisObject = thisValue.toObject(state);
    Value onRejected = argv[0];
    Value then = thisObject->get(state, strings->then).value(state, thisObject);
    Value arguments[] = { Value(), onRejected };
    return Object::call(state, then, thisObject, 2, arguments);
}


static Value builtinPromiseFinally(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // https://www.ecma-international.org/ecma-262/10.0/#sec-promise.prototype.finally
    auto strings = &state.context()->staticStrings();

    if (!thisValue.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->Promise.string(), false, strings->finally.string(), "%s: not a Promise object");
    }

    Object* thisObject = thisValue.asObject();
    Value C = thisObject->speciesConstructor(state, state.context()->globalObject()->promise());

    Value onFinally = argv[0];
    Value arguments[] = { onFinally, onFinally };

    if (onFinally.isCallable()) {
        ExtendedNativeFunctionObject* thenFinally = new ExtendedNativeFunctionObjectImpl<2>(state, NativeFunctionInfo(AtomicString(), PromiseObject::promiseThenFinally, 1, NativeFunctionInfo::Strict));
        thenFinally->setInternalSlot(PromiseObject::BuiltinFunctionSlot::Constructor, C);
        thenFinally->setInternalSlot(PromiseObject::BuiltinFunctionSlot::OnFinally, onFinally);

        ExtendedNativeFunctionObject* catchFinally = new ExtendedNativeFunctionObjectImpl<2>(state, NativeFunctionInfo(AtomicString(), PromiseObject::promiseCatchFinally, 1, NativeFunctionInfo::Strict));
        catchFinally->setInternalSlot(PromiseObject::BuiltinFunctionSlot::Constructor, C);
        catchFinally->setInternalSlot(PromiseObject::BuiltinFunctionSlot::OnFinally, onFinally);

        arguments[0] = thenFinally;
        arguments[1] = catchFinally;
    }

    Value then = thisObject->get(state, strings->then).value(state, thisObject);
    return Object::call(state, then, thisObject, 2, arguments);
}

static Value builtinPromiseThen(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    auto strings = &state.context()->staticStrings();
    if (!thisValue.isObject() || !thisValue.asObject()->isPromiseObject())
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->Promise.string(), false, strings->then.string(), "%s: not a Promise object");
    Value C = thisValue.asObject()->speciesConstructor(state, state.context()->globalObject()->promise());
    PromiseReaction::Capability promiseCapability = PromiseObject::newPromiseCapability(state, C.asObject(), thisValue.asObject()->asPromiseObject());
    return thisValue.asObject()->asPromiseObject()->then(state, argv[0], argv[1], promiseCapability).value();
}

// https://tc39.es/ecma262/#sec-performpromiseallsettled
static Value performPromiseAllSettled(ExecutionState& state, IteratorRecord* iteratorRecord, Object* constructor, PromiseReaction::Capability& resultCapability, Value promiseResolve)
{
    // Assert: ! IsConstructor(constructor) is true.
    // Assert: resultCapability is a PromiseCapability Record.
    // Let values be a new empty List.
    ValueVector* values = new ValueVector();
    // Let remainingElementsCount be a new Record { [[Value]]: 1 }.
    size_t* remainingElementsCount = new (PointerFreeGC) size_t(1);
    // Let index be 0.
    size_t index = 0;

    // Repeat,
    while (true) {
        // Let next be IteratorStep(iteratorRecord).
        Optional<Object*> next;
        try {
            next = IteratorObject::iteratorStep(state, iteratorRecord);
        } catch (const Value& e) {
            // If next is an abrupt completion, set iteratorRecord.[[Done]] to true.
            iteratorRecord->m_done = true;
            // ReturnIfAbrupt(next).
            state.throwException(e);
        }

        // If next is false, then
        if (!next.hasValue()) {
            // Set iteratorRecord.[[Done]] to true.
            iteratorRecord->m_done = true;
            // Set remainingElementsCount.[[Value]] to remainingElementsCount.[[Value]] - 1.
            *remainingElementsCount = *remainingElementsCount - 1;
            // If remainingElementsCount.[[Value]] is 0, then
            if (*remainingElementsCount == 0) {
                // Let valuesArray be ! CreateArrayFromList(values).
                Value valuesArray = ArrayObject::createArrayFromList(state, *values);
                // Perform ? Call(resultCapability.[[Resolve]], undefined, « valuesArray »).
                Object::call(state, resultCapability.m_resolveFunction, Value(), 1, &valuesArray);
            }
            // Return resultCapability.[[Promise]].
            return resultCapability.m_promise;
        }

        // Let nextValue be IteratorValue(next).
        Value nextValue;
        try {
            nextValue = IteratorObject::iteratorValue(state, next.value());
        } catch (const Value& e) {
            // If nextValue is an abrupt completion, set iteratorRecord.[[Done]] to true.
            iteratorRecord->m_done = true;
            // ReturnIfAbrupt(nextValue).
            state.throwException(e);
        }

        // Append undefined to values.
        values->pushBack(Value());
        // Let nextPromise be ? Call(promiseResolve, constructor, « nextValue »).
        Value nextValueArgv = nextValue;
        Value nextPromise = Object::call(state, promiseResolve, constructor, 1, &nextValueArgv);
        // Let steps be the algorithm steps defined in Promise.allSettled Resolve Element Functions.
        // Let resolveElement be ! CreateBuiltinFunction(steps, « [[AlreadyCalled]], [[Index]], [[Values]], [[Capability]], [[RemainingElements]] »).
        // Let alreadyCalled be the Record { [[Value]]: false }.
        bool* alreadyCalled = new (PointerFreeGC) bool(false);
        // Set resolveElement.[[AlreadyCalled]] to alreadyCalled.
        // Set resolveElement.[[Index]] to index.
        // Set resolveElement.[[Values]] to values.
        // Set resolveElement.[[Capability]] to resultCapability.
        // Set resolveElement.[[RemainingElements]] to remainingElementsCount.
        auto resolveElement = new ExtendedNativeFunctionObjectImpl<6>(state, NativeFunctionInfo(AtomicString(), PromiseObject::promiseAllSettledResolveElementFunction, 1, NativeFunctionInfo::Strict));
        resolveElement->setInternalSlotAsPointer(PromiseObject::BuiltinFunctionSlot::AlreadyCalled, alreadyCalled);
        resolveElement->setInternalSlot(PromiseObject::BuiltinFunctionSlot::Index, Value(index));
        resolveElement->setInternalSlotAsPointer(PromiseObject::BuiltinFunctionSlot::Values, values);
        resolveElement->setInternalSlot(PromiseObject::BuiltinFunctionSlot::Resolve, resultCapability.m_resolveFunction);
        resolveElement->setInternalSlot(PromiseObject::BuiltinFunctionSlot::Reject, resultCapability.m_rejectFunction);
        resolveElement->setInternalSlotAsPointer(PromiseObject::BuiltinFunctionSlot::RemainingElements, remainingElementsCount);

        // Let rejectSteps be the algorithm steps defined in Promise.allSettled Reject Element Functions.
        // Let rejectElement be ! CreateBuiltinFunction(rejectSteps, « [[AlreadyCalled]], [[Index]], [[Values]], [[Capability]], [[RemainingElements]] »).
        // Set rejectElement.[[AlreadyCalled]] to alreadyCalled.
        // Set rejectElement.[[Index]] to index.
        // Set rejectElement.[[Values]] to values.
        // Set rejectElement.[[Capability]] to resultCapability.
        // Set rejectElement.[[RemainingElements]] to remainingElementsCount.
        auto rejectElement = new ExtendedNativeFunctionObjectImpl<6>(state, NativeFunctionInfo(AtomicString(), PromiseObject::promiseAllSettledRejectElementFunction, 1, NativeFunctionInfo::Strict));
        rejectElement->setInternalSlotAsPointer(PromiseObject::BuiltinFunctionSlot::AlreadyCalled, alreadyCalled);
        rejectElement->setInternalSlot(PromiseObject::BuiltinFunctionSlot::Index, Value(index));
        rejectElement->setInternalSlotAsPointer(PromiseObject::BuiltinFunctionSlot::Values, values);
        rejectElement->setInternalSlot(PromiseObject::BuiltinFunctionSlot::Resolve, resultCapability.m_resolveFunction);
        rejectElement->setInternalSlot(PromiseObject::BuiltinFunctionSlot::Reject, resultCapability.m_rejectFunction);
        rejectElement->setInternalSlotAsPointer(PromiseObject::BuiltinFunctionSlot::RemainingElements, remainingElementsCount);

        // Set remainingElementsCount.[[Value]] to remainingElementsCount.[[Value]] + 1.
        *remainingElementsCount = *remainingElementsCount + 1;
        // Perform ? Invoke(nextPromise, "then", « resolveElement, rejectElement »).
        Value argv[2] = { resolveElement, rejectElement };
        Object::call(state, Object::getMethod(state, nextPromise, state.context()->staticStrings().then), nextPromise, 2, argv);
        // Set index to index + 1.
        index++;
    }

    return Value();
}

// https://tc39.es/ecma262/#sec-promise.allsettled
static Value builtinPromiseAllSettled(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Let C be the this value.
    // If Type(C) is not Object, throw a TypeError exception.
    if (!thisValue.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "this value of allSettled is not Object");
    }
    Object* C = thisValue.asObject();
    // Let promiseCapability be ? NewPromiseCapability(C).
    auto promiseCapability = PromiseObject::newPromiseCapability(state, C);
    // Let promiseResolve be GetPromiseResolve(C).
    Value promiseResolve;
    try {
        promiseResolve = getPromiseResolve(state, C);
    } catch (const Value& v) {
        Value thrownValue = v;
        // If value is an abrupt completion,
        // Perform ? Call(capability.[[Reject]], undefined, « value.[[Value]] »).
        Object::call(state, promiseCapability.m_rejectFunction, Value(), 1, &thrownValue);
        // Return capability.[[Promise]].
        return promiseCapability.m_promise;
    }

    // Let iteratorRecord be GetIterator(iterable).
    // IfAbruptRejectPromise(iteratorRecord, promiseCapability).
    IteratorRecord* iteratorRecord;
    try {
        iteratorRecord = IteratorObject::getIterator(state, argv[0]);
    } catch (const Value& v) {
        Value thrownValue = v;
        // If value is an abrupt completion,
        // Perform ? Call(capability.[[Reject]], undefined, « value.[[Value]] »).
        Object::call(state, promiseCapability.m_rejectFunction, Value(), 1, &thrownValue);
        // Return capability.[[Promise]].
        return promiseCapability.m_promise;
    }

    Value result;
    // Let result be PerformPromiseAllSettled(iteratorRecord, C, promiseCapability).
    try {
        result = performPromiseAllSettled(state, iteratorRecord, C, promiseCapability, promiseResolve);
    } catch (const Value& v) {
        Value exceptionValue = v;
        // If result is an abrupt completion,
        // If iteratorRecord.[[Done]] is false, set result to IteratorClose(iteratorRecord, result).
        if (!iteratorRecord->m_done) {
            try {
                result = IteratorObject::iteratorClose(state, iteratorRecord, exceptionValue, true);
            } catch (const Value& v) {
                // IfAbruptRejectPromise(result, promiseCapability).
                // If value is an abrupt completion,
                // Perform ? Call(capability.[[Reject]], undefined, « value.[[Value]] »).
                Object::call(state, promiseCapability.m_rejectFunction, Value(), 1, &exceptionValue);
                // Return capability.[[Promise]].
                return promiseCapability.m_promise;
            }
        } else {
            // IfAbruptRejectPromise(result, promiseCapability).
            // If value is an abrupt completion,
            // Perform ? Call(capability.[[Reject]], undefined, « value.[[Value]] »).
            Object::call(state, promiseCapability.m_rejectFunction, Value(), 1, &exceptionValue);
            // Return capability.[[Promise]].
            return promiseCapability.m_promise;
        }
    }
    // Return Completion(result).
    return result;
}

// https://tc39.es/ecma262/#sec-performpromiseany
static Value performPromiseAny(ExecutionState& state, IteratorRecord* iteratorRecord, Object* constructor, const PromiseReaction::Capability& resultCapability, const Value& promiseResolve)
{
    const StaticStrings* strings = &state.context()->staticStrings();
    // Assert: ! IsConstructor(constructor) is true.
    // Assert: ! IsCallable(promiseResolve) is true.
    // Let errors be a new empty List.
    ValueVector* errors = new ValueVector();
    // Let remainingElementsCount be the Record { [[Value]]: 1 }.
    size_t* remainingElementsCount = new (PointerFreeGC) size_t(1);
    // Let index be 0.
    int64_t index = 0;
    // Repeat,
    while (true) {
        // Let next be IteratorStep(iteratorRecord).
        // If next is an abrupt completion, set iteratorRecord.[[Done]] to true.
        // ReturnIfAbrupt(next).
        Optional<Object*> next;
        try {
            next = IteratorObject::iteratorStep(state, iteratorRecord);
        } catch (const Value& v) {
            iteratorRecord->m_done = true;
            throw v;
        }

        // If next is false, then
        if (!next.hasValue()) {
            // Set iteratorRecord.[[Done]] to true.
            iteratorRecord->m_done = true;
            // Set remainingElementsCount.[[Value]] to remainingElementsCount.[[Value]] - 1.
            *remainingElementsCount = *remainingElementsCount - 1;
            // If remainingElementsCount.[[Value]] is 0, then
            if (*remainingElementsCount == 0) {
                // Let error be a newly created AggregateError object.
                ErrorObject* error = ErrorObject::createBuiltinError(state, ErrorObject::AggregateError, "Got AggregateError on processing Promise.any");
                // Perform ! DefinePropertyOrThrow(error, "errors", PropertyDescriptor { [[Configurable]]: true, [[Enumerable]]: false, [[Writable]]: true, [[Value]]: ! CreateArrayFromList(errors) }).
                error->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, String::fromASCII("errors")),
                                                        ObjectPropertyDescriptor(Object::createArrayFromList(state, *errors),
                                                                                 (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent | ObjectPropertyDescriptor::WritablePresent)));
                // Return ThrowCompletion(error).
                throw Value(error);
            }
            // Return resultCapability.[[Promise]].
            return resultCapability.m_promise;
        }

        // Let nextValue be IteratorValue(next).
        Value nextValue;
        try {
            nextValue = IteratorObject::iteratorValue(state, next.value());
        } catch (const Value& v) {
            // If nextValue is an abrupt completion, set iteratorRecord.[[Done]] to true.
            iteratorRecord->m_done = true;
            // ReturnIfAbrupt(nextValue).
            throw v;
        }
        // Append undefined to errors.
        errors->pushBack(Value());
        // Let nextPromise be ? Call(promiseResolve, constructor, « nextValue »).
        Value argv = nextValue;
        Value nextPromise = Object::call(state, promiseResolve, constructor, 1, &argv);
        // Let steps be the algorithm steps defined in Promise.any Reject Element Functions.
        // Let rejectElement be ! CreateBuiltinFunction(steps, « [[AlreadyCalled]], [[Index]], [[Errors]], [[Capability]], [[RemainingElements]] »).
        ExtendedNativeFunctionObject* rejectElement = new ExtendedNativeFunctionObjectImpl<6>(state, NativeFunctionInfo(AtomicString(), PromiseObject::promiseAnyRejectElementFunction, 1, NativeFunctionInfo::Strict));
        // Set rejectElement.[[AlreadyCalled]] to false.
        // Set rejectElement.[[Index]] to index.
        // Set rejectElement.[[Errors]] to errors.
        // Set rejectElement.[[Capability]] to resultCapability.
        // Set rejectElement.[[RemainingElements]] to remainingElementsCount.
        bool* alreadyCalled = new (PointerFreeGC) bool(false);
        rejectElement->setInternalSlotAsPointer(PromiseObject::BuiltinFunctionSlot::AlreadyCalled, alreadyCalled);
        rejectElement->setInternalSlot(PromiseObject::BuiltinFunctionSlot::Index, Value(index));
        rejectElement->setInternalSlotAsPointer(PromiseObject::BuiltinFunctionSlot::Errors, errors);
        rejectElement->setInternalSlot(PromiseObject::BuiltinFunctionSlot::Resolve, resultCapability.m_resolveFunction);
        rejectElement->setInternalSlot(PromiseObject::BuiltinFunctionSlot::Reject, resultCapability.m_rejectFunction);
        rejectElement->setInternalSlotAsPointer(PromiseObject::BuiltinFunctionSlot::RemainingElements, remainingElementsCount);
        // Set remainingElementsCount.[[Value]] to remainingElementsCount.[[Value]] + 1.
        *remainingElementsCount = *remainingElementsCount + 1;

        // Perform ? Invoke(nextPromise, "then", « resultCapability.[[Resolve]], rejectElement »).
        Object* nextPromiseObject = nextPromise.toObject(state);
        Value argv2[] = { Value(resultCapability.m_resolveFunction), Value(rejectElement) };
        Object::call(state, nextPromiseObject->get(state, strings->then).value(state, nextPromiseObject), nextPromiseObject, 2, argv2);
        // Set index to index + 1.
        index++;
    }
    ASSERT_NOT_REACHED();
}

// https://tc39.es/ecma262/#sec-promise.any
static Value builtinPromiseAny(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    const StaticStrings* strings = &state.context()->staticStrings();

    // Let C be the this value.
    // If Type(C) is not Object, throw a TypeError exception.
    if (!thisValue.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->Promise.string(), false, strings->all.string(), ErrorObject::Messages::GlobalObject_ThisNotObject);
    }
    Object* C = thisValue.asObject();

    // Let promiseCapability be NewPromiseCapability(C).
    PromiseReaction::Capability promiseCapability = PromiseObject::newPromiseCapability(state, C);
    // Let promiseResolve be GetPromiseResolve(C).
    // IfAbruptRejectPromise(promiseResolve, promiseCapability).
    Value promiseResolve;
    try {
        promiseResolve = getPromiseResolve(state, C);
    } catch (const Value& v) {
        Value thrownValue = v;
        // If value is an abrupt completion,
        // Perform ? Call(capability.[[Reject]], undefined, « value.[[Value]] »).
        Object::call(state, promiseCapability.m_rejectFunction, Value(), 1, &thrownValue);
        // Return capability.[[Promise]].
        return promiseCapability.m_promise;
    }

    // Let iteratorRecord be GetIterator(iterable).
    // IfAbruptRejectPromise(iteratorRecord, promiseCapability).
    const Value& iterable = argv[0];
    IteratorRecord* iteratorRecord = nullptr;
    try {
        iteratorRecord = IteratorObject::getIterator(state, iterable);
    } catch (const Value& v) {
        Value thrownValue = v;
        // If value is an abrupt completion,
        // Perform ? Call(capability.[[Reject]], undefined, « value.[[Value]] »).
        Object::call(state, promiseCapability.m_rejectFunction, Value(), 1, &thrownValue);
        // Return capability.[[Promise]].
        return promiseCapability.m_promise;
    }

    // Let result be PerformPromiseAny(iteratorRecord, C, promiseCapability, promiseResolve).
    Value result;
    try {
        result = performPromiseAny(state, iteratorRecord, C, promiseCapability, promiseResolve);
    } catch (const Value& v) {
        Value thrownValue = v;
        // If result is an abrupt completion, then
        // If iteratorRecord.[[Done]] is false, set result to IteratorClose(iteratorRecord, result).
        try {
            if (!iteratorRecord->m_done) {
                IteratorObject::iteratorClose(state, iteratorRecord, thrownValue, true);
            }
        } catch (const Value& v) {
            thrownValue = v;
        }
        // IfAbruptRejectPromise(result, promiseCapability).
        Object::call(state, promiseCapability.m_rejectFunction, Value(), 1, &thrownValue);
        return promiseCapability.m_promise;
    }
    // Return Completion(result).
    return result;
}

void GlobalObject::initializePromise(ExecutionState& state)
{
    ObjectPropertyNativeGetterSetterData* nativeData = new ObjectPropertyNativeGetterSetterData(true, false, true,
                                                                                                [](ExecutionState& state, Object* self, const Value& receiver, const EncodedValue& privateDataFromObjectPrivateArea) -> Value {
                                                                                                    ASSERT(self->isGlobalObject());
                                                                                                    return self->asGlobalObject()->promise();
                                                                                                },
                                                                                                nullptr);

    defineNativeDataAccessorProperty(state, ObjectPropertyName(state.context()->staticStrings().Promise), nativeData, Value(Value::EmptyValue));
}

void GlobalObject::installPromise(ExecutionState& state)
{
    const StaticStrings* strings = &state.context()->staticStrings();
    m_promise = new NativeFunctionObject(state, NativeFunctionInfo(strings->Promise, builtinPromiseConstructor, 1), NativeFunctionObject::__ForBuiltinConstructor__);
    m_promise->setGlobalIntrinsicObject(state);

    {
        JSGetterSetter gs(
            new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().getSymbolSpecies, builtinSpeciesGetter, 0, NativeFunctionInfo::Strict)), Value(Value::EmptyValue));
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        m_promise->directDefineOwnProperty(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().species), desc);
    }

    m_promisePrototype = new PrototypeObject(state);
    m_promisePrototype->setGlobalIntrinsicObject(state, true);

    m_promisePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->constructor), ObjectPropertyDescriptor(m_promise, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_promisePrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().toStringTag),
                                                ObjectPropertyDescriptor(Value(state.context()->staticStrings().Promise.string()), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));

    // $25.4.4.1 Promise.all(iterable);
    m_promise->directDefineOwnProperty(state, ObjectPropertyName(strings->all),
                                       ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->all, builtinPromiseAll, 1, NativeFunctionInfo::Strict)),
                                                                (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // $25.4.4.3 Promise.race(iterable)
    m_promise->directDefineOwnProperty(state, ObjectPropertyName(strings->race),
                                       ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->race, builtinPromiseRace, 1, NativeFunctionInfo::Strict)),
                                                                (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // $25.4.4.4 Promise.reject(r)
    m_promise->directDefineOwnProperty(state, ObjectPropertyName(strings->reject),
                                       ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->reject, builtinPromiseReject, 1, NativeFunctionInfo::Strict)),
                                                                (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // $25.4.4.5 Promise.resolve(r)
    m_promise->directDefineOwnProperty(state, ObjectPropertyName(strings->resolve),
                                       ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->resolve, builtinPromiseResolve, 1, NativeFunctionInfo::Strict)),
                                                                (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // $25.4.5.1 Promise.prototype.catch(onRejected)
    m_promisePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->stringCatch),
                                                ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->stringCatch, builtinPromiseCatch, 1, NativeFunctionInfo::Strict)),
                                                                         (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // $25.4.5.3 Promise.prototype.then(onFulfilled, onRejected)
    m_promisePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->then),
                                                ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->then, builtinPromiseThen, 2, NativeFunctionInfo::Strict)),
                                                                         (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // $25.6.5.3 Promise.prototype.finally ( onFinally )
    m_promisePrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->finally),
                                                ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->finally, builtinPromiseFinally, 1, NativeFunctionInfo::Strict)),
                                                                         (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_promise->setFunctionPrototype(state, m_promisePrototype);

    // Promise.allSettled ( iterable )
    m_promise->directDefineOwnProperty(state, ObjectPropertyName(strings->allSettled),
                                       ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->allSettled, builtinPromiseAllSettled, 1, NativeFunctionInfo::Strict)),
                                                                (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // Promise.any ( iterable )
    m_promise->directDefineOwnProperty(state, ObjectPropertyName(strings->any),
                                       ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->any, builtinPromiseAny, 1, NativeFunctionInfo::Strict)),
                                                                (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));


    redefineOwnProperty(state, ObjectPropertyName(strings->Promise),
                        ObjectPropertyDescriptor(m_promise, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}
} // namespace Escargot
