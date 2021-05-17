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
#include "PromiseObject.h"
#include "runtime/Context.h"
#include "runtime/VMInstance.h"
#include "runtime/JobQueue.h"
#include "runtime/ArrayObject.h"
#include "runtime/JobQueue.h"
#include "runtime/SandBox.h"
#include "runtime/NativeFunctionObject.h"
#include "runtime/ExtendedNativeFunctionObject.h"
#include "interpreter/ByteCodeInterpreter.h"

namespace Escargot {

// http://www.ecma-international.org/ecma-262/10.0/#sec-promise-resolve-functions
static Value promiseResolveFunctions(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget);
static Value promiseRejectFunctions(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget);

PromiseObject::PromiseObject(ExecutionState& state)
    : PromiseObject(state, state.context()->globalObject()->promisePrototype())
{
}

PromiseObject::PromiseObject(ExecutionState& state, Object* proto)
    : Object(state, proto)
    , m_state(PromiseState::Pending)
{
}

void* PromiseObject::operator new(size_t size)
{
    static bool typeInited = false;
    static GC_descr descr;
    if (!typeInited) {
        GC_word desc[GC_BITMAP_SIZE(PromiseObject)] = { 0 };
        fillGCDescriptor(desc);
        descr = GC_make_descriptor(desc, GC_WORD_LEN(PromiseObject));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

PromiseReaction::Capability PromiseObject::createResolvingFunctions(ExecutionState& state)
{
    // https://www.ecma-international.org/ecma-262/10.0/#sec-createresolvingfunctions
    const StaticStrings* strings = &state.context()->staticStrings();

    // Let alreadyResolved be a new Record { [[Value]]: false }.
    Object* alreadyResolved = new Object(state);
    alreadyResolved->defineOwnProperty(state, ObjectPropertyName(strings->value), ObjectPropertyDescriptor(Value(false), ObjectPropertyDescriptor::AllPresent));

    // Let resolve be CreateBuiltinFunction(stepsResolve, « [[Promise]], [[AlreadyResolved]] »).
    ExtendedNativeFunctionObject* resolve = new ExtendedNativeFunctionObjectImpl<2>(state, NativeFunctionInfo(AtomicString(), promiseResolveFunctions, 1, NativeFunctionInfo::Strict));
    resolve->setInternalSlot(BuiltinFunctionSlot::Promise, this);
    resolve->setInternalSlot(BuiltinFunctionSlot::AlreadyResolved, alreadyResolved);

    // Let reject be CreateBuiltinFunction(stepsReject, « [[Promise]], [[AlreadyResolved]] »).
    ExtendedNativeFunctionObject* reject = new ExtendedNativeFunctionObjectImpl<2>(state, NativeFunctionInfo(AtomicString(), promiseRejectFunctions, 1, NativeFunctionInfo::Strict));
    reject->setInternalSlot(BuiltinFunctionSlot::Promise, this);
    reject->setInternalSlot(BuiltinFunctionSlot::AlreadyResolved, alreadyResolved);

    // Return a new Record { [[Resolve]]: resolve, [[Reject]]: reject }.
    return PromiseReaction::Capability(this, resolve, reject);
}

PromiseReaction::Capability PromiseObject::newPromiseCapability(ExecutionState& state, Object* constructor)
{
    // https://www.ecma-international.org/ecma-262/10.0/#sec-newpromisecapability

    // fast path
    if (constructor == state.context()->globalObject()->promise()) {
        PromiseObject* promise = new PromiseObject(state, state.context()->globalObject()->promisePrototype());
        return promise->createResolvingFunctions(state);
    }

    const StaticStrings* strings = &state.context()->staticStrings();

    // If IsConstructor(C) is false, throw a TypeError exception.
    if (!constructor->isConstructor()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, ErrorObject::Messages::Not_Constructor);
    }

    // Let promiseCapability be a new PromiseCapability { [[Promise]]: undefined, [[Resolve]]: undefined, [[Reject]]: undefined }.
    Object* capability = new Object(state);
    capability->defineOwnProperty(state, ObjectPropertyName(strings->Promise), ObjectPropertyDescriptor(Value(), ObjectPropertyDescriptor::AllPresent));
    capability->defineOwnProperty(state, ObjectPropertyName(strings->resolve), ObjectPropertyDescriptor(Value(), ObjectPropertyDescriptor::AllPresent));
    capability->defineOwnProperty(state, ObjectPropertyName(strings->reject), ObjectPropertyDescriptor(Value(), ObjectPropertyDescriptor::AllPresent));

    // Let executor be CreateBuiltinFunction(steps, « [[Capability]] »).
    ExtendedNativeFunctionObject* executor = new ExtendedNativeFunctionObjectImpl<1>(state, NativeFunctionInfo(AtomicString(), getCapabilitiesExecutorFunction, 2, NativeFunctionInfo::Strict));
    executor->setInternalSlot(BuiltinFunctionSlot::Capability, capability);

    // Let promise be ? Construct(C, « executor »).
    Value arguments[] = { executor };
    Object* promise = Object::construct(state, constructor, 1, arguments).toObject(state);

    Value resolveFunction = capability->get(state, strings->resolve).value(state, capability);
    Value rejectFunction = capability->get(state, strings->reject).value(state, capability);

    if (!resolveFunction.isCallable() || !rejectFunction.isCallable()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Promise resolve or reject function is not callable");
    }

    return PromiseReaction::Capability(promise, resolveFunction.asObject(), rejectFunction.asObject());
}

// https://www.ecma-international.org/ecma-262/10.0/#sec-fulfillpromise
void PromiseObject::fulfill(ExecutionState& state, Value value)
{
    m_state = PromiseState::FulFilled;
    m_promiseResult = value;
    triggerPromiseReactions(state, m_fulfillReactions);

    m_fulfillReactions.clear();
    m_rejectReactions.clear();
}

// https://www.ecma-international.org/ecma-262/10.0/#sec-rejectpromise
void PromiseObject::reject(ExecutionState& state, Value reason)
{
    m_state = PromiseState::Rejected;
    m_promiseResult = reason;
    triggerPromiseReactions(state, m_rejectReactions);

    m_fulfillReactions.clear();
    m_rejectReactions.clear();
}

PromiseReaction::Capability PromiseObject::newPromiseResultCapability(ExecutionState& state)
{
    // Let C be ? SpeciesConstructor(promise, %Promise%).
    Value C = speciesConstructor(state, state.context()->globalObject()->promise());
    // Let resultCapability be ? NewPromiseCapability(C).
    return PromiseObject::newPromiseCapability(state, C.toObject(state));
}

Object* PromiseObject::then(ExecutionState& state, Value handler)
{
    return then(state, handler, Value(), newPromiseResultCapability(state)).value();
}

Object* PromiseObject::catchOperation(ExecutionState& state, Value handler)
{
    return then(state, Value(), handler, newPromiseResultCapability(state)).value();
}

Optional<Object*> PromiseObject::then(ExecutionState& state, Value onFulfilledValue, Value onRejectedValue, Optional<PromiseReaction::Capability> resultCapability)
{
    Object* onFulfilled = onFulfilledValue.isCallable() ? onFulfilledValue.asObject() : (Object*)(1);
    Object* onRejected = onRejectedValue.isCallable() ? onRejectedValue.asObject() : (Object*)(2);

    PromiseReaction::Capability capability = resultCapability.hasValue() ? resultCapability.value() : PromiseReaction::Capability(nullptr, nullptr, nullptr);

    switch (this->state()) {
    case PromiseObject::PromiseState::Pending: {
        appendReaction(onFulfilled, onRejected, capability);
        break;
    }
    case PromiseObject::PromiseState::FulFilled: {
        Job* job = new PromiseReactionJob(state.context(), PromiseReaction(onFulfilled, capability), promiseResult());
        state.context()->vmInstance()->enqueueJob(job);
        break;
    }
    case PromiseObject::PromiseState::Rejected: {
        Job* job = new PromiseReactionJob(state.context(), PromiseReaction(onRejected, capability), promiseResult());
        state.context()->vmInstance()->enqueueJob(job);
        break;
    }
    default:
        break;
    }

    if (resultCapability) {
        return capability.m_promise;
    } else {
        return nullptr;
    }
}

void PromiseObject::triggerPromiseReactions(ExecutionState& state, PromiseObject::Reactions& reactions)
{
    for (size_t i = 0; i < reactions.size(); i++) {
        state.context()->vmInstance()->enqueueJob(new PromiseReactionJob(state.context(), reactions[i], m_promiseResult));
    }
}

// http://www.ecma-international.org/ecma-262/10.0/#sec-promise-resolve
// The abstract operation PromiseResolve, given a constructor and a value, returns a new promise resolved with that value.
Object* PromiseObject::promiseResolve(ExecutionState& state, Object* C, const Value& x)
{
    // Assert: Type(C) is Object.
    // If IsPromise(x) is true, then
    if (x.isObject() && x.asObject()->isPromiseObject()) {
        // Let xConstructor be ? Get(x, "constructor").
        // If SameValue(xConstructor, C) is true, return x.
        if (x.asObject()->get(state, state.context()->staticStrings().constructor).value(state, x.asObject()) == Value(C)) {
            return x.asObject()->asPromiseObject();
        }
    }
    // Let promiseCapability be ? NewPromiseCapability(C).
    PromiseReaction::Capability capability = PromiseObject::newPromiseCapability(state, C);

    // Perform ? Call(promiseCapability.[[Resolve]], undefined, « x »).
    Value arguments[] = { x };
    Object::call(state, capability.m_resolveFunction, Value(), 1, arguments);
    // Return promiseCapability.[[Promise]].
    return capability.m_promise;
}

// https://www.ecma-international.org/ecma-262/10.0/#sec-getcapabilitiesexecutor-functions
Value PromiseObject::getCapabilitiesExecutorFunction(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    auto strings = &state.context()->staticStrings();

    // Let F be the active function object.
    ExtendedNativeFunctionObject* executor = state.resolveCallee()->asExtendedNativeFunctionObject();

    // Let promiseCapability be F.[[Capability]].
    Value capabilityValue = executor->internalSlot(PromiseObject::BuiltinFunctionSlot::Capability);
    Object* capability = capabilityValue.asObject();

    // If promiseCapability.[[Resolve]] is not undefined, throw a TypeError exception.
    // If promiseCapability.[[Reject]] is not undefined, throw a TypeError exception.
    if (!capability->getOwnProperty(state, strings->resolve).value(state, capability).isUndefined()
        || !capability->getOwnProperty(state, strings->reject).value(state, capability).isUndefined()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, "Executor function has already called");
    }

    // Set promiseCapability.[[Resolve]] to resolve.
    // Set promiseCapability.[[Reject]] to reject.
    Value resolve = argv[0];
    Value reject = argv[1];
    capability->defineOwnProperty(state, strings->resolve, ObjectPropertyDescriptor(resolve, ObjectPropertyDescriptor::AllPresent));
    capability->defineOwnProperty(state, strings->reject, ObjectPropertyDescriptor(reject, ObjectPropertyDescriptor::AllPresent));

    return Value();
}

// https://www.ecma-international.org/ecma-262/10.0/#sec-promise-resolve-functions
static Value promiseResolveFunctions(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    auto strings = &state.context()->staticStrings();
    // Let F be the active function object.
    ExtendedNativeFunctionObject* callee = state.resolveCallee()->asExtendedNativeFunctionObject();

    // Let promise be F.[[Promise]].
    Value promiseValue = callee->internalSlot(PromiseObject::BuiltinFunctionSlot::Promise);
    PromiseObject* promise = promiseValue.asObject()->asPromiseObject();

    // Let alreadyResolved be F.[[AlreadyResolved]].
    Value alreadyResolvedValue = callee->internalSlot(PromiseObject::BuiltinFunctionSlot::AlreadyResolved);
    Object* alreadyResolved = alreadyResolvedValue.asObject();

    // If alreadyResolved.[[Value]] is true, return undefined.
    if (alreadyResolved->getOwnProperty(state, strings->value).value(state, alreadyResolved).isTrue()) {
        return Value();
    }
    // Set alreadyResolved.[[Value]] to true.
    alreadyResolved->defineOwnProperty(state, ObjectPropertyName(strings->value), ObjectPropertyDescriptor(Value(true), ObjectPropertyDescriptor::AllPresent));

    Value resolutionValue = argv[0];
    if (resolutionValue == Value(promise)) {
        promise->reject(state, ErrorObject::createError(state, ErrorObject::TypeError, new ASCIIString("Self resolution error")));
        return Value();
    }

    if (!resolutionValue.isObject()) {
        promise->fulfill(state, resolutionValue);
        return Value();
    }
    Object* resolution = resolutionValue.asObject();

    SandBox sb(state.context());
    auto res = sb.run([&]() -> Value {
        return resolution->get(state, strings->then).value(state, resolution);
    });
    if (!res.error.isEmpty()) {
        promise->reject(state, res.error);
        return Value();
    }
    Value then = res.result;

    if (then.isCallable()) {
        state.context()->vmInstance()->enqueueJob(new PromiseResolveThenableJob(state.context(), promise, resolution, then.asObject()));
    } else {
        promise->fulfill(state, resolution);
    }

    return Value();
}

// https://www.ecma-international.org/ecma-262/10.0/#sec-promise-reject-functions
static Value promiseRejectFunctions(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    auto strings = &state.context()->staticStrings();
    // Let F be the active function object.
    ExtendedNativeFunctionObject* callee = state.resolveCallee()->asExtendedNativeFunctionObject();

    // Let promise be F.[[Promise]].
    Value promiseValue = callee->internalSlot(PromiseObject::BuiltinFunctionSlot::Promise);
    PromiseObject* promise = promiseValue.asObject()->asPromiseObject();

    // Let alreadyResolved be F.[[AlreadyResolved]].
    Value alreadyResolvedValue = callee->internalSlot(PromiseObject::BuiltinFunctionSlot::AlreadyResolved);
    Object* alreadyResolved = alreadyResolvedValue.asObject();

    // If alreadyResolved.[[Value]] is true, return undefined.
    if (alreadyResolved->getOwnProperty(state, strings->value).value(state, alreadyResolved).isTrue()) {
        return Value();
    }

    // Set alreadyResolved.[[Value]] to true.
    alreadyResolved->defineOwnProperty(state, ObjectPropertyName(strings->value), ObjectPropertyDescriptor(Value(true), ObjectPropertyDescriptor::AllPresent));

    // Return RejectPromise(promise, reason).
    promise->reject(state, argv[0]);
    return Value();
}

// https://www.ecma-international.org/ecma-262/10.0/#sec-promise.all-resolve-element-functions
Value PromiseObject::promiseAllResolveElementFunction(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    auto strings = &state.context()->staticStrings();
    // Let F be the active function object.
    ExtendedNativeFunctionObject* callee = state.resolveCallee()->asExtendedNativeFunctionObject();

    // Let alreadyCalled be F.[[AlreadyCalled]].
    bool* alreadyCalled = callee->internalSlotAsPointer<bool>(BuiltinFunctionSlot::AlreadyCalled);

    // If alreadyCalled.[[Value]] is true, return undefined.
    if (*alreadyCalled) {
        return Value();
    }
    // Set alreadyCalled.[[Value]] to true.
    *alreadyCalled = true;

    Value index = callee->internalSlot(BuiltinFunctionSlot::Index);
    ValueVector* values = callee->internalSlotAsPointer<ValueVector>(BuiltinFunctionSlot::Values);
    Value resolveFunction = callee->internalSlot(BuiltinFunctionSlot::Resolve);
    size_t* remainingElementsCount = callee->internalSlotAsPointer<size_t>(BuiltinFunctionSlot::RemainingElements);

    // Set values[index] to x.
    values->at(index.asUInt32()) = argv[0];

    // Set remainingElementsCount.[[Value]] to remainingElementsCount.[[Value]] - 1.
    *remainingElementsCount = *remainingElementsCount - 1;

    if (*remainingElementsCount == 0) {
        Value arguments[] = { Object::createArrayFromList(state, *values) };
        return Object::call(state, resolveFunction, Value(), 1, arguments);
    }
    return Value();
}


static Value ValueThunkHelper(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Let F be the active function object.
    Object* F = state.resolveCallee();
    // Return the resolve's member value
    return F->asExtendedNativeFunctionObject()->internalSlot(PromiseObject::BuiltinFunctionSlot::ValueOrReason);
}


static Value ValueThunkThrower(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Let F be the active function object.
    Object* F = state.resolveCallee();
    // Throw the resolve's member value
    state.throwException(F->asExtendedNativeFunctionObject()->internalSlot(PromiseObject::BuiltinFunctionSlot::ValueOrReason));
    return Value();
}


Value PromiseObject::promiseThenFinally(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // https://www.ecma-international.org/ecma-262/10.0/#sec-thenfinallyfunctions
    auto strings = &state.context()->staticStrings();
    // Let F be the active function object.
    ExtendedNativeFunctionObject* F = state.resolveCallee()->asExtendedNativeFunctionObject();
    // Let onFinally be F.[[OnFinally]].
    Value onFinally = F->internalSlot(BuiltinFunctionSlot::OnFinally);

    // Assert: IsCallable(onFinally) is true.
    // Let result be ? Call(onFinally, undefined).
    ASSERT(onFinally.isCallable());
    Value result = Object::call(state, onFinally, Value(), 0, nullptr);

    // Let C be F.[[Constructor]].
    // Assert: IsConstructor(C) is true.
    Value C = F->internalSlot(BuiltinFunctionSlot::Constructor);
    ASSERT(C.isConstructor());

    // Let promise be ? PromiseResolve(C, result).
    Value promise = PromiseObject::promiseResolve(state, C.asObject(), result);

    // Let valueThunk be equivalent to a function that returns value.
    ExtendedNativeFunctionObject* valueThunk = new ExtendedNativeFunctionObjectImpl<1>(state, NativeFunctionInfo(AtomicString(), ValueThunkHelper, 0, NativeFunctionInfo::Strict));
    valueThunk->setInternalSlot(BuiltinFunctionSlot::ValueOrReason, argv[0]);

    // Return ? Invoke(promise, "then", « valueThunk »).
    Value then = promise.asObject()->get(state, strings->then).value(state, promise);
    Value argument[1] = { Value(valueThunk) };

    return Object::call(state, then, promise, 1, argument);
}


Value PromiseObject::promiseCatchFinally(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // https://www.ecma-international.org/ecma-262/10.0/#sec-catchfinallyfunctions
    auto strings = &state.context()->staticStrings();
    // Let F be the active function object.
    ExtendedNativeFunctionObject* F = state.resolveCallee()->asExtendedNativeFunctionObject();
    // Let onFinally be F.[[OnFinally]].
    Value onFinally = F->internalSlot(BuiltinFunctionSlot::OnFinally);

    // Assert: IsCallable(onFinally) is true.
    // Let result be ? Call(onFinally, undefined).
    ASSERT(onFinally.isCallable());
    Value result = Object::call(state, onFinally, Value(), 0, nullptr);

    // Let C be F.[[Constructor]].
    // Assert: IsConstructor(C) is true.
    Value C = F->internalSlot(BuiltinFunctionSlot::Constructor);
    ASSERT(C.isConstructor());

    // Let promise be ? PromiseResolve(C, result).
    Value promise = PromiseObject::promiseResolve(state, C.asObject(), result);

    // Let thrower be equivalent to a function that throws reason.
    ExtendedNativeFunctionObject* valueThunk = new ExtendedNativeFunctionObjectImpl<1>(state, NativeFunctionInfo(AtomicString(), ValueThunkThrower, 0, NativeFunctionInfo::Strict));
    valueThunk->setInternalSlot(BuiltinFunctionSlot::ValueOrReason, argv[0]);

    // Return ? Invoke(promise, "then", « thrower »).
    Value then = promise.asObject()->get(state, strings->then).value(state, promise);
    Value argument[1] = { Value(valueThunk) };

    return Object::call(state, then, promise, 1, argument);
}

// https://tc39.es/ecma262/#sec-promise.allsettled-resolve-element-functions
Value PromiseObject::promiseAllSettledResolveElementFunction(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Let F be the active function object.
    ExtendedNativeFunctionObject* F = state.resolveCallee()->asExtendedNativeFunctionObject();
    // Let alreadyCalled be F.[[AlreadyCalled]].
    bool* alreadyCalled = F->internalSlotAsPointer<bool>(BuiltinFunctionSlot::AlreadyCalled);
    // If alreadyCalled.[[Value]] is true, return undefined.
    if (*alreadyCalled) {
        return Value();
    }
    // Set alreadyCalled.[[Value]] to true.
    *alreadyCalled = true;
    // Let index be F.[[Index]].
    uint32_t index = Value(F->internalSlot(BuiltinFunctionSlot::Index)).asUInt32();
    // Let values be F.[[Values]].
    ValueVector* values = F->internalSlotAsPointer<ValueVector>(BuiltinFunctionSlot::Values);
    // Let promiseCapability be F.[[Capability]].
    // Let remainingElementsCount be F.[[RemainingElements]].
    size_t* remainingElementsCount = F->internalSlotAsPointer<size_t>(BuiltinFunctionSlot::RemainingElements);
    // Let obj be ! ObjectCreate(%ObjectPrototype%).
    Object* obj = new Object(state);
    // Perform ! CreateDataProperty(obj, "status", "fulfilled").
    obj->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().lazyStatus()), ObjectPropertyDescriptor(state.context()->staticStrings().lazyFulfilled().string(), ObjectPropertyDescriptor::AllPresent));
    // Perform ! CreateDataProperty(obj, "value", x).
    obj->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().value), ObjectPropertyDescriptor(argv[0], ObjectPropertyDescriptor::AllPresent));
    // Set values[index] to be obj.
    values->at(index) = obj;
    // Set remainingElementsCount.[[Value]] to remainingElementsCount.[[Value]] - 1.
    *remainingElementsCount = *remainingElementsCount - 1;

    // If remainingElementsCount.[[Value]] is 0, then
    if (*remainingElementsCount == 0) {
        // Let valuesArray be ! CreateArrayFromList(values).
        Value valuesArray = ArrayObject::createArrayFromList(state, *values);
        // Return ? Call(promiseCapability.[[Resolve]], undefined, « valuesArray »).
        return Object::call(state, F->internalSlot(BuiltinFunctionSlot::Resolve), Value(), 1, &valuesArray);
    }

    return Value();
}

// https://tc39.es/ecma262/#sec-promise.allsettled-resolve-element-functions
Value PromiseObject::promiseAllSettledRejectElementFunction(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Let F be the active function object.
    ExtendedNativeFunctionObject* F = state.resolveCallee()->asExtendedNativeFunctionObject();
    // Let alreadyCalled be F.[[AlreadyCalled]].
    bool* alreadyCalled = F->internalSlotAsPointer<bool>(BuiltinFunctionSlot::AlreadyCalled);
    // If alreadyCalled.[[Value]] is true, return undefined.
    if (*alreadyCalled) {
        return Value();
    }
    // Set alreadyCalled.[[Value]] to true.
    *alreadyCalled = true;
    // Let index be F.[[Index]].
    uint32_t index = Value(F->internalSlot(BuiltinFunctionSlot::Index)).asUInt32();
    // Let values be F.[[Values]].
    ValueVector* values = F->internalSlotAsPointer<ValueVector>(BuiltinFunctionSlot::Values);
    // Let promiseCapability be F.[[Capability]].
    // Let remainingElementsCount be F.[[RemainingElements]].
    size_t* remainingElementsCount = F->internalSlotAsPointer<size_t>(BuiltinFunctionSlot::RemainingElements);
    // Let obj be ! OrdinaryObjectCreate(%Object.prototype%).
    Object* obj = new Object(state);
    // Perform ! CreateDataPropertyOrThrow(obj, "status", "rejected").
    obj->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().lazyStatus()), ObjectPropertyDescriptor(state.context()->staticStrings().lazyRejected().string(), ObjectPropertyDescriptor::AllPresent));
    // Perform ! CreateDataPropertyOrThrow(obj, "reason", x).
    obj->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().lazyReason()), ObjectPropertyDescriptor(argv[0], ObjectPropertyDescriptor::AllPresent));
    // Set values[index] to obj.
    values->at(index) = obj;
    // Set remainingElementsCount.[[Value]] to remainingElementsCount.[[Value]] - 1.
    *remainingElementsCount = *remainingElementsCount - 1;
    // If remainingElementsCount.[[Value]] is 0, then
    if (*remainingElementsCount == 0) {
        // Let valuesArray be ! CreateArrayFromList(values).
        Value valuesArray = ArrayObject::createArrayFromList(state, *values);
        // Return ? Call(promiseCapability.[[Resolve]], undefined, « valuesArray »).
        return Object::call(state, F->internalSlot(BuiltinFunctionSlot::Resolve), Value(), 1, &valuesArray);
    }
    // Return undefined.
    return Value();
}

// https://tc39.es/ecma262/#sec-promise.any-reject-element-functions
Value PromiseObject::promiseAnyRejectElementFunction(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Let F be the active function object.
    ExtendedNativeFunctionObject* F = state.resolveCallee()->asExtendedNativeFunctionObject();
    // If F.[[AlreadyCalled]] is true, return undefined.
    bool* alreadyCalled = F->internalSlotAsPointer<bool>(BuiltinFunctionSlot::AlreadyCalled);
    // If alreadyCalled.[[Value]] is true, return undefined.
    if (*alreadyCalled) {
        return Value();
    }
    // Set F.[[AlreadyCalled]] to true.
    *alreadyCalled = true;
    // Let index be F.[[Index]].
    uint32_t index = Value(F->internalSlot(BuiltinFunctionSlot::Index)).asUInt32();
    // Let errors be F.[[Errors]].
    ValueVector* errors = F->internalSlotAsPointer<ValueVector>(BuiltinFunctionSlot::Values);
    // Let promiseCapability be F.[[Capability]].
    // Let remainingElementsCount be F.[[RemainingElements]].
    size_t* remainingElementsCount = F->internalSlotAsPointer<size_t>(BuiltinFunctionSlot::RemainingElements);
    // Set errors[index] to x.
    errors->at(index) = argv[0];
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
        // Return ? Call(promiseCapability.[[Reject]], undefined, « error »).
        Value argv = error;
        return Object::call(state, F->internalSlot(BuiltinFunctionSlot::Reject), Value(), 1, &argv);
    }
    // Return undefined.
    return Value();
}
} // namespace Escargot
