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

#if ESCARGOT_ENABLE_PROMISE

#include "Escargot.h"
#include "GlobalObject.h"
#include "Context.h"
#include "PromiseObject.h"
#include "ArrayObject.h"
#include "JobQueue.h"
#include "SandBox.h"

namespace Escargot {

static SandBox::SandBoxResult tryCallMethodAndCatchError(ExecutionState& state, ObjectPropertyName name, Value receiver, Value arguments[], const size_t& argumentCount, bool isNewExpression)
{
    SandBox sb(state.context());
    return sb.run([&]() -> Value {
        Value callee = receiver.toObject(state)->get(state, name).value(state, receiver);
        return FunctionObject::call(state, callee, receiver, argumentCount, arguments, isNewExpression);
    });
}

// $25.4.3 Promise(executor)
static Value builtinPromiseConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    auto strings = &state.context()->staticStrings();
    PromiseObject* promise;
    Value executor;
    if (isNewExpression) {
        executor = argv[0];
        if (!executor.isFunction())
            ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->Promise.string(), false, String::emptyString, "%s: Promise executor is not a function object");
        promise = thisValue.toObject(state)->asPromiseObject();
    } else {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->Promise.string(), false, String::emptyString, "%s: Promise constructor should be called with new Promise()");
        RELEASE_ASSERT_NOT_REACHED();
    }

    PromiseReaction::Capability capability = promise->createResolvingFunctions(state);

    SandBox sb(state.context());
    auto res = sb.run([&]() -> Value {
        Value arguments[] = { capability.m_resolveFunction, capability.m_rejectFunction };
        FunctionObject::call(state, executor, Value(), 2, arguments, false);
        return Value();
    });
    if (!res.error.isEmpty()) {
        Value arguments[] = { res.error };
        return FunctionObject::call(state, capability.m_rejectFunction, Value(), 1, arguments, false);
    }
    return promise;
}

static Value builtinPromiseAll(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    auto strings = &state.context()->staticStrings();

    Object* thisObject = thisValue.toObject(state);
    Value iterableValue = argv[0];

    PromiseReaction::Capability capability = PromiseObject::newPromiseCapability(state, thisObject);

    if (!iterableValue.isIterable()) {
        Value arguments[] = { new TypeErrorObject(state, String::emptyString) };
        FunctionObject::call(state, capability.m_rejectFunction, Value(), 1, arguments, false);
    } else {
        Object* iterable = iterableValue.toObject(state);
        bool done = false;

        // 25.4.4.1.1 Runtime Semantics: PerformPromiseAll (iteratorRecord, constructor, resultCapability)

        ArrayObject* values = new ArrayObject(state);
        Object* remainingElementsCount = new Object(state);
        remainingElementsCount->defineOwnProperty(state, strings->value, ObjectPropertyDescriptor(Value(1), ObjectPropertyDescriptor::AllPresent));
        uint32_t index = 0;

        ASSERT(iterable->isArrayObject()); // TODO
        ArrayObject* iterableArray = iterable->asArrayObject();

        // 6. Repeat
        uint32_t len = iterableArray->length(state);
        uint32_t k = 0;
        Value error(Value::EmptyValue);
        while (k < len) {
            bool kPresent = iterableArray->hasProperty(state, ObjectPropertyName(state, Value(k)));
            if (kPresent) {
                // 6.e. Let nextValue be IteratorValue(next).
                Value nextValue = iterableArray->getOwnProperty(state, ObjectPropertyName(state, Value(k))).value(state, iterableArray);

                // 6.h. Append undefined to values
                values->defineOwnProperty(state, ObjectPropertyName(state, Value(index)), ObjectPropertyDescriptor(Value(), ObjectPropertyDescriptor::AllPresent));

                // 6.i Let nextPromise be Invoke(constructor, "resolve", «nextValue»).
                Value argumentsI[] = { nextValue };
                auto res = tryCallMethodAndCatchError(state, strings->resolve, thisObject, argumentsI, 1, false);
                if (!res.error.isEmpty()) {
                    error = res.error;
                    break;
                }
                Value nextPromise = res.result;

                // 6.k
                FunctionObject* resolveElement = new FunctionObject(state, NativeFunctionInfo(strings->Empty, promiseAllResolveElementFunction, 1, nullptr, NativeFunctionInfo::Strict));
                resolveElement->deleteOwnProperty(state, strings->name);
                Object* internalSlot = new Object(state);
                resolveElement->setInternalSlot(internalSlot);

                // 6.l ~ 6.p
                Object* alreadyCalled = new Object(state);
                alreadyCalled->defineOwnProperty(state, strings->value, ObjectPropertyDescriptor(Value(false), ObjectPropertyDescriptor::AllPresent));

                internalSlot->defineOwnProperty(state, strings->alreadyCalled, ObjectPropertyDescriptor(alreadyCalled, ObjectPropertyDescriptor::AllPresent));
                internalSlot->defineOwnProperty(state, strings->index, ObjectPropertyDescriptor(Value(index), ObjectPropertyDescriptor::AllPresent));
                internalSlot->defineOwnProperty(state, strings->values, ObjectPropertyDescriptor(values, ObjectPropertyDescriptor::AllPresent));
                internalSlot->defineOwnProperty(state, strings->resolve, ObjectPropertyDescriptor(capability.m_resolveFunction, ObjectPropertyDescriptor::AllPresent));
                internalSlot->defineOwnProperty(state, strings->reject, ObjectPropertyDescriptor(capability.m_rejectFunction, ObjectPropertyDescriptor::AllPresent));
                internalSlot->defineOwnProperty(state, strings->remainingElements, ObjectPropertyDescriptor(Value(remainingElementsCount), ObjectPropertyDescriptor::AllPresent));

                // 6.q
                uint32_t remainingElements = remainingElementsCount->getOwnProperty(state, strings->value).value(state, remainingElementsCount).asUInt32();
                remainingElementsCount->setThrowsException(state, strings->value, Value(remainingElements + 1), remainingElementsCount);

                // 6.r
                Value argumentsR[] = { resolveElement, capability.m_rejectFunction };

                res = tryCallMethodAndCatchError(state, strings->then, nextPromise, argumentsR, 2, false);
                if (!res.error.isEmpty()) {
                    error = res.error;
                    break;
                }

                // 6.t
                index++;
                k++;
            } else {
                k = Object::nextIndexForward(state, iterableArray, k, len, true);
            }
        }

        // 6.d. If next is false
        if (error.isEmpty()) {
            done = true;
            uint32_t remainingElements = remainingElementsCount->getOwnProperty(state, strings->value).value(state, remainingElementsCount).asUInt32();
            remainingElementsCount->setThrowsException(state, strings->value, Value(remainingElements - 1), remainingElementsCount);
            if (remainingElements == 1) {
                SandBox sb(state.context());
                auto res = sb.run([&]() -> Value {
                    Value arguments[] = { values };
                    FunctionObject::call(state, capability.m_resolveFunction, Value(), 1, arguments, false);
                    return Value();
                });
                if (!res.error.isEmpty()) {
                    error = res.error;
                }
            }
        }

        // If result is an abrupt completion && If iteratorRecord.[[done]] is false
        if (!error.isEmpty() && !done) {
            Value arguments[] = { error };
            FunctionObject::call(state, capability.m_rejectFunction, Value(), 1, arguments, false);
        }
    }
    return capability.m_promise;
}

static Value builtinPromiseRace(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    auto strings = &state.context()->staticStrings();
    Object* thisObject = thisValue.toObject(state);
    Value iterableValue = argv[0];

    PromiseReaction::Capability capability = PromiseObject::newPromiseCapability(state, thisObject);

    if (!iterableValue.isIterable()) {
        Value arguments[] = { new TypeErrorObject(state, String::emptyString) };
        FunctionObject::call(state, capability.m_rejectFunction, Value(), 1, arguments, false);
    } else {
        Object* iterable = iterableValue.toObject(state);
        bool done = false;

        // 25.4.4.1.1 Runtime Semantics: PerformPromiseRace (iteratorRecord, promiseCapability, C)

        ArrayObject* values = new ArrayObject(state);
        uint32_t index = 0;

        ASSERT(iterable->isArrayObject()); // TODO
        ArrayObject* iterableArray = iterable->asArrayObject();

        // 6. Repeat
        uint32_t len = iterableArray->length(state);
        uint32_t k = 0;
        Value error(Value::EmptyValue);
        while (k < len) {
            bool kPresent = iterableArray->hasProperty(state, ObjectPropertyName(state, Value(k)));
            if (kPresent) {
                Value nextValue = iterableArray->getOwnProperty(state, ObjectPropertyName(state, Value(k))).value(state, iterableArray);

                Value argumentsH[] = { nextValue };
                auto res = tryCallMethodAndCatchError(state, strings->resolve, thisObject, argumentsH, 1, false);
                if (!res.error.isEmpty()) {
                    error = res.error;
                    break;
                }
                Value nextPromise = res.result;

                Value argumentsJ[] = { capability.m_resolveFunction, capability.m_rejectFunction };
                res = tryCallMethodAndCatchError(state, strings->then, nextPromise, argumentsJ, 2, false);
                if (!res.error.isEmpty()) {
                    error = res.error;
                    break;
                }
                Value result = res.result;

                k++;
            } else {
                k = Object::nextIndexForward(state, iterableArray, k, len, true);
            }
        }

        // 1.d. If next is false
        if (error.isEmpty())
            done = true;

        // If result is an abrupt completion && If iteratorRecord.[[done]] is false
        if (!error.isEmpty() && !done) {
            Value arguments[] = { error };
            FunctionObject::call(state, capability.m_rejectFunction, Value(), 1, arguments, false);
        }
    }
    return capability.m_promise;
}

static Value builtinPromiseReject(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    auto strings = &state.context()->staticStrings();
    Object* thisObject = thisValue.toObject(state);
    PromiseReaction::Capability capability = PromiseObject::newPromiseCapability(state, thisObject);

    Value arguments[] = { argv[0] };
    FunctionObject::call(state, capability.m_rejectFunction, Value(), 1, arguments, false);
    return capability.m_promise;
}

static Value builtinPromiseResolve(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    auto strings = &state.context()->staticStrings();
    Object* thisObject = thisValue.toObject(state);
    Value x = argv[0];
    if (x.isObject() && x.asObject()->isPromiseObject()) {
        if (x.asObject()->get(state, strings->constructor).value(state, x.asObject()) == Value(thisObject))
            return x;
    }
    PromiseReaction::Capability capability = PromiseObject::newPromiseCapability(state, thisObject);

    Value arguments[] = { x };
    FunctionObject::call(state, capability.m_resolveFunction, Value(), 1, arguments, false);
    return capability.m_promise;
}

static Value builtinPromiseCatch(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    auto strings = &state.context()->staticStrings();
    Object* thisObject = thisValue.toObject(state);
    Value onRejected = argv[0];
    Value then = thisObject->get(state, strings->then).value(state, thisObject);
    Value arguments[] = { Value(), onRejected };
    return FunctionObject::call(state, then, thisObject, 2, arguments, false);
}


static Value builtinPromiseThen(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    auto strings = &state.context()->staticStrings();
    if (!thisValue.isObject() || !thisValue.asObject()->isPromiseObject())
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->Promise.string(), false, strings->then.string(), "%s: not a Promise object");
    PromiseObject* promise = thisValue.asPointerValue()->asPromiseObject();

    Value onFulfilledValue = argv[0];
    Value onRejectedValue = argv[1];

    FunctionObject* onFulfilled = onFulfilledValue.isFunction() ? onFulfilledValue.asFunction() : (FunctionObject*)(1);
    FunctionObject* onRejected = onRejectedValue.isFunction() ? onRejectedValue.asFunction() : (FunctionObject*)(2);

    PromiseReaction::Capability capability = PromiseObject::newPromiseCapability(state, promise->get(state, strings->constructor).value(state, promise).toObject(state));

    switch (promise->state()) {
    case PromiseObject::PromiseState::Pending: {
        promise->appendReaction(onFulfilled, onRejected, capability);
        break;
    }
    case PromiseObject::PromiseState::FulFilled: {
        Job* job = new PromiseReactionJob(PromiseReaction(onFulfilled, capability), promise->promiseResult());
        state.context()->jobQueue()->enqueueJob(job);
        break;
    }
    case PromiseObject::PromiseState::Rejected: {
        Job* job = new PromiseReactionJob(PromiseReaction(onRejected, capability), promise->promiseResult());
        state.context()->jobQueue()->enqueueJob(job);
        break;
    }
    default:
        break;
    }
    return capability.m_promise;
}

// $25.4.1.5.1 Internal GetCapabilitiesExecutor Function
Value getCapabilitiesExecutorFunction(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    auto strings = &state.context()->staticStrings();
    FunctionObject* executor = state.executionContext()->resolveCallee();
    executor->deleteOwnProperty(state, strings->name);
    Object* executorInternalSlot = executor->ensureInternalSlot(state);
    if (!executorInternalSlot->getOwnProperty(state, strings->resolve).value(state, executorInternalSlot).isUndefined()
        || !executorInternalSlot->getOwnProperty(state, strings->reject).value(state, executorInternalSlot).isUndefined())
        state.throwException(new TypeErrorObject(state, new ASCIIString("Executor function has already called")));

    Value resolve = argv[0];
    Value reject = argv[1];

    executorInternalSlot->defineOwnProperty(state, strings->resolve, ObjectPropertyDescriptor(resolve, ObjectPropertyDescriptor::AllPresent));
    executorInternalSlot->defineOwnProperty(state, strings->reject, ObjectPropertyDescriptor(reject, ObjectPropertyDescriptor::AllPresent));

    return Value();
}

// $25.4.1.3.2 Internal Promise Resolve Function
Value promiseResolveFunction(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    auto strings = &state.context()->staticStrings();
    FunctionObject* callee = state.executionContext()->resolveCallee();
    Object* alreadyResolved = PromiseObject::resolvingFunctionAlreadyResolved(state, callee);
    Object* internalSlot = callee->internalSlot();
    PromiseObject* promise = internalSlot->getOwnProperty(state, strings->Promise).value(state, internalSlot).asObject()->asPromiseObject();
    if (alreadyResolved->getOwnProperty(state, strings->value).value(state, alreadyResolved).asBoolean())
        return Value();
    alreadyResolved->setThrowsException(state, strings->value, Value(true), alreadyResolved);

    Value resolutionValue = argv[0];
    if (resolutionValue == Value(promise)) {
        promise->rejectPromise(state, new TypeErrorObject(state, new ASCIIString("Self resolution error")));
        return Value();
    }

    if (!resolutionValue.isObject()) {
        promise->fulfillPromise(state, resolutionValue);
        return Value();
    }
    Object* resolution = resolutionValue.asObject();

    SandBox sb(state.context());
    auto res = sb.run([&]() -> Value {
        return resolution->get(state, strings->then).value(state, resolution);
    });
    if (!res.error.isEmpty()) {
        promise->rejectPromise(state, res.error);
        return Value();
    }
    Value then = res.result;

    if (then.isFunction()) {
        state.context()->jobQueue()->enqueueJob(new PromiseResolveThenableJob(promise, resolution, then.asFunction()));
    } else {
        promise->fulfillPromise(state, resolution);
        return Value();
    }

    return Value();
}

// $25.4.1.3.1 Internal Promise Reject Function
Value promiseRejectFunction(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    auto strings = &state.context()->staticStrings();
    FunctionObject* callee = state.executionContext()->resolveCallee();
    Object* alreadyResolved = PromiseObject::resolvingFunctionAlreadyResolved(state, callee);
    Object* internalSlot = callee->internalSlot();
    PromiseObject* promise = internalSlot->getOwnProperty(state, strings->Promise).value(state, internalSlot).asObject()->asPromiseObject();
    if (alreadyResolved->getOwnProperty(state, strings->value).value(state, alreadyResolved).asBoolean())
        return Value();
    alreadyResolved->setThrowsException(state, strings->value, Value(true), alreadyResolved);

    promise->rejectPromise(state, argv[0]);
    return Value();
}

// $25.4.4.1.2 Internal Promise.all Resolve Element Function
Value promiseAllResolveElementFunction(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    auto strings = &state.context()->staticStrings();
    FunctionObject* callee = state.executionContext()->resolveCallee();
    Value x = argv[0];
    Object* internalSlot = callee->internalSlot();

    Object* alreadyCalled = internalSlot->getOwnProperty(state, strings->alreadyCalled).value(state, internalSlot).asObject();
    if (alreadyCalled->getOwnProperty(state, strings->value).value(state, alreadyCalled).asBoolean())
        return Value();
    alreadyCalled->setThrowsException(state, strings->value, Value(true), alreadyCalled);

    uint32_t index = internalSlot->getOwnProperty(state, strings->index).value(state, internalSlot).asUInt32();
    ArrayObject* values = internalSlot->getOwnProperty(state, strings->values).value(state, internalSlot).asObject()->asArrayObject();
    FunctionObject* resolveFunction = internalSlot->getOwnProperty(state, strings->resolve).value(state, internalSlot).asFunction();
    Object* remainingElementsCount = internalSlot->getOwnProperty(state, strings->remainingElements).value(state, internalSlot).asObject();

    values->setThrowsException(state, ObjectPropertyName(state, Value(index)), x, values);
    uint32_t remainingElements = remainingElementsCount->getOwnProperty(state, strings->value).value(state, remainingElementsCount).asUInt32();
    remainingElementsCount->setThrowsException(state, strings->value, Value(remainingElements - 1), remainingElementsCount);
    if (remainingElements == 1) {
        Value arguments[] = { values };
        FunctionObject::call(state, resolveFunction, Value(), 1, arguments, false);
    }
    return Value();
}

static Value builtinPromiseToString(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    return Value(state.context()->staticStrings().Promise.string());
}

void GlobalObject::installPromise(ExecutionState& state)
{
    const StaticStrings* strings = &state.context()->staticStrings();
    m_promise = new FunctionObject(state, NativeFunctionInfo(strings->Promise, builtinPromiseConstructor, 1, [](ExecutionState& state, size_t argc, Value* argv) -> Object* {
                                       return new PromiseObject(state);
                                   }),
                                   FunctionObject::__ForBuiltin__);
    m_promise->markThisObjectDontNeedStructureTransitionTable(state);
    m_promise->setPrototype(state, m_functionPrototype);
    m_promisePrototype = m_objectPrototype;
    m_promisePrototype = new PromiseObject(state);
    m_promisePrototype->markThisObjectDontNeedStructureTransitionTable(state);
    m_promisePrototype->setPrototype(state, m_objectPrototype);
    m_promisePrototype->defineOwnProperty(state, ObjectPropertyName(strings->constructor), ObjectPropertyDescriptor(m_promise, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // $25.4.4.1 Promise.all(iterable);
    m_promise->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->all),
                                                ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->all, builtinPromiseAll, 1, nullptr, NativeFunctionInfo::Strict)),
                                                                         (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // $25.4.4.3 Promise.race(iterable)
    m_promise->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->race),
                                                ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->race, builtinPromiseRace, 1, nullptr, NativeFunctionInfo::Strict)),
                                                                         (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // $25.4.4.4 Promise.reject(r)
    m_promise->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->reject),
                                                ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->reject, builtinPromiseReject, 1, nullptr, NativeFunctionInfo::Strict)),
                                                                         (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // $25.4.4.5 Promise.resolve(r)
    m_promise->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->resolve),
                                                ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->resolve, builtinPromiseResolve, 1, nullptr, NativeFunctionInfo::Strict)),
                                                                         (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    // $25.4.5.1 Promise.prototype.catch(onRejected)
    m_promisePrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->stringCatch),
                                                         ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->stringCatch, builtinPromiseCatch, 1, nullptr, NativeFunctionInfo::Strict)),
                                                                                  (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // $25.4.5.3 Promise.prototype.then(onFulfilled, onRejected)
    m_promisePrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->then),
                                                         ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->then, builtinPromiseThen, 2, nullptr, NativeFunctionInfo::Strict)),
                                                                                  (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    // $25.4.5.4 Promise.prototype.toString
    m_promisePrototype->defineOwnPropertyThrowsException(state, ObjectPropertyName(strings->toString),
                                                         ObjectPropertyDescriptor(new FunctionObject(state, NativeFunctionInfo(strings->toString, builtinPromiseToString, 1, nullptr, NativeFunctionInfo::Strict)),
                                                                                  (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_promise->setFunctionPrototype(state, m_promisePrototype);

    defineOwnProperty(state, ObjectPropertyName(strings->Promise),
                      ObjectPropertyDescriptor(m_promise, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}
}

#endif // ESCARGOT_ENABLE_PROMISE
