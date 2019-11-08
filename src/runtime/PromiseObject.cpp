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
#include "PromiseObject.h"
#include "runtime/Context.h"
#include "runtime/VMInstance.h"
#include "runtime/JobQueue.h"
#include "runtime/NativeFunctionObject.h"
#include "runtime/ArrayObject.h"
#include "runtime/JobQueue.h"
#include "runtime/SandBox.h"
#include "runtime/NativeFunctionObject.h"
#include "runtime/IteratorOperations.h"
#include "interpreter/ByteCodeInterpreter.h"

namespace Escargot {

PromiseObject::PromiseObject(ExecutionState& state)
    : Object(state)
    , m_state(PromiseState::Pending)
{
    Object::setPrototype(state, state.context()->globalObject()->promisePrototype());
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
    const StaticStrings* strings = &state.context()->staticStrings();

    Object* resolveFunction = new NativeFunctionObject(state, NativeFunctionInfo(strings->Empty, promiseResolveFunctions, 1, NativeFunctionInfo::Strict));
    Object* rejectFunction = new NativeFunctionObject(state, NativeFunctionInfo(strings->Empty, promiseRejectFunction, 1, NativeFunctionInfo::Strict));

    resolveFunction->deleteOwnProperty(state, strings->name);
    rejectFunction->deleteOwnProperty(state, strings->name);

    Object* resolveFunctionInternalSlot = new Object(state);
    Object* rejectFunctionInternalSlot = new Object(state);

    Object* alreadyResolved = new Object(state);
    alreadyResolved->defineOwnProperty(state, ObjectPropertyName(strings->value), ObjectPropertyDescriptor(Value(false), ObjectPropertyDescriptor::AllPresent));

    ObjectPropertyDescriptor::PresentAttribute attrPromise = (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::EnumerablePresent);
    ObjectPropertyDescriptor::PresentAttribute attrAlreadyResolved = (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::EnumerablePresent);
    resolveFunctionInternalSlot->defineOwnProperty(state, strings->Promise, ObjectPropertyDescriptor(Value(this), attrPromise));
    resolveFunctionInternalSlot->defineOwnProperty(state, strings->alreadyResolved, ObjectPropertyDescriptor(Value(alreadyResolved), attrAlreadyResolved));
    rejectFunctionInternalSlot->defineOwnProperty(state, strings->Promise, ObjectPropertyDescriptor(Value(this), attrPromise));
    rejectFunctionInternalSlot->defineOwnProperty(state, strings->alreadyResolved, ObjectPropertyDescriptor(Value(alreadyResolved), attrAlreadyResolved));

    resolveFunction->setInternalSlot(resolveFunctionInternalSlot);
    rejectFunction->setInternalSlot(rejectFunctionInternalSlot);

    return PromiseReaction::Capability(this, resolveFunction, rejectFunction);
}

PromiseReaction::Capability PromiseObject::newPromiseCapability(ExecutionState& state, Object* constructor)
{
    const StaticStrings* strings = &state.context()->staticStrings();

    if (!constructor->isConstructor())
        state.throwException(new TypeErrorObject(state, new ASCIIString("Constructor is not a function object")));

    // FIXME: Let executor be a new built-in function object as defined in GetCapabilitiesExecutor Functions (25.4.1.5.1).
    Object* executor = new NativeFunctionObject(state, NativeFunctionInfo(strings->Empty, getCapabilitiesExecutorFunction, 2, NativeFunctionInfo::Strict));
    Object* internalSlot = executor->ensureInternalSlot(state);

    Value arguments[] = { executor };
    Value promise = Object::construct(state, constructor, 1, arguments);
    ASSERT(internalSlot == executor->internalSlot());

    Value resolveFunction = internalSlot->get(state, strings->resolve).value(state, internalSlot);
    Value rejectFunction = internalSlot->get(state, strings->reject).value(state, internalSlot);

    if (!resolveFunction.isCallable() || !rejectFunction.isCallable())
        state.throwException(new TypeErrorObject(state, new ASCIIString("Promise resolve or reject function is not callable")));

    return PromiseReaction::Capability(promise, resolveFunction.asObject(), rejectFunction.asObject());
}

Object* PromiseObject::resolvingFunctionAlreadyResolved(ExecutionState& state, Object* callee)
{
    const StaticStrings* strings = &state.context()->staticStrings();
    Object* internalSlot = callee->internalSlot();
    Value alreadyResolved = internalSlot->get(state, strings->alreadyResolved).value(state, internalSlot);
    return alreadyResolved.asObject();
}

void PromiseObject::fulfill(ExecutionState& state, Value value)
{
    m_state = PromiseState::FulFilled;
    m_promiseResult = value;
    triggerPromiseReactions(state, m_fulfillReactions);
    m_fulfillReactions.clear();
    m_rejectReactions.clear();
}

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

PromiseObject* PromiseObject::then(ExecutionState& state, Value handler)
{
    return then(state, handler, Value(), newPromiseResultCapability(state)).value();
}

PromiseObject* PromiseObject::catchOperation(ExecutionState& state, Value handler)
{
    return then(state, Value(), handler, newPromiseResultCapability(state)).value();
}

Optional<PromiseObject*> PromiseObject::then(ExecutionState& state, Value onFulfilledValue, Value onRejectedValue, Optional<PromiseReaction::Capability> resultCapability)
{
    Object* onFulfilled = onFulfilledValue.isCallable() ? onFulfilledValue.asObject() : (Object*)(1);
    Object* onRejected = onRejectedValue.isCallable() ? onRejectedValue.asObject() : (Object*)(2);

    PromiseReaction::Capability capability = resultCapability.hasValue() ? resultCapability.value() : PromiseReaction::Capability(Value(Value::EmptyValue), nullptr, nullptr);

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
        return capability.m_promise.asPointerValue()->asPromiseObject();
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
Value promiseResolve(ExecutionState& state, Object* C, const Value& x)
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

// $25.4.1.5.1 Internal GetCapabilitiesExecutor Function
Value getCapabilitiesExecutorFunction(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    auto strings = &state.context()->staticStrings();
    Object* executor = state.resolveCallee();
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

// http://www.ecma-international.org/ecma-262/10.0/#sec-promise-resolve-functions
Value promiseResolveFunctions(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    auto strings = &state.context()->staticStrings();
    Object* callee = state.resolveCallee();
    Object* alreadyResolved = PromiseObject::resolvingFunctionAlreadyResolved(state, callee);
    Object* internalSlot = callee->internalSlot();
    PromiseObject* promise = internalSlot->getOwnProperty(state, strings->Promise).value(state, internalSlot).asObject()->asPromiseObject();
    if (alreadyResolved->getOwnProperty(state, strings->value).value(state, alreadyResolved).asBoolean())
        return Value();
    alreadyResolved->setThrowsException(state, strings->value, Value(true), alreadyResolved);

    Value resolutionValue = argv[0];
    if (resolutionValue == Value(promise)) {
        promise->reject(state, new TypeErrorObject(state, new ASCIIString("Self resolution error")));
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
        return Value();
    }

    return Value();
}

// $25.4.1.3.1 Internal Promise Reject Function
Value promiseRejectFunction(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    auto strings = &state.context()->staticStrings();
    Object* callee = state.resolveCallee();
    Object* alreadyResolved = PromiseObject::resolvingFunctionAlreadyResolved(state, callee);
    Object* internalSlot = callee->internalSlot();
    PromiseObject* promise = internalSlot->getOwnProperty(state, strings->Promise).value(state, internalSlot).asObject()->asPromiseObject();
    if (alreadyResolved->getOwnProperty(state, strings->value).value(state, alreadyResolved).asBoolean())
        return Value();
    alreadyResolved->setThrowsException(state, strings->value, Value(true), alreadyResolved);

    promise->reject(state, argv[0]);
    return Value();
}

// $25.4.4.1.2 Internal Promise.all Resolve Element Function
Value promiseAllResolveElementFunction(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    auto strings = &state.context()->staticStrings();
    Object* callee = state.resolveCallee();
    Value x = argv[0];
    Object* internalSlot = callee->internalSlot();

    Object* alreadyCalled = internalSlot->getOwnProperty(state, strings->alreadyCalled).value(state, internalSlot).asObject();
    if (alreadyCalled->getOwnProperty(state, strings->value).value(state, alreadyCalled).asBoolean())
        return Value();
    alreadyCalled->setThrowsException(state, strings->value, Value(true), alreadyCalled);

    uint32_t index = internalSlot->getOwnProperty(state, strings->index).value(state, internalSlot).asUInt32();
    ArrayObject* values = internalSlot->getOwnProperty(state, strings->values).value(state, internalSlot).asObject()->asArrayObject();
    Object* resolveFunction = internalSlot->getOwnProperty(state, strings->resolve).value(state, internalSlot).asObject();
    Object* remainingElementsCount = internalSlot->getOwnProperty(state, strings->remainingElements).value(state, internalSlot).asObject();

    values->setThrowsException(state, ObjectPropertyName(state, Value(index)), x, values);
    uint32_t remainingElements = remainingElementsCount->getOwnProperty(state, strings->value).value(state, remainingElementsCount).asUInt32();
    remainingElementsCount->setThrowsException(state, strings->value, Value(remainingElements - 1), remainingElementsCount);
    if (remainingElements == 1) {
        Value arguments[] = { values };
        Object::call(state, resolveFunction, Value(), 1, arguments);
    }
    return Value();
}
}
