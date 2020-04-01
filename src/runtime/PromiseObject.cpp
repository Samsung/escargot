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
#include "runtime/NativeFunctionObject.h"
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
        GC_word obj_bitmap[GC_BITMAP_SIZE(PromiseObject)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(PromiseObject, m_structure));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(PromiseObject, m_prototype));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(PromiseObject, m_values));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(PromiseObject, m_promiseResult));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(PromiseObject, m_fulfillReactions));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(PromiseObject, m_rejectReactions));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(PromiseObject));
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
    const StaticStrings* strings = &state.context()->staticStrings();

    // If IsConstructor(C) is false, throw a TypeError exception.
    if (!constructor->isConstructor()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, errorMessage_Not_Constructor);
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
    Object* promise = Object::construct(state, constructor, 1, arguments);

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
        state.context()->vmInstance()->enqueuePromiseJob(this, job);
        break;
    }
    case PromiseObject::PromiseState::Rejected: {
        Job* job = new PromiseReactionJob(state.context(), PromiseReaction(onRejected, capability), promiseResult());
        state.context()->vmInstance()->enqueuePromiseJob(this, job);
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
        state.context()->vmInstance()->enqueuePromiseJob(this, new PromiseReactionJob(state.context(), reactions[i], m_promiseResult));
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
    Value capabilityValue = executor->getInternalSlot(PromiseObject::BuiltinFunctionSlot::Capability);
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
    Value promiseValue = callee->getInternalSlot(PromiseObject::BuiltinFunctionSlot::Promise);
    PromiseObject* promise = promiseValue.asObject()->asPromiseObject();

    // Let alreadyResolved be F.[[AlreadyResolved]].
    Value alreadyResolvedValue = callee->getInternalSlot(PromiseObject::BuiltinFunctionSlot::AlreadyResolved);
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
        state.context()->vmInstance()->enqueuePromiseJob(promise, new PromiseResolveThenableJob(state.context(), promise, resolution, then.asObject()));
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
    Value promiseValue = callee->getInternalSlot(PromiseObject::BuiltinFunctionSlot::Promise);
    PromiseObject* promise = promiseValue.asObject()->asPromiseObject();

    // Let alreadyResolved be F.[[AlreadyResolved]].
    Value alreadyResolvedValue = callee->getInternalSlot(PromiseObject::BuiltinFunctionSlot::AlreadyResolved);
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
    Value alreadyCalled = callee->getInternalSlot(BuiltinFunctionSlot::AlreadyCalled);

    // If alreadyCalled.[[Value]] is true, return undefined.
    if (alreadyCalled.asObject()->getOwnProperty(state, strings->value).value(state, alreadyCalled).asBoolean()) {
        return Value();
    }
    // Set alreadyCalled.[[Value]] to true.
    alreadyCalled.asObject()->setThrowsException(state, strings->value, Value(true), alreadyCalled);

    Value index = callee->getInternalSlot(BuiltinFunctionSlot::Index);
    Value values = callee->getInternalSlot(BuiltinFunctionSlot::Values);
    Value resolveFunction = callee->getInternalSlot(BuiltinFunctionSlot::Resolve);
    Value remainingElementsCount = callee->getInternalSlot(BuiltinFunctionSlot::RemainingElements);

    // Set values[index] to x.
    values.asObject()->setThrowsException(state, ObjectPropertyName(state, index.asUInt32()), argv[0], values);

    // Set remainingElementsCount.[[Value]] to remainingElementsCount.[[Value]] - 1.
    uint32_t elementsCount = remainingElementsCount.asObject()->getOwnProperty(state, strings->value).value(state, remainingElementsCount).asUInt32();
    remainingElementsCount.asObject()->setThrowsException(state, strings->value, Value(elementsCount - 1), remainingElementsCount);

    if (elementsCount == 1) {
        Value arguments[] = { values };
        return Object::call(state, resolveFunction, Value(), 1, arguments);
    }
    return Value();
}


static Value ValueThunkHelper(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Let F be the active function object.
    Object* F = state.resolveCallee();
    // Return the resolve's member value
    return F->asExtendedNativeFunctionObject()->getInternalSlot(PromiseObject::BuiltinFunctionSlot::ValueOrReason);
}


static Value ValueThunkThrower(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Let F be the active function object.
    Object* F = state.resolveCallee();
    // Throw the resolve's member value
    state.throwException(F->asExtendedNativeFunctionObject()->getInternalSlot(PromiseObject::BuiltinFunctionSlot::ValueOrReason));
    return Value();
}


Value PromiseObject::promiseThenFinally(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // https://www.ecma-international.org/ecma-262/10.0/#sec-thenfinallyfunctions
    auto strings = &state.context()->staticStrings();
    // Let F be the active function object.
    ExtendedNativeFunctionObject* F = state.resolveCallee()->asExtendedNativeFunctionObject();
    // Let onFinally be F.[[OnFinally]].
    Value onFinally = F->getInternalSlot(BuiltinFunctionSlot::OnFinally);

    // Assert: IsCallable(onFinally) is true.
    // Let result be ? Call(onFinally, undefined).
    ASSERT(onFinally.isCallable());
    Value result = Object::call(state, onFinally, Value(), 0, nullptr);

    // Let C be F.[[Constructor]].
    // Assert: IsConstructor(C) is true.
    Value C = F->getInternalSlot(BuiltinFunctionSlot::Constructor);
    ASSERT(C.isConstructor());

    // Let promise be ? PromiseResolve(C, result).
    Value promise = PromiseObject::promiseResolve(state, C.asObject(), result);

    // Let valueThunk be equivalent to a function that returns value.
    ExtendedNativeFunctionObject* valueThunk = new ExtendedNativeFunctionObjectImpl<1>(state, NativeFunctionInfo(AtomicString(), ValueThunkHelper, 1, NativeFunctionInfo::Strict));
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
    Value onFinally = F->getInternalSlot(BuiltinFunctionSlot::OnFinally);

    // Assert: IsCallable(onFinally) is true.
    // Let result be ? Call(onFinally, undefined).
    ASSERT(onFinally.isCallable());
    Value result = Object::call(state, onFinally, Value(), 0, nullptr);

    // Let C be F.[[Constructor]].
    // Assert: IsConstructor(C) is true.
    Value C = F->getInternalSlot(BuiltinFunctionSlot::Constructor);
    ASSERT(C.isConstructor());

    // Let promise be ? PromiseResolve(C, result).
    Value promise = PromiseObject::promiseResolve(state, C.asObject(), result);

    // Let thrower be equivalent to a function that throws reason.
    ExtendedNativeFunctionObject* valueThunk = new ExtendedNativeFunctionObjectImpl<1>(state, NativeFunctionInfo(AtomicString(), ValueThunkThrower, 1, NativeFunctionInfo::Strict));
    valueThunk->setInternalSlot(BuiltinFunctionSlot::ValueOrReason, argv[0]);

    // Return ? Invoke(promise, "then", « thrower »).
    Value then = promise.asObject()->get(state, strings->then).value(state, promise);
    Value argument[1] = { Value(valueThunk) };

    return Object::call(state, then, promise, 1, argument);
}
}
