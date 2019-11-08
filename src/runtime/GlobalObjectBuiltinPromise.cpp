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
#include "runtime/GlobalObject.h"
#include "runtime/Context.h"
#include "runtime/VMInstance.h"
#include "runtime/PromiseObject.h"
#include "runtime/ArrayObject.h"
#include "runtime/JobQueue.h"
#include "runtime/SandBox.h"
#include "runtime/NativeFunctionObject.h"
#include "runtime/IteratorOperations.h"

namespace Escargot {

// $25.4.3 Promise(executor)
static Value builtinPromiseConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    auto strings = &state.context()->staticStrings();
    if (!isNewExpression) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->Promise.string(), false, String::emptyString, "%s: Promise constructor should be called with new Promise()");
    }

    Value executor = argv[0];
    if (!executor.isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->Promise.string(), false, String::emptyString, "%s: Promise executor is not a function object");
    }

    PromiseObject* promise = new PromiseObject(state);
    PromiseReaction::Capability capability = promise->createResolvingFunctions(state);

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

static Value builtinPromiseAll(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // https://www.ecma-international.org/ecma-262/6.0/#sec-promise.all
    auto strings = &state.context()->staticStrings();

    // Let C be the this value.
    // If Type(C) is not Object, throw a TypeError exception.
    if (!thisValue.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->Promise.string(), false, strings->all.string(), errorMessage_GlobalObject_ThisNotObject);
    }

    Object* C = thisValue.asObject();
    Value iterableValue = argv[0];

    // Let S be Get(C, @@S).
    Value S = C->get(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().species)).value(state, C);

    // If S is neither undefined nor null, let C be S.
    if (!S.isUndefinedOrNull()) {
        C = S.toObject(state);
    }

    // Let promiseCapability be NewPromiseCapability(C).
    PromiseReaction::Capability promiseCapability = PromiseObject::newPromiseCapability(state, C);

    const Value& iterable = argv[0];

    // Let iterator be GetIterator(iterable).
    // IfAbruptRejectPromise(iterator, promiseCapability).
    Value iterator;
    try {
        iterator = getIterator(state, iterable);
    } catch (const Value& v) {
        Value thrownValue = v;
        // If value is an abrupt completion,
        // Let rejectResult be Call(capability.[[Reject]], undefined, «value.[[value]]»).
        Object::call(state, promiseCapability.m_rejectFunction, Value(), 1, &thrownValue);
        // ReturnIfAbrupt(rejectResult).
        // Return capability.[[Promise]].
        return promiseCapability.m_promise;
    }

    // Let iteratorRecord be Record {[[iterator]]: iterator, [[done]]: false}.
    const Value& iteratorRecordIterator = iterator;
    bool iteratorRecordDone = false;

    // Let result be PerformPromiseAll(iteratorRecord, C, promiseCapability).
    Value result;
    try {
        // Let values be a new empty List.
        ArrayObject* values = new ArrayObject(state);
        // Let remainingElementsCount be a new Record { [[value]]: 1 }.
        Object* remainingElementsCount = new Object(state);
        remainingElementsCount->defineOwnProperty(state, strings->value, ObjectPropertyDescriptor(Value(1), ObjectPropertyDescriptor::AllPresent));
        // Let index be 0.
        int64_t index = 0;

        // Repeat
        while (true) {
            // Let next be IteratorStep(iteratorRecord.[[iterator]]).
            Value next;
            try {
                next = iteratorStep(state, iteratorRecordIterator);
            } catch (const Value& e) {
                // If next is an abrupt completion, set iteratorRecord.[[done]] to true.
                iteratorRecordDone = true;
                // ReturnIfAbrupt(next).
                state.throwException(e);
            }
            // If next is false,
            if (next.isFalse()) {
                // set iteratorRecord.[[done]] to true.
                iteratorRecordDone = true;
                // Set remainingElementsCount.[[value]] to remainingElementsCount.[[value]] − 1.
                int64_t remainingElements = remainingElementsCount->getOwnProperty(state, strings->value).value(state, remainingElementsCount).asNumber();
                remainingElementsCount->setThrowsException(state, strings->value, Value(remainingElements - 1), remainingElementsCount);
                // If remainingElementsCount.[[value]] is 0,
                if (remainingElements == 1) {
                    // Let valuesArray be CreateArrayFromList(values).
                    // Let resolveResult be Call(resultCapability.[[Resolve]], undefined, «valuesArray»).
                    Value argv = values;
                    Value resolveResult = Object::call(state, promiseCapability.m_resolveFunction, Value(), 1, &argv);
                    // ReturnIfAbrupt(resolveResult)
                }
                // Return resultCapability.[[Promise]].
                result = promiseCapability.m_promise;
                break;
            }
            // Let nextValue be IteratorValue(next).
            Value nextValue;
            try {
                nextValue = iteratorValue(state, next);
            } catch (const Value& e) {
                // If next is an abrupt completion, set iteratorRecord.[[done]] to true.
                iteratorRecordDone = true;
                // ReturnIfAbrupt(next).
                state.throwException(e);
            }
            // Append undefined to values.
            values->defineOwnProperty(state, ObjectPropertyName(state, Value(index)), ObjectPropertyDescriptor(Value(), ObjectPropertyDescriptor::AllPresent));
            // Let nextPromise be Invoke(constructor, "resolve", «‍nextValue»).
            Value nextPromise = Object::call(state, C->get(state, ObjectPropertyName(state, strings->resolve)).value(state, C), C, 1, &nextValue);
            // ReturnIfAbrupt(nextPromise)

            // Let resolveElement be a new built-in function object as defined in Promise.all Resolve Element Functions.
            FunctionObject* resolveElement = new NativeFunctionObject(state, NativeFunctionInfo(strings->Empty, promiseAllResolveElementFunction, 1, NativeFunctionInfo::Strict));
            resolveElement->deleteOwnProperty(state, strings->name);
            Object* internalSlot = new Object(state);
            resolveElement->setInternalSlot(internalSlot);
            // Set the [[AlreadyCalled]] internal slot of resolveElement to a new Record {[[value]]: false }.
            // Set the [[Index]] internal slot of resolveElement to index.
            // Set the [[Values]] internal slot of resolveElement to values.
            // Set the [[Capabilities]] internal slot of resolveElement to resultCapability.
            // Set the [[RemainingElements]] internal slot of resolveElement to remainingElementsCount.
            Object* alreadyCalled = new Object(state);
            alreadyCalled->defineOwnProperty(state, strings->value, ObjectPropertyDescriptor(Value(false), ObjectPropertyDescriptor::AllPresent));

            internalSlot->defineOwnProperty(state, strings->alreadyCalled, ObjectPropertyDescriptor(alreadyCalled, ObjectPropertyDescriptor::AllPresent));
            internalSlot->defineOwnProperty(state, strings->index, ObjectPropertyDescriptor(Value(index), ObjectPropertyDescriptor::AllPresent));
            internalSlot->defineOwnProperty(state, strings->values, ObjectPropertyDescriptor(values, ObjectPropertyDescriptor::AllPresent));
            internalSlot->defineOwnProperty(state, strings->resolve, ObjectPropertyDescriptor(promiseCapability.m_resolveFunction, ObjectPropertyDescriptor::AllPresent));
            internalSlot->defineOwnProperty(state, strings->reject, ObjectPropertyDescriptor(promiseCapability.m_rejectFunction, ObjectPropertyDescriptor::AllPresent));
            internalSlot->defineOwnProperty(state, strings->remainingElements, ObjectPropertyDescriptor(Value(remainingElementsCount), ObjectPropertyDescriptor::AllPresent));

            // Set remainingElementsCount.[[value]] to remainingElementsCount.[[value]] + 1.
            int64_t remainingElements = remainingElementsCount->getOwnProperty(state, strings->value).value(state, remainingElementsCount).asNumber();
            remainingElementsCount->setThrowsException(state, strings->value, Value(remainingElements + 1), remainingElementsCount);
            // Let result be Invoke(nextPromise, "then", «‍resolveElement, resultCapability.[[Reject]]»).
            Object* nextPromiseObject = nextPromise.toObject(state);
            Value argv[] = { Value(resolveElement), Value(promiseCapability.m_rejectFunction) };
            result = Object::call(state, nextPromiseObject->get(state, strings->then).value(state, nextPromiseObject), nextPromiseObject, 2, argv);
            // ReturnIfAbrupt(result).
            // Set index to index + 1
            index++;
        }
    } catch (const Value& v) {
        Value exceptionValue = v;
        // If result is an abrupt completion,
        // If iteratorRecord.[[done]] is false, let result be IteratorClose(iterator, result).
        try {
            if (!iteratorRecordDone) {
                iteratorClose(state, iterator, exceptionValue, true);
            }
        } catch (const Value& v) {
            exceptionValue = v;
        }
        // IfAbruptRejectPromise(result, promiseCapability).
        // If value is an abrupt completion,
        // Let rejectResult be Call(capability.[[Reject]], undefined, «value.[[value]]»).
        Object::call(state, promiseCapability.m_rejectFunction, Value(), 1, &exceptionValue);
        // ReturnIfAbrupt(rejectResult).
    }

    // Return capability.[[Promise]].
    return promiseCapability.m_promise;
}

static Value builtinPromiseRace(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // https://www.ecma-international.org/ecma-262/6.0/#sec-promise.race
    auto strings = &state.context()->staticStrings();
    // Let C be the this value.
    // If Type(C) is not Object, throw a TypeError exception.
    if (!thisValue.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->Promise.string(), false, strings->race.string(), errorMessage_GlobalObject_ThisNotObject);
    }
    Object* C = thisValue.asObject();

    // Let S be Get(C, @@species).
    Value S = C->get(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().species)).value(state, C);
    // ReturnIfAbrupt(S).
    // If S is neither undefined nor null, let C be S.
    if (!S.isUndefinedOrNull()) {
        C = S.toObject(state);
    }

    // Let promiseCapability be NewPromiseCapability(C).
    PromiseReaction::Capability promiseCapability = PromiseObject::newPromiseCapability(state, C);
    // ReturnIfAbrupt(promiseCapability).

    // Let iterator be GetIterator(iterable).
    // IfAbruptRejectPromise(iterator, promiseCapability).
    const Value& iterable = argv[0];
    Value iterator;
    try {
        iterator = getIterator(state, iterable);
    } catch (const Value& v) {
        Value thrownValue = v;
        // If value is an abrupt completion,
        // Let rejectResult be Call(capability.[[Reject]], undefined, «value.[[value]]»).
        Object::call(state, promiseCapability.m_rejectFunction, Value(), 1, &thrownValue);
        // ReturnIfAbrupt(rejectResult).
        // Return capability.[[Promise]].
        return promiseCapability.m_promise;
    }

    // Let iteratorRecord be Record {[[iterator]]: iterator, [[done]]: false}.
    // Let result be PerformPromiseRace(iteratorRecord, promiseCapability, C).
    const Value& iteratorRecordIterator = iterator;
    bool iteratorRecordDone = false;

    try {
        // Repeat
        while (true) {
            // Let next be IteratorStep(iteratorRecord.[[iterator]]).
            Value next;
            try {
                next = iteratorStep(state, iteratorRecordIterator);
            } catch (const Value& e) {
                // If next is an abrupt completion, set iteratorRecord.[[done]] to true.
                // ReturnIfAbrupt(next).
                iteratorRecordDone = true;
                state.throwException(e);
            }
            // If next is false, then
            if (next.isFalse()) {
                // Set iteratorRecord.[[done]] to true.
                iteratorRecordDone = true;
                // Return promiseCapability.[[Promise]].
                break;
            }

            // Let nextValue be IteratorValue(next).
            Value nextValue;
            try {
                nextValue = iteratorValue(state, next);
            } catch (const Value& e) {
                // If next is an abrupt completion, set iteratorRecord.[[done]] to true.
                iteratorRecordDone = true;
                // ReturnIfAbrupt(next).
                state.throwException(e);
            }

            // Let nextPromise be Invoke(C, "resolve", «‍nextValue»).
            Value nextPromise = Object::call(state, C->get(state, strings->resolve).value(state, C), C, 1, &nextValue);
            // ReturnIfAbrupt(nextPromise).
            // Let result be Invoke(nextPromise, "then", «‍promiseCapability.[[Resolve]], promiseCapability.[[Reject]]»).
            Object* nextPromiseObject = nextPromise.toObject(state);
            Value argv[] = { Value(promiseCapability.m_resolveFunction), Value(promiseCapability.m_rejectFunction) };
            Object::call(state, nextPromiseObject->get(state, strings->then).value(state, C), nextPromise, 2, argv);
            // ReturnIfAbrupt(result).
        }
    } catch (const Value& e) {
        // If result is an abrupt completion, then
        Value exceptionValue = e;
        try {
            // If iteratorRecord.[[done]] is false, let result be IteratorClose(iterator, result).
            if (!iteratorRecordDone) {
                iteratorClose(state, iterator, exceptionValue, true);
            }
        } catch (const Value& v) {
            exceptionValue = v;
        }
        // IfAbruptRejectPromise(result, promiseCapability).
        // If value is an abrupt completion,
        // Let rejectResult be Call(capability.[[Reject]], undefined, «value.[[value]]»).
        Object::call(state, promiseCapability.m_rejectFunction, Value(), 1, &exceptionValue);
        // ReturnIfAbrupt(rejectResult).
    }
    // Return capability.[[Promise]].
    return promiseCapability.m_promise;
}

static Value builtinPromiseReject(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    auto strings = &state.context()->staticStrings();
    Object* thisObject = thisValue.toObject(state);
    PromiseReaction::Capability capability = PromiseObject::newPromiseCapability(state, thisObject);

    Value arguments[] = { argv[0] };
    Object::call(state, capability.m_rejectFunction, Value(), 1, arguments);
    return capability.m_promise;
}

// http://www.ecma-international.org/ecma-262/10.0/#sec-promise.resolve
static Value builtinPromiseResolve(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    // Let C be the this value.
    const Value& C = thisValue;
    // If Type(C) is not Object, throw a TypeError exception.
    if (!C.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, state.context()->staticStrings().Promise.string(), false, state.context()->staticStrings().resolve.string(), "%s: PromiseResolve called on non-object");
    }
    // Return ? PromiseResolve(C, x).
    return promiseResolve(state, C.asObject(), argv[0]);
}

static Value builtinPromiseCatch(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    auto strings = &state.context()->staticStrings();
    Object* thisObject = thisValue.toObject(state);
    Value onRejected = argv[0];
    Value then = thisObject->get(state, strings->then).value(state, thisObject);
    Value arguments[] = { Value(), onRejected };
    return Object::call(state, then, thisObject, 2, arguments);
}


static Value builtinPromiseThen(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    auto strings = &state.context()->staticStrings();
    if (!thisValue.isObject() || !thisValue.asObject()->isPromiseObject())
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->Promise.string(), false, strings->then.string(), "%s: not a Promise object");
    PromiseObject* promise = thisValue.asPointerValue()->asPromiseObject();
    return promise->then(state, argv[0], argv[1], promise->newPromiseResultCapability(state)).value();
}

void GlobalObject::installPromise(ExecutionState& state)
{
    const StaticStrings* strings = &state.context()->staticStrings();
    m_promise = new NativeFunctionObject(state, NativeFunctionInfo(strings->Promise, builtinPromiseConstructor, 1), NativeFunctionObject::__ForBuiltinConstructor__);
    m_promise->markThisObjectDontNeedStructureTransitionTable();
    m_promise->setPrototype(state, m_functionPrototype);

    {
        JSGetterSetter gs(
            new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().getSymbolSpecies, builtinSpeciesGetter, 0, NativeFunctionInfo::Strict)), Value(Value::EmptyValue));
        ObjectPropertyDescriptor desc(gs, ObjectPropertyDescriptor::ConfigurablePresent);
        m_promise->defineOwnProperty(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().species), desc);
    }

    m_promisePrototype = m_objectPrototype;
    m_promisePrototype = new PromiseObject(state);
    m_promisePrototype->markThisObjectDontNeedStructureTransitionTable();
    m_promisePrototype->setPrototype(state, m_objectPrototype);
    m_promisePrototype->defineOwnProperty(state, ObjectPropertyName(strings->constructor), ObjectPropertyDescriptor(m_promise, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    m_promisePrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().toStringTag),
                                                         ObjectPropertyDescriptor(Value(state.context()->staticStrings().Promise.string()), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));

    // $25.4.4.1 Promise.all(iterable);
    m_promise->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->all),
                                                ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->all, builtinPromiseAll, 1, NativeFunctionInfo::Strict)),
                                                                         (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // $25.4.4.3 Promise.race(iterable)
    m_promise->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->race),
                                                ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->race, builtinPromiseRace, 1, NativeFunctionInfo::Strict)),
                                                                         (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // $25.4.4.4 Promise.reject(r)
    m_promise->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->reject),
                                                ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->reject, builtinPromiseReject, 1, NativeFunctionInfo::Strict)),
                                                                         (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // $25.4.4.5 Promise.resolve(r)
    m_promise->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->resolve),
                                                ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->resolve, builtinPromiseResolve, 1, NativeFunctionInfo::Strict)),
                                                                         (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // $25.4.5.1 Promise.prototype.catch(onRejected)
    m_promisePrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->stringCatch),
                                                         ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->stringCatch, builtinPromiseCatch, 1, NativeFunctionInfo::Strict)),
                                                                                  (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // $25.4.5.3 Promise.prototype.then(onFulfilled, onRejected)
    m_promisePrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->then),
                                                         ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->then, builtinPromiseThen, 2, NativeFunctionInfo::Strict)),
                                                                                  (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // $25.4.5.4 Promise.prototype [ @@toStringTag ]
    m_promisePrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, Value(state.context()->vmInstance()->globalSymbols().toStringTag)),
                                                         ObjectPropertyDescriptor(Value(state.context()->staticStrings().Promise.string()), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::NonWritablePresent | ObjectPropertyDescriptor::NonEnumerablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_promise->setFunctionPrototype(state, m_promisePrototype);

    defineOwnProperty(state, ObjectPropertyName(strings->Promise),
                      ObjectPropertyDescriptor(m_promise, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}
}
